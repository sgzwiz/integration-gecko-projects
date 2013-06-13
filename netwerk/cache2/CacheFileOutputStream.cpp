/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheFileOutputStream.h"

#include "CacheFile.h"
#include "nsStreamUtils.h"
#include "nsThreadUtils.h"
#include <algorithm>

namespace mozilla {
namespace net {

NS_IMPL_THREADSAFE_ADDREF(CacheFileOutputStream)
NS_IMETHODIMP_(nsrefcnt)
CacheFileOutputStream::Release()
{
  nsrefcnt count;
  NS_PRECONDITION(0 != mRefCnt, "dup release");
  count = NS_AtomicDecrementRefcnt(mRefCnt);
  NS_LOG_RELEASE(this, count, "CacheFileOutputStream");

  if (0 == count) {
    mRefCnt = 1;
    delete (this);
    return 0;
  }

  if (count == 1) {
    mFile->RemoveOutput(this);
    mFile = nullptr;
  }

  return count;
}

NS_INTERFACE_MAP_BEGIN(CacheFileOutputStream)
  NS_INTERFACE_MAP_ENTRY(nsIOutputStream)
  NS_INTERFACE_MAP_ENTRY(nsIAsyncOutputStream)
  NS_INTERFACE_MAP_ENTRY(nsISeekableStream)
  NS_INTERFACE_MAP_ENTRY(mozilla::net::CacheFileChunkListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIOutputStream)
NS_INTERFACE_MAP_END_THREADSAFE

CacheFileOutputStream::CacheFileOutputStream(CacheFile *aFile)
  : mFile(aFile)
  , mPos(0)
  , mClosed(false)
  , mStatus(NS_OK)
  , mCallbackFlags(0)
{
  MOZ_COUNT_CTOR(CacheFileOutputStream);
}

CacheFileOutputStream::~CacheFileOutputStream()
{
  MOZ_COUNT_DTOR(CacheFileOutputStream);
}

// nsIOutputStream
NS_IMETHODIMP
CacheFileOutputStream::Close()
{
  return CloseWithStatus(NS_OK);
}

NS_IMETHODIMP
CacheFileOutputStream::Flush()
{
  // TODO do we need to implement flush ???
  return NS_OK;
}

NS_IMETHODIMP
CacheFileOutputStream::Write(const char * aBuf, uint32_t aCount,
                             uint32_t *_retval)
{
  MutexAutoLock lock(*mFile->GetLock());

  if (mClosed)
    return mStatus;

  EnsureCorrectChunk(true);
  if (!mChunk)
    return NS_BASE_STREAM_WOULD_BLOCK;

  FillHole();

  int64_t canWrite;
  char *buf;
  CanWrite(&canWrite, &buf);
  MOZ_ASSERT(canWrite > 0);

  *_retval = std::min(static_cast<uint32_t>(canWrite), aCount);
  memcpy(buf, aBuf, *_retval);
  mPos += *_retval;

  mChunk->UpdateDataSize(mPos - mChunk->Index() * kChunkSize, false);

  EnsureCorrectChunk(canWrite < aCount && mPos % kChunkSize);

  return NS_OK;
}

NS_IMETHODIMP
CacheFileOutputStream::WriteFrom(nsIInputStream *aFromStream, uint32_t aCount,
                                 uint32_t *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CacheFileOutputStream::WriteSegments(nsReadSegmentFun aReader, void *aClosure,
                                     uint32_t aCount, uint32_t *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CacheFileOutputStream::IsNonBlocking(bool *_retval)
{
  *_retval = true;
  return NS_OK;
}

// nsIAsyncOutputStream
NS_IMETHODIMP
CacheFileOutputStream::CloseWithStatus(nsresult aStatus)
{
  MutexAutoLock lock(*mFile->GetLock());

  if (mClosed) {
    MOZ_ASSERT(!mCallback);
    return NS_OK;
  }

  mClosed = true;
  mStatus = NS_FAILED(aStatus) ? aStatus : NS_BASE_STREAM_CLOSED;

  ReleaseChunk();

  if (mCallback)
    NotifyListener();

  return NS_OK;
}

NS_IMETHODIMP
CacheFileOutputStream::AsyncWait(nsIOutputStreamCallback *aCallback,
                                 uint32_t aFlags,
                                 uint32_t aRequestedCount,
                                 nsIEventTarget *aEventTarget)
{
  MutexAutoLock lock(*mFile->GetLock());

  mCallback = aCallback;
  mCallbackFlags = aFlags;

  if (!mCallback)
    return NS_OK;

  if (mClosed) {
    NotifyListener();
    return NS_OK;
  }

  EnsureCorrectChunk(true);

  if (!mChunk) {
    // wait for OnChunkAvailable
    return NS_OK;
  }

#ifdef DEBUG
  int64_t canWrite;
  char *buf;
  CanWrite(&canWrite, &buf);
  MOZ_ASSERT(canWrite > 0);
#endif

  NotifyListener();
  return NS_OK;
}

// nsISeekableStream
NS_IMETHODIMP
CacheFileOutputStream::Seek(int32_t whence, int64_t offset)
{
  MutexAutoLock lock(*mFile->GetLock());

  if (mClosed)
    return NS_ERROR_NOT_AVAILABLE;

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
CacheFileOutputStream::Tell(int64_t *_retval)
{
  MutexAutoLock lock(*mFile->GetLock());

  if (mClosed)
    return NS_ERROR_NOT_AVAILABLE;

  *_retval = mPos;
  return NS_OK;
}

NS_IMETHODIMP
CacheFileOutputStream::SetEOF()
{
  MOZ_ASSERT(false, "Will be implemented later...");
  return NS_ERROR_NOT_IMPLEMENTED;
}

// CacheFileChunkListener
nsresult
CacheFileOutputStream::OnChunkRead(nsresult aResult, CacheFileChunk *aChunk)
{
  MOZ_NOT_REACHED("CacheFileOutputStream::OnChunkRead should not be called!");
  return NS_ERROR_UNEXPECTED;
}

nsresult
CacheFileOutputStream::OnChunkWritten(nsresult aResult, CacheFileChunk *aChunk)
{
  MOZ_NOT_REACHED(
    "CacheFileOutputStream::OnChunkWritten should not be called!");
  return NS_ERROR_UNEXPECTED;
}

nsresult
CacheFileOutputStream::OnChunkAvailable(nsresult aResult,
                                        CacheFileChunk *aChunk)
{
  MutexAutoLock lock(*mFile->GetLock());

  MOZ_ASSERT(!mChunk);

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
CacheFileOutputStream::OnChunkUpdated(CacheFileChunk *aChunk)
{
  MOZ_NOT_REACHED(
    "CacheFileOutputStream::OnChunkUpdated should not be called!");
  return NS_ERROR_UNEXPECTED;
}

void
CacheFileOutputStream::ReleaseChunk()
{
  mFile->AssertOwnsLock();
  mChunk = nullptr;
}

void
CacheFileOutputStream::EnsureCorrectChunk(bool aReleaseOnly)
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

  if (aReleaseOnly)
    return;

  rv = mFile->GetChunkLocked(mPos / kChunkSize, true, this);
  MOZ_ASSERT(NS_SUCCEEDED(rv),
             "GetChunkLocked should always fail asynchronously");
  if (NS_FAILED(rv)) {
    // LOG ???
  }
}

void
CacheFileOutputStream::CanWrite(int64_t *aCanWrite, char **aBuf)
{
  mFile->AssertOwnsLock();

  MOZ_ASSERT(mChunk);
  MOZ_ASSERT(mPos / kChunkSize == mChunk->Index());

  uint32_t chunkOffset = mPos - (mPos / kChunkSize) * kChunkSize;
  *aCanWrite = kChunkSize - chunkOffset;
  *aBuf = mChunk->Buf() + chunkOffset;
}

void
CacheFileOutputStream::FillHole()
{
  mFile->AssertOwnsLock();

  MOZ_ASSERT(mChunk);
  MOZ_ASSERT(mPos / kChunkSize == mChunk->Index());

  uint32_t pos = mPos - (mPos / kChunkSize) * kChunkSize;
  if (mChunk->DataSize() >= pos)
    return;

  memset(mChunk->Buf() + mChunk->DataSize(), 0, pos - mChunk->DataSize());

  mChunk->UpdateDataSize(pos, false);
}

void
CacheFileOutputStream::NotifyListener()
{
  mFile->AssertOwnsLock();

  MOZ_ASSERT(mCallback);

  if (!mCallbackTarget)
    mCallbackTarget = NS_GetCurrentThread();

  nsCOMPtr<nsIOutputStreamCallback> asyncCallback =
    NS_NewOutputStreamReadyEvent(mCallback, mCallbackTarget);

  mCallback = nullptr;
  mCallbackTarget = nullptr;

  asyncCallback->OnOutputStreamReady(this);
}

} // net
} // mozilla
