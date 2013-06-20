/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheFileInputStream.h"

#include "CacheFile.h"
#include "nsStreamUtils.h"
#include "nsThreadUtils.h"
#include <algorithm>

namespace mozilla {
namespace net {

NS_IMPL_THREADSAFE_ADDREF(CacheFileInputStream)
NS_IMETHODIMP_(nsrefcnt)
CacheFileInputStream::Release()
{
  nsrefcnt count;
  NS_PRECONDITION(0 != mRefCnt, "dup release");
  count = NS_AtomicDecrementRefcnt(mRefCnt);
  NS_LOG_RELEASE(this, count, "CacheFileInputStream");

  if (0 == count) {
    mRefCnt = 1;
    delete (this);
    return 0;
  }

  if (count == 1) {
    mFile->RemoveInput(this);
  }

  return count;
}

NS_INTERFACE_MAP_BEGIN(CacheFileInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIAsyncInputStream)
  NS_INTERFACE_MAP_ENTRY(nsISeekableStream)
  NS_INTERFACE_MAP_ENTRY(mozilla::net::CacheFileChunkListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInputStream)
NS_INTERFACE_MAP_END_THREADSAFE

CacheFileInputStream::CacheFileInputStream(CacheFile *aFile)
  : mFile(aFile)
  , mPos(0)
  , mClosed(false)
  , mStatus(NS_OK)
  , mWaitingForUpdate(false)
  , mListeningForChunk(-1)
  , mCallbackFlags(0)
{
  MOZ_COUNT_CTOR(CacheFileInputStream);
}

CacheFileInputStream::~CacheFileInputStream()
{
  MOZ_COUNT_DTOR(CacheFileInputStream);
}

// nsIInputStream
NS_IMETHODIMP
CacheFileInputStream::Close()
{
  return CloseWithStatus(NS_OK);
}

NS_IMETHODIMP
CacheFileInputStream::Available(uint64_t *_retval)
{
  CacheFileAutoLock lock(mFile);

  if (mClosed)
    return NS_FAILED(mStatus) ? mStatus : NS_BASE_STREAM_CLOSED;

  EnsureCorrectChunk(false);
  *_retval = 0;

  if (!mChunk)
    return NS_OK;

  int64_t canRead;
  const char *buf;
  CanRead(&canRead, &buf);

  if (canRead > 0)
    *_retval = canRead;

  return NS_OK;
}

NS_IMETHODIMP
CacheFileInputStream::Read(char *aBuf, uint32_t aCount, uint32_t *_retval)
{
  CacheFileAutoLock lock(mFile);

  nsresult rv;

  if (mClosed) {
    if NS_FAILED(mStatus)
      return mStatus;

    *_retval = 0;
    return NS_OK;
  }

  EnsureCorrectChunk(false);
  if (!mChunk)
    return NS_BASE_STREAM_WOULD_BLOCK;

  int64_t canRead;
  const char *buf;
  CanRead(&canRead, &buf);

  if (canRead < 0) {
    // file was truncated ???
    // TODO what to return?
    *_retval = 0;
    rv = NS_OK;
  }
  else if (canRead > 0) {
    *_retval = std::min(static_cast<uint32_t>(canRead), aCount);
    memcpy(aBuf, buf, *_retval);
    mPos += *_retval;

    EnsureCorrectChunk(!(canRead < aCount && mPos % kChunkSize == 0));

    rv = NS_OK;
  }
  else {
    if (mFile->mOutput)
      rv = NS_BASE_STREAM_WOULD_BLOCK;
    else {
      *_retval = 0;
      rv = NS_OK;
    }
  }

  return rv;
}

NS_IMETHODIMP
CacheFileInputStream::ReadSegments(nsWriteSegmentFun aWriter, void *aClosure,
                                   uint32_t aCount, uint32_t *_retval)
{
  CacheFileAutoLock lock(mFile);

  nsresult rv;

  if (mClosed) {
    if NS_FAILED(mStatus)
      return mStatus;

    *_retval = 0;
    return NS_OK;
  }

  EnsureCorrectChunk(false);
  if (!mChunk)
    return NS_BASE_STREAM_WOULD_BLOCK;

  int64_t canRead;
  const char *buf;
  CanRead(&canRead, &buf);

  if (canRead < 0) {
    // file was truncated ???
    // TODO what to return?
    *_retval = 0;
    rv = NS_OK;
  }
  else if (canRead > 0) {
    uint32_t toRead = std::min(static_cast<uint32_t>(canRead), aCount);
    rv = aWriter(this, aClosure, buf, 0, toRead, _retval);
    if (NS_SUCCEEDED(rv)) {
      MOZ_ASSERT(*_retval <= toRead,
                 "writer should not write more than we asked it to write");
      mPos += *_retval;
    }

    EnsureCorrectChunk(!(canRead < aCount && mPos % kChunkSize == 0));

    rv = NS_OK;
  }
  else {
    if (mFile->mOutput)
      rv = NS_BASE_STREAM_WOULD_BLOCK;
    else {
      *_retval = 0;
      rv = NS_OK;
    }
  }

  return rv;
}

NS_IMETHODIMP
CacheFileInputStream::IsNonBlocking(bool *_retval)
{
  *_retval = true;
  return NS_OK;
}

// nsIAsyncInputStream
NS_IMETHODIMP
CacheFileInputStream::CloseWithStatus(nsresult aStatus)
{
  CacheFileAutoLock lock(mFile);

  if (mClosed) {
    MOZ_ASSERT(!mCallback);
    return NS_OK;
  }

  mClosed = true;
  mStatus = NS_FAILED(aStatus) ? aStatus : NS_BASE_STREAM_CLOSED;

  if (mChunk)
    ReleaseChunk();

  // TODO propagate error from input stream to other streams ???

  if (mCallback)
    NotifyListener();

  return NS_OK;
}

NS_IMETHODIMP
CacheFileInputStream::AsyncWait(nsIInputStreamCallback *aCallback,
                                uint32_t aFlags,
                                uint32_t aRequestedCount,
                                nsIEventTarget *aEventTarget)
{
  CacheFileAutoLock lock(mFile);

  mCallback = aCallback;
  mCallbackFlags = aFlags;

  if (!mCallback && mWaitingForUpdate) {
    mChunk->CancelWait(this);
    mWaitingForUpdate = false;
    return NS_OK;
  }

  if (mClosed) {
    NotifyListener();
    return NS_OK;
  }

  EnsureCorrectChunk(false);

  if (!mChunk || mWaitingForUpdate) {
    // wait for OnChunkAvailable or OnChunkUpdated
    return NS_OK;
  }

  int64_t canRead;
  const char *buf;
  CanRead(&canRead, &buf);

  if (canRead != 0 || !mFile->mOutput) {
    NotifyListener();
    return NS_OK;
  }

  mChunk->WaitForUpdate(this);
  mWaitingForUpdate = true;

  return NS_OK;
}

// nsISeekableStream
NS_IMETHODIMP
CacheFileInputStream::Seek(int32_t whence, int64_t offset)
{
  CacheFileAutoLock lock(mFile);

  if (mClosed)
    return NS_BASE_STREAM_CLOSED;

  int64_t newPos = offset;
  switch (whence) {
    case NS_SEEK_SET:
      break;
    case NS_SEEK_CUR:
      newPos += mPos;
      break;
    case NS_SEEK_END:
      newPos += mFile->mDataSize;
      break;
    default:
      NS_ERROR("invalid whence");
      return NS_ERROR_INVALID_ARG;
  }
  mPos = newPos;
  EnsureCorrectChunk(true);

  return NS_OK;
}

NS_IMETHODIMP
CacheFileInputStream::Tell(int64_t *_retval)
{
  CacheFileAutoLock lock(mFile);

  if (mClosed)
    return NS_BASE_STREAM_CLOSED;

  *_retval = mPos;
  return NS_OK;
}

NS_IMETHODIMP
CacheFileInputStream::SetEOF()
{
  MOZ_ASSERT(false, "Don't call SetEOF on cache input stream");
  return NS_ERROR_NOT_IMPLEMENTED;
}

// CacheFileChunkListener
nsresult
CacheFileInputStream::OnChunkRead(nsresult aResult, CacheFileChunk *aChunk)
{
  MOZ_NOT_REACHED("CacheFileInputStream::OnChunkRead should not be called!");
  return NS_ERROR_UNEXPECTED;
}

nsresult
CacheFileInputStream::OnChunkWritten(nsresult aResult, CacheFileChunk *aChunk)
{
  MOZ_NOT_REACHED("CacheFileInputStream::OnChunkWritten should not be called!");
  return NS_ERROR_UNEXPECTED;
}

nsresult
CacheFileInputStream::OnChunkAvailable(nsresult aResult, uint32_t aChunkIdx,
                                       CacheFileChunk *aChunk)
{
  CacheFileAutoLock lock(mFile);

  MOZ_ASSERT(mListeningForChunk != -1);

  if (mListeningForChunk != static_cast<int64_t>(aChunkIdx)) {
    // This is not a chunk that we're waiting for
    return NS_OK;
  }

  MOZ_ASSERT(!mChunk);
  MOZ_ASSERT(!mWaitingForUpdate);
  mListeningForChunk = -1;

  if (mClosed) {
    MOZ_ASSERT(!mCallback);
    return NS_OK;
  }

  mChunk = aChunk;

  if (mCallback)
    NotifyListener();

  return NS_OK;
}

nsresult
CacheFileInputStream::OnChunkUpdated(CacheFileChunk *aChunk)
{
  CacheFileAutoLock lock(mFile);

  if (!mWaitingForUpdate) {
    // LOG
  }
  else {
    mWaitingForUpdate = false;
  }

  if (mCallback)
    NotifyListener();

  return NS_OK;
}

void
CacheFileInputStream::ReleaseChunk()
{
  mFile->AssertOwnsLock();

  if (mWaitingForUpdate) {
    mChunk->CancelWait(this);
    mWaitingForUpdate = false;
  }

  mFile->ReleaseOutsideLock(mChunk.forget().get());
}

void
CacheFileInputStream::EnsureCorrectChunk(bool aReleaseOnly)
{
  mFile->AssertOwnsLock();

  nsresult rv;

  uint32_t chunkIdx = mPos / kChunkSize;

  if (mChunk) {
    if (mChunk->Index() == chunkIdx) {
      // we have a correct chunk
      return;
    }
    else {
      ReleaseChunk();
    }
  }

  MOZ_ASSERT(!mWaitingForUpdate);

  if (aReleaseOnly)
    return;

  if (mListeningForChunk == static_cast<int64_t>(chunkIdx)) {
    // We're already waiting for this chunk
    return;
  }

  mListeningForChunk = static_cast<int64_t>(chunkIdx);

  rv = mFile->GetChunkLocked(chunkIdx, false, this);
  MOZ_ASSERT(NS_SUCCEEDED(rv),
             "GetChunkLocked should always fail asynchronously");
  if (NS_FAILED(rv)) {
    // LOG ???
  }
}

void
CacheFileInputStream::CanRead(int64_t *aCanRead, const char **aBuf)
{
  mFile->AssertOwnsLock();

  MOZ_ASSERT(mChunk);
  MOZ_ASSERT(mPos / kChunkSize == mChunk->Index());

  uint32_t chunkOffset = mPos - (mPos / kChunkSize) * kChunkSize;
  *aCanRead = mChunk->DataSize() - chunkOffset;
  *aBuf = mChunk->Buf() + chunkOffset;
}

void
CacheFileInputStream::NotifyListener()
{
  mFile->AssertOwnsLock();

  MOZ_ASSERT(mCallback);

  if (!mCallbackTarget)
    mCallbackTarget = NS_GetCurrentThread();

  nsCOMPtr<nsIInputStreamCallback> asyncCallback =
    NS_NewInputStreamReadyEvent(mCallback, mCallbackTarget);

  mCallback = nullptr;
  mCallbackTarget = nullptr;

  asyncCallback->OnInputStreamReady(this);
}

} // net
} // mozilla
