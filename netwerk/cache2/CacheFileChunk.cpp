/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheFileChunk.h"

#include "CacheLog.h"
#include "CacheFile.h"
#include "nsThreadUtils.h"
#include "nsAlgorithm.h"
#include <algorithm>

namespace mozilla {
namespace net {

class NotifyUpdateListenerEvent : public nsRunnable {
public:
  NotifyUpdateListenerEvent(CacheFileChunkListener *aCallback,
                            CacheFileChunk *aChunk)
    : mCallback(aCallback)
    , mChunk(aChunk)
  {
    LOG(("NotifyUpdateListenerEvent::NotifyUpdateListenerEvent() [this=%p]",
         this));
    MOZ_COUNT_CTOR(NotifyUpdateListenerEvent);
  }

  ~NotifyUpdateListenerEvent()
  {
    LOG(("NotifyUpdateListenerEvent::~NotifyUpdateListenerEvent() [this=%p]",
         this));
    MOZ_COUNT_DTOR(NotifyUpdateListenerEvent);
  }

  NS_IMETHOD Run()
  {
    LOG(("NotifyUpdateListenerEvent::Run() [this=%p]", this));

    mCallback->OnChunkUpdated(mChunk);
    return NS_OK;
  }

protected:
  nsCOMPtr<CacheFileChunkListener> mCallback;
  nsRefPtr<CacheFileChunk>         mChunk;
};


class ValidityPair {
public:
  ValidityPair(uint32_t aOffset, uint32_t aLen)
    : mOffset(aOffset), mLen(aLen)
  {}

  ValidityPair& operator=(const ValidityPair& aOther) {
    mOffset = aOther.mOffset;
    mLen = aOther.mLen;
    return *this;
  }

  bool Overlaps(const ValidityPair& aOther) const {
    if ((mOffset <= aOther.mOffset && mOffset + mLen >= aOther.mOffset) ||
        (aOther.mOffset <= mOffset && aOther.mOffset + mLen >= mOffset))
      return true;

    return false;
  }

  bool LessThan(const ValidityPair& aOther) const {
    if (mOffset < aOther.mOffset)
      return true;

    if (mOffset == aOther.mOffset && mLen < aOther.mLen)
      return true;

    return false;
  }

  void Merge(const ValidityPair& aOther) {
    MOZ_ASSERT(Overlaps(aOther));

    uint32_t offset = std::min(mOffset, aOther.mOffset);
    uint32_t end = std::max(mOffset + mLen, aOther.mOffset + aOther.mLen);

    mOffset = offset;
    mLen = end - offset;
  }

  uint32_t Offset() { return mOffset; }
  uint32_t Len()    { return mLen; }

private:
  uint32_t mOffset;
  uint32_t mLen;
};


NS_IMPL_THREADSAFE_ADDREF(CacheFileChunk)
NS_IMETHODIMP_(nsrefcnt)
CacheFileChunk::Release()
{
  nsrefcnt count;
  NS_PRECONDITION(0 != mRefCnt, "dup release");
  count = NS_AtomicDecrementRefcnt(mRefCnt);
  NS_LOG_RELEASE(this, count, "CacheFileChunk");

  if (0 == count) {
    mRefCnt = 1;
    delete (this);
    return 0;
  }

  if (!mRemovingChunk && count == 1) {
    mFile->RemoveChunk(this);
  }

  return count;
}

NS_INTERFACE_MAP_BEGIN(CacheFileChunk)
  NS_INTERFACE_MAP_ENTRY(mozilla::net::CacheFileIOListener)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END_THREADSAFE

CacheFileChunk::CacheFileChunk(CacheFile *aFile, uint32_t aIndex)
  : mIndex(aIndex)
  , mIsReady(false)
  , mIsDirty(false)
  , mRemovingChunk(false)
  , mDataSize(0)
  , mBuf(nullptr)
  , mBufSize(0)
  , mReadBuf(nullptr)
  , mReadBufSize(0)
  , mReadHash(0)
  , mFile(aFile)
{
  LOG(("CacheFileChunk::CacheFileChunk() [this=%p]", this));
  MOZ_COUNT_CTOR(CacheFileChunk);
}

CacheFileChunk::~CacheFileChunk()
{
  LOG(("CacheFileChunk::~CacheFileChunk() [this=%p]", this));
  MOZ_COUNT_DTOR(CacheFileChunk);

  if (mBuf) {
    free(mBuf);
    mBuf = nullptr;
    mBufSize = 0;
  }

  if (mReadBuf) {
    free(mReadBuf);
    mReadBuf = nullptr;
    mReadBufSize = 0;
  }
}

void
CacheFileChunk::InitNew(CacheFileChunkListener *aCallback)
{
  mFile->AssertOwnsLock();

  LOG(("CacheFileChunk::InitNew() [this=%p, listener=%p]", this, aCallback));

  MOZ_ASSERT(!mBuf);

  mBuf = static_cast<char *>(moz_xmalloc(kChunkSize));
  mBufSize = kChunkSize;
  mDataSize = 0;
}

nsresult
CacheFileChunk::Read(CacheFileHandle *aHandle, uint32_t aLen,
                     CacheHashUtils::Hash16_t aHash,
                     CacheFileChunkListener *aCallback)
{
  mFile->AssertOwnsLock();

  LOG(("CacheFileChunk::Read() [this=%p, handle=%p, len=%d, listener=%p]",
       this, aHandle, aLen, aCallback));

  MOZ_ASSERT(!mBuf);
  MOZ_ASSERT(!mReadBuf);
  MOZ_ASSERT(!mIsReady);
  MOZ_ASSERT(aLen);

  nsresult rv;

  mReadBuf = static_cast<char *>(moz_xmalloc(aLen));
  mReadBufSize = aLen;

  rv = CacheFileIOManager::Read(aHandle, mIndex * kChunkSize, mReadBuf, aLen,
                                this);
  NS_ENSURE_SUCCESS(rv, rv);

  mListener = aCallback;
  mDataSize = aLen;
  mReadHash = aHash;
  return NS_OK;
}

nsresult
CacheFileChunk::Write(CacheFileHandle *aHandle,
                      CacheFileChunkListener *aCallback)
{
  mFile->AssertOwnsLock();

  LOG(("CacheFileChunk::Write() [this=%p, handle=%p, listener=%p]",
       this, aHandle, aCallback));

  MOZ_ASSERT(mBuf);
  MOZ_ASSERT(!mIsReady);

  nsresult rv;

  // TODO FIXME !!! Don't write chunk when it is empty

  rv = CacheFileIOManager::Write(aHandle, mIndex * kChunkSize, mBuf, mDataSize,
                                 false, this);
  NS_ENSURE_SUCCESS(rv, rv);

  mListener = aCallback;
  return NS_OK;
}

void
CacheFileChunk::WaitForUpdate(CacheFileChunkListener *aCallback)
{
  mFile->AssertOwnsLock();

  LOG(("CacheFileChunk::WaitForUpdate() [this=%p, listener=%p]",
       this, aCallback));

  MOZ_ASSERT(mFile->mOutput);

#ifdef DEBUG
  for (uint32_t i = 0 ; i < mUpdateListeners.Length() ; i++) {
    MOZ_ASSERT(mUpdateListeners[i]->mCallback != aCallback);
  }
#endif

  ChunkListenerItem *item = new ChunkListenerItem();
//  item->mTarget = NS_GetCurrentThread();
  nsCOMPtr<nsIThread> mainThread;               // temporary HACK
  NS_GetMainThread(getter_AddRefs(mainThread)); // there are long delays when
  item->mTarget = mainThread;                   // using streamcopier's thread
  item->mCallback = aCallback;

  mUpdateListeners.AppendElement(item);
}

nsresult
CacheFileChunk::CancelWait(CacheFileChunkListener *aCallback)
{
  mFile->AssertOwnsLock();

  LOG(("CacheFileChunk::CancelWait() [this=%p, listener=%p]", this, aCallback));

  uint32_t i;
  for (i = 0 ; i < mUpdateListeners.Length() ; i++) {
    ChunkListenerItem *item = mUpdateListeners[i];

    if (item->mCallback == aCallback) {
      mUpdateListeners.RemoveElementAt(i);
      delete item;
      break;
    }
  }

#ifdef DEBUG
  for ( ; i < mUpdateListeners.Length() ; i++) {
    MOZ_ASSERT(mUpdateListeners[i]->mCallback != aCallback);
  }
#endif

  return NS_OK;
}

nsresult
CacheFileChunk::NotifyUpdateListeners()
{
  mFile->AssertOwnsLock();

  LOG(("CacheFileChunk::NotifyUpdateListeners() [this=%p]", this));

  nsresult rv, rv2;

  rv = NS_OK;
  for (uint32_t i = 0 ; i < mUpdateListeners.Length() ; i++) {
    ChunkListenerItem *item = mUpdateListeners[i];

    LOG(("CacheFileChunk::NotifyUpdateListeners() - Notifying listener %p "
         "[this=%p]", item->mCallback.get(), this));

    nsRefPtr<NotifyUpdateListenerEvent> ev;
    ev = new NotifyUpdateListenerEvent(item->mCallback, this);
    rv2 = item->mTarget->Dispatch(ev, NS_DISPATCH_NORMAL);
    if (NS_FAILED(rv2) && NS_SUCCEEDED(rv))
      rv = rv2;
    delete item;
  }

  mUpdateListeners.Clear();

  return rv;
}

uint32_t
CacheFileChunk::Index()
{
  return mIndex;
}

CacheHashUtils::Hash16_t
CacheFileChunk::Hash()
{
  mFile->AssertOwnsLock();

  MOZ_ASSERT(mBuf);
  MOZ_ASSERT(!mListener);

  return CacheHashUtils::Hash16(mBuf, mDataSize);
}

uint32_t
CacheFileChunk::DataSize()
{
  mFile->AssertOwnsLock();
  return mDataSize;
}

void
CacheFileChunk::UpdateDataSize(uint32_t aOffset, uint32_t aLen, bool aEOF)
{
  mFile->AssertOwnsLock();

  MOZ_ASSERT(!aEOF, "Implement me! What to do with opened streams?");

  LOG(("CacheFileChunk::UpdateDataSize() [this=%p, offset=%d, len=%d, EOF=%d]",
       this, aOffset, aLen, aEOF));

  mIsDirty = true;

  if (mIsReady) {
    MOZ_ASSERT(mValidityMap.Length() == 0);

    int64_t fileSize = kChunkSize * mIndex + aOffset + aLen;
    if (aEOF || fileSize > mFile->mDataSize) {
      mFile->mDataSize = fileSize;
    }

    if (aEOF || aOffset + aLen > mDataSize) {
      mDataSize = aOffset + aLen;
      NotifyUpdateListeners();
    }

    return;
  }


  // We're still waiting for data from the disk. This chunk cannot be used by
  // input stream, so there must be no update listener. We also need to keep
  // track of where the data is written so that we can correctly merge the new
  // data with the old one.

  MOZ_ASSERT(mUpdateListeners.Length() == 0);

  ValidityPair pair(aOffset, aLen);

  if (mValidityMap.Length() == 0) {
    mValidityMap.AppendElement(pair);
    return;
  }


  // Find out where to place this pair into the map, it can overlap with
  // one preceding pair and all subsequent pairs.
  uint32_t pos = 0;
  for (pos = mValidityMap.Length() ; pos > 0 ; pos--) {
    if (mValidityMap[pos-1].LessThan(pair)) {
      if (mValidityMap[pos-1].Overlaps(pair)) {
        // Merge with the preceding pair
        mValidityMap[pos-1].Merge(pair);
        pos--; // Point to the updated pair
      }
      else {
        if (pos == mValidityMap.Length())
          mValidityMap.AppendElement(pair);
        else
          mValidityMap.InsertElementAt(pos, pair);
      }

      break;
    }
  }

  if (!pos)
    mValidityMap.InsertElementAt(0, pair);

  // Now pos points to merged or inserted pair, check whether it overlaps with
  // subsequent pairs.
  while (pos + 1 < mValidityMap.Length()) {
    if (mValidityMap[pos].Overlaps(mValidityMap[pos + 1])) {
      mValidityMap[pos].Merge(mValidityMap[pos + 1]);
      mValidityMap.RemoveElementAt(pos + 1);
    }
    else {
      break;
    }
  }
}

nsresult
CacheFileChunk::OnFileOpened(CacheFileHandle *aHandle, nsresult aResult)
{
  MOZ_NOT_REACHED("CacheFileChunk::OnFileOpened should not be called!");
  return NS_ERROR_UNEXPECTED;
}

nsresult
CacheFileChunk::OnDataWritten(CacheFileHandle *aHandle, const char *aBuf,
                              nsresult aResult)
{
  LOG(("CacheFileChunk::OnDataWritten() [this=%p, handle=%p]", this, aHandle));

  nsCOMPtr<CacheFileChunkListener> listener;

  {
    CacheFileAutoLock lock(mFile);

    MOZ_ASSERT(mListener);

    mIsDirty = false;
    mListener.swap(listener);
  }

  listener->OnChunkWritten(aResult, this);

  return NS_OK;
}

nsresult
CacheFileChunk::OnDataRead(CacheFileHandle *aHandle, char *aBuf,
                           nsresult aResult)
{
  LOG(("CacheFileChunk::OnDataRead() [this=%p, handle=%p, result=0x%08x]",
       this, aHandle, aResult));

  nsCOMPtr<CacheFileChunkListener> listener;

  {
    CacheFileAutoLock lock(mFile);

    MOZ_ASSERT(mListener);

    if (NS_SUCCEEDED(aResult)) {
      CacheHashUtils::Hash16_t hash = CacheHashUtils::Hash16(mReadBuf,
                                                             mReadBufSize);
      if (hash != mReadHash) {
        LOG(("CacheFileChunk::OnDataRead() - Hash mismatch! Hash of the data is"
             " %hx, hash in metadata is %hx. [this=%p, idx=%d]",
             hash, mReadHash, this, mIndex));
        aResult = NS_ERROR_FILE_CORRUPTED;
      }
      else {
        if (!mBuf) {
          // Just swap the buffers if we don't have mBuf yet
          MOZ_ASSERT(mDataSize == mReadBufSize);
          mBuf = mReadBuf;
          mBufSize = mReadBufSize;
          mReadBuf = nullptr;
          mReadBufSize = 0;
        } else {
          // Merge data with write buffer
          if (mReadBufSize < mBufSize) {
            mReadBuf = static_cast<char *>(moz_xrealloc(mReadBuf, mBufSize));
            mReadBufSize = mBufSize;
          }

          for (uint32_t i = 0 ; i < mValidityMap.Length() ; i++) {
            memcpy(mReadBuf + mValidityMap[i].Offset(),
                   mBuf + mValidityMap[i].Offset(), mValidityMap[i].Len());
          }

          free(mBuf);
          mBuf = mReadBuf;
          mBufSize = mReadBufSize;
          mReadBuf = nullptr;
          mReadBufSize = 0;
        }
      }
    }

    if (NS_FAILED(aResult)) {
      mDataSize = 0;
    }

    mListener.swap(listener);
  }

  listener->OnChunkRead(aResult, this);

  return NS_OK;
}

nsresult
CacheFileChunk::OnFileDoomed(CacheFileHandle *aHandle, nsresult aResult)
{
  MOZ_NOT_REACHED("CacheFileChunk::OnFileDoomed should not be called!");
  return NS_ERROR_UNEXPECTED;
}

nsresult
CacheFileChunk::OnEOFSet(CacheFileHandle *aHandle, nsresult aResult)
{
  MOZ_NOT_REACHED("CacheFileChunk::OnEOFSet should not be called!");
  return NS_ERROR_UNEXPECTED;
}

bool
CacheFileChunk::IsReady()
{
  mFile->AssertOwnsLock();

  return mIsReady;
}

void
CacheFileChunk::SetReady(bool aReady)
{
  mFile->AssertOwnsLock();

  LOG(("CacheFileChunk::SetReady() [this=%p, ready=%d]", this, aReady));

  mIsReady = aReady;
}

bool
CacheFileChunk::IsDirty()
{
  mFile->AssertOwnsLock();

  return mIsDirty;
}

char *
CacheFileChunk::Buf()
{
  mFile->AssertOwnsLock();

  return mBuf;
}

void
CacheFileChunk::EnsureBufSize(uint32_t aBufSize)
{
  if (mBufSize >= aBufSize)
    return;

  // find smallest power of 2 greater than or equal to aBufSize
  aBufSize--;
  aBufSize |= aBufSize >> 1;
  aBufSize |= aBufSize >> 2;
  aBufSize |= aBufSize >> 4;
  aBufSize |= aBufSize >> 8;
  aBufSize |= aBufSize >> 16;
  aBufSize++;

  const uint32_t minBufSize = 512;
  const uint32_t maxBufSize = kChunkSize;
  aBufSize = clamped(aBufSize, minBufSize, maxBufSize);

  mBuf = static_cast<char *>(moz_xrealloc(mBuf, aBufSize));
  mBufSize = aBufSize;
}

} // net
} // mozilla
