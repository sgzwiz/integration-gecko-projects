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
    mFile = nullptr;
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
  // TODO what to return when we have an output stream?
  *_retval = mFile->DataSize() - mPos;
  return NS_OK;
}

NS_IMETHODIMP
CacheFileInputStream::Read(char *aBuf, uint32_t aCount, uint32_t *_retval)
{
  MutexAutoLock lock(*mFile->GetLock());

  nsresult rv;

  if (mClosed)
    return mStatus;

  EnsureCorrectChunk(true);
  if (!mChunk)
    return NS_BASE_STREAM_WOULD_BLOCK;

  int64_t canRead;
  const char *buf;
  CanRead(&canRead, &buf);

  if (canRead < 0) {
    // file was truncated ???
    // TODO what to return?
    *_retval = 0;
    rv = NS_BASE_STREAM_CLOSED;
  }
  else if (canRead > 0) {
    *_retval = std::min(static_cast<uint32_t>(canRead), aCount);
    memcpy(aBuf, buf, *_retval);
    mPos += *_retval;

    EnsureCorrectChunk(canRead < aCount && mPos % kChunkSize);

    rv = NS_OK;
  }
  else {
    if (mFile->mOutput)
      rv = NS_BASE_STREAM_WOULD_BLOCK;
    else {
      *_retval = 0;
      mClosed = true;
      mStatus = NS_OK;
      rv = NS_OK;
    }
  }

  return rv;
}

NS_IMETHODIMP
CacheFileInputStream::ReadSegments(nsWriteSegmentFun aWriter, void *aClosure,
                                   uint32_t aCount, uint32_t *_retval)
{
  MutexAutoLock lock(*mFile->GetLock());

  nsresult rv;

  if (mClosed)
    return mStatus;

  EnsureCorrectChunk(true);
  if (!mChunk)
    return NS_BASE_STREAM_WOULD_BLOCK;

  int64_t canRead;
  const char *buf;
  CanRead(&canRead, &buf);

  if (canRead < 0) {
    // file was truncated ???
    // TODO what to return?
    *_retval = 0;
    rv = NS_BASE_STREAM_CLOSED;
  }
  else if (canRead > 0) {
    uint32_t toRead = std::min(static_cast<uint32_t>(canRead), aCount);
    rv = aWriter(this, aClosure, buf, 0, toRead, _retval);
    if (NS_SUCCEEDED(rv)) {
      MOZ_ASSERT(*_retval <= toRead,
                 "writer should not write more than we asked it to write");
      mPos += *_retval;
    }

    EnsureCorrectChunk(canRead < aCount && mPos % kChunkSize);

    rv = NS_OK;
  }
  else {
    if (mFile->mOutput)
      rv = NS_BASE_STREAM_WOULD_BLOCK;
    else {
      *_retval = 0;
      mClosed = true;
      mStatus = NS_OK;
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
  MutexAutoLock lock(*mFile->GetLock());

  if (mClosed) {
    MOZ_ASSERT(!mCallback);
    return NS_OK;
  }

  mClosed = true;
  mStatus = NS_FAILED(aStatus) ? aStatus : NS_BASE_STREAM_CLOSED;

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
  MutexAutoLock lock(*mFile->GetLock());

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

  EnsureCorrectChunk(true);

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
  MutexAutoLock lock(*mFile->GetLock());

  int64_t newPos = offset;
  switch (whence) {
    case NS_SEEK_SET:
      break;
    case NS_SEEK_CUR:
      newPos += mPos;
      break;
    case NS_SEEK_END:
      newPos += mFile->DataSize();
      break;
    default:
      NS_ERROR("invalid whence");
      return NS_ERROR_INVALID_ARG;
  }
  mPos = newPos;
  EnsureCorrectChunk(false);

  return NS_OK;
}

NS_IMETHODIMP
CacheFileInputStream::Tell(int64_t *_retval)
{
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
CacheFileInputStream::OnChunkAvailable(nsresult aResult, CacheFileChunk *aChunk)
{
  MutexAutoLock lock(*mFile->GetLock());

  MOZ_ASSERT(!mChunk);
  MOZ_ASSERT(!mWaitingForUpdate);

  mChunk = aChunk;

  if (mCallback)
    NotifyListener();

  return NS_OK;
}

nsresult
CacheFileInputStream::OnChunkUpdated(CacheFileChunk *aChunk)
{
  MutexAutoLock lock(*mFile->GetLock());

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

  mChunk = nullptr;
}

void
CacheFileInputStream::EnsureCorrectChunk(bool aReleaseOnly)
{
  mFile->AssertOwnsLock();

  nsresult rv;

  if (mChunk) {
    if (mChunk->Index() == mPos / kChunkSize) {
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

  rv = mFile->GetChunkLocked(mPos / kChunkSize, false, this);
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
