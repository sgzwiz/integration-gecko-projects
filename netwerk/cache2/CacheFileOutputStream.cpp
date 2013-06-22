/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheFileOutputStream.h"

#include "CacheLog.h"
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
  , mListeningForChunk(-1)
  , mCallbackFlags(0)
{
  LOG(("CacheFileOutputStream::CacheFileOutputStream() [this=%p]", this));
  MOZ_COUNT_CTOR(CacheFileOutputStream);
}

CacheFileOutputStream::~CacheFileOutputStream()
{
  LOG(("CacheFileOutputStream::~CacheFileOutputStream() [this=%p]", this));
  MOZ_COUNT_DTOR(CacheFileOutputStream);
}

// nsIOutputStream
NS_IMETHODIMP
CacheFileOutputStream::Close()
{
  LOG(("CacheFileOutputStream::Close() [this=%p]", this));
  return CloseWithStatus(NS_OK);
}

NS_IMETHODIMP
CacheFileOutputStream::Flush()
{
  // TODO do we need to implement flush ???
  LOG(("CacheFileOutputStream::Flush() [this=%p]", this));
  return NS_OK;
}

NS_IMETHODIMP
CacheFileOutputStream::Write(const char * aBuf, uint32_t aCount,
                             uint32_t *_retval)
{
  CacheFileAutoLock lock(mFile);

  LOG(("CacheFileOutputStream::Write() [this=%p, count=%d]", this, aCount));

  if (mClosed) {
    LOG(("CacheFileOutputStream::Write() - Stream is closed. [this=%p, "
         "status=0x%08x]", this, mStatus));

    return NS_FAILED(mStatus) ? mStatus : NS_BASE_STREAM_CLOSED;
  }

  EnsureCorrectChunk(false);
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

  EnsureCorrectChunk(!(canWrite < aCount && mPos % kChunkSize == 0));

  LOG(("CacheFileOutputStream::Write() - Wrote %d bytes [this=%p]",
       *_retval, this));

  return NS_OK;
}

NS_IMETHODIMP
CacheFileOutputStream::WriteFrom(nsIInputStream *aFromStream, uint32_t aCount,
                                 uint32_t *_retval)
{
  LOG(("CacheFileOutputStream::WriteFrom() - NOT_IMPLEMENTED [this=%p, from=%p"
       ", count=%d]", this, aFromStream, aCount));

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CacheFileOutputStream::WriteSegments(nsReadSegmentFun aReader, void *aClosure,
                                     uint32_t aCount, uint32_t *_retval)
{
  LOG(("CacheFileOutputStream::WriteSegments() - NOT_IMPLEMENTED [this=%p, "
       "count=%d]", this, aCount));

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
  CacheFileAutoLock lock(mFile);

  LOG(("CacheFileOutputStream::CloseWithStatus() [this=%p, aStatus=0x%08x]",
       this, aStatus));

  if (mClosed) {
    MOZ_ASSERT(!mCallback);
    return NS_OK;
  }

  mClosed = true;
  mStatus = NS_FAILED(aStatus) ? aStatus : NS_BASE_STREAM_CLOSED;

  if (mChunk)
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
  CacheFileAutoLock lock(mFile);

  LOG(("CacheFileOutputStream::AsyncWait() [this=%p, callback=%p, flags=%d, "
       "requestedCount=%d, eventTarget=%p]", this, aCallback, aFlags,
       aRequestedCount, aEventTarget));

  mCallback = aCallback;
  mCallbackFlags = aFlags;

  if (!mCallback)
    return NS_OK;

  if (mClosed) {
    NotifyListener();
    return NS_OK;
  }

  EnsureCorrectChunk(false);

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
  CacheFileAutoLock lock(mFile);

  LOG(("CacheFileOutputStream::Seek() [this=%p, whence=%d, offset=%lld]",
       this, whence, offset));

  if (mClosed) {
    LOG(("CacheFileOutputStream::Seek() - Stream is closed. [this=%p]", this));
    return NS_BASE_STREAM_CLOSED;
  }

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

  LOG(("CacheFileOutputStream::Seek() [this=%p, pos=%lld]", this, mPos));
  return NS_OK;
}

NS_IMETHODIMP
CacheFileOutputStream::Tell(int64_t *_retval)
{
  CacheFileAutoLock lock(mFile);

  if (mClosed) {
    LOG(("CacheFileOutputStream::Tell() - Stream is closed. [this=%p]", this));
    return NS_BASE_STREAM_CLOSED;
  }

  *_retval = mPos;

  LOG(("CacheFileOutputStream::Tell() [this=%p, retval=%lld]", this, *_retval));
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
                                        uint32_t aChunkIdx,
                                        CacheFileChunk *aChunk)
{
  CacheFileAutoLock lock(mFile);

  LOG(("CacheFileOutputStream::OnChunkAvailable() [this=%p, idx=%d, chunk=%p]",
       this, aChunkIdx, aChunk));

  MOZ_ASSERT(mListeningForChunk != -1);

  if (mListeningForChunk != static_cast<int64_t>(aChunkIdx)) {
    // This is not a chunk that we're waiting for
    LOG(("CacheFileOutputStream::OnChunkAvailable() - Notification is for a "
         "different chunk. [this=%p, listeningForChunk=%lld]",
         this, mListeningForChunk));

    return NS_OK;
  }

  MOZ_ASSERT(!mChunk);
  mListeningForChunk = -1;

  if (mClosed) {
    MOZ_ASSERT(!mCallback);

    LOG(("CacheFileOutputStream::OnChunkAvailable() - Stream is closed, "
         "ignoring notification. [this=%p]", this));

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
  LOG(("CacheFileOutputStream::ReleaseChunk() [this=%p, idx=%d]",
       this, mChunk->Index()));

  mFile->ReleaseOutsideLock(mChunk.forget().get());
}

void
CacheFileOutputStream::EnsureCorrectChunk(bool aReleaseOnly)
{
  mFile->AssertOwnsLock();

  LOG(("CacheFileOutputStream::EnsureCorrectChunk() [this=%p, releaseOnly=%d]",
       this, aReleaseOnly));

  nsresult rv;

  uint32_t chunkIdx = mPos / kChunkSize;

  if (mChunk) {
    if (mChunk->Index() == chunkIdx) {
      // we have a correct chunk
      LOG(("CacheFileOutputStream::EnsureCorrectChunk() - Have correct chunk "
           "[this=%p, idx=%d]", this, chunkIdx));

      return;
    }
    else {
      ReleaseChunk();
    }
  }

  if (aReleaseOnly)
    return;

  if (mListeningForChunk == static_cast<int64_t>(chunkIdx)) {
    // We're already waiting for this chunk
    LOG(("CacheFileOutputStream::EnsureCorrectChunk() - Already listening for "
         "chunk %lld [this=%p]", mListeningForChunk, this));

    return;
  }

  mListeningForChunk = static_cast<int64_t>(chunkIdx);

  rv = mFile->GetChunkLocked(chunkIdx, true, this);
  MOZ_ASSERT(NS_SUCCEEDED(rv),
             "GetChunkLocked should always fail asynchronously");
  if (NS_FAILED(rv)) {
    LOG(("CacheFileOutputStream::EnsureCorrectChunk() - GetChunkLocked failed "
         " synchronously! [this=%p, idx=%d, rv=0x%08x]", this, chunkIdx, rv));
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

  LOG(("CacheFileOutputStream::CanWrite() [this=%p, canWrite=%lld]",
       this, *aCanWrite));
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

  LOG(("CacheFileOutputStream::FillHole() - Zeroing hole in chunk %d, range "
       "%d-%d [this=%p]", mChunk->Index(), mChunk->DataSize(), pos, this));

  memset(mChunk->Buf() + mChunk->DataSize(), 0, pos - mChunk->DataSize());

  mChunk->UpdateDataSize(pos, false);
}

void
CacheFileOutputStream::NotifyListener()
{
  mFile->AssertOwnsLock();

  LOG(("CacheFileOutputStream::NotifyListener() [this=%p]", this));

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
