/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheFileChunk.h"

#include "CacheFile.h"
#include "nsThreadUtils.h"


namespace mozilla {
namespace net {

class NotifyUpdateListenerEvent : public nsRunnable {
public:
  NotifyUpdateListenerEvent(CacheFileChunkListener *aCallback,
                            CacheFileChunk *aChunk)
    : mCallback(aCallback)
    , mChunk(aChunk)
  {
    MOZ_COUNT_CTOR(NotifyUpdateListenerEvent);
  }

  ~NotifyUpdateListenerEvent()
  {
    MOZ_COUNT_DTOR(NotifyUpdateListenerEvent);
  }

  NS_IMETHOD Run()
  {
    mCallback->OnChunkUpdated(mChunk);
    return NS_OK;
  }

protected:
  nsCOMPtr<CacheFileChunkListener> mCallback;
  nsRefPtr<CacheFileChunk>         mChunk;
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
  , mFile(aFile)
{
  MOZ_COUNT_CTOR(CacheFileChunk);
}

CacheFileChunk::~CacheFileChunk()
{
  MOZ_COUNT_DTOR(CacheFileChunk);

  if (mBuf) {
    free(mBuf);
    mBuf = nullptr;
  }
}

void
CacheFileChunk::InitNew(CacheFileChunkListener *aCallback)
{
  mFile->AssertOwnsLock();

  MOZ_ASSERT(!mBuf);

  mBuf = static_cast<char *>(moz_xmalloc(kChunkSize));
  mDataSize = 0;
}

nsresult
CacheFileChunk::Read(CacheFileHandle *aHandle, uint32_t aLen,
                     CacheFileChunkListener *aCallback)
{
  mFile->AssertOwnsLock();

  MOZ_ASSERT(!mBuf);
  MOZ_ASSERT(!mIsReady);

  nsresult rv;

  mBuf = static_cast<char *>(moz_xmalloc(kChunkSize));
  rv = CacheFileIOManager::Read(aHandle, mIndex * kChunkSize, mBuf, aLen, this);
  NS_ENSURE_SUCCESS(rv, rv);

  mListener = aCallback;
  mDataSize = aLen;
  return NS_OK;
}

nsresult
CacheFileChunk::Write(CacheFileHandle *aHandle,
                      CacheFileChunkListener *aCallback)
{
  mFile->AssertOwnsLock();

  MOZ_ASSERT(mBuf);
  MOZ_ASSERT(!mIsReady);

  nsresult rv;

  rv = CacheFileIOManager::Write(aHandle, mIndex * kChunkSize, mBuf, mDataSize,
                                 this);
  NS_ENSURE_SUCCESS(rv, rv);

  mListener = aCallback;
  return NS_OK;
}

void
CacheFileChunk::WaitForUpdate(CacheFileChunkListener *aCallback)
{
  mFile->AssertOwnsLock();

  MOZ_ASSERT(mFile->mOutput);

#ifdef DEBUG
  for (uint32_t i = 0 ; i < mUpdateListeners.Length() ; i++) {
    MOZ_ASSERT(mUpdateListeners[i]->mCallback != aCallback);
  }
#endif

  ChunkListenerItem *item = new ChunkListenerItem();
  item->mTarget = NS_GetCurrentThread();
  item->mCallback = aCallback;

  mUpdateListeners.AppendElement(item);
}

nsresult
CacheFileChunk::CancelWait(CacheFileChunkListener *aCallback)
{
  mFile->AssertOwnsLock();

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

  nsresult rv, rv2;

  rv = NS_OK;
  for (uint32_t i = 0 ; i < mUpdateListeners.Length() ; i++) {
    ChunkListenerItem *item = mUpdateListeners[i];

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
  MOZ_ASSERT(mBuf);
  MOZ_ASSERT(!mListener);

  return CacheHashUtils::Hash16(mBuf, mDataSize);
}

uint32_t
CacheFileChunk::DataSize()
{
  return mDataSize;
}

nsresult
CacheFileChunk::OnFileOpened(CacheFileHandle *aHandle, nsresult aResult)
{
  MOZ_NOT_REACHED("CacheFileChunk::OnFileOpened should not be called!");
  return NS_ERROR_UNEXPECTED;
}

nsresult
CacheFileChunk::OnDataWritten(CacheFileHandle *aHandle, nsresult aResult)
{
  MOZ_ASSERT(mListener);

  mIsDirty = false;
  nsCOMPtr<CacheFileChunkListener> listener;
  mListener.swap(listener);
  listener->OnChunkWritten(aResult, this);

  return NS_OK;
}

nsresult
CacheFileChunk::OnDataRead(CacheFileHandle *aHandle, nsresult aResult)
{
  MOZ_ASSERT(mListener);

  if (NS_FAILED(aResult))
    mDataSize = 0;

  nsCOMPtr<CacheFileChunkListener> listener;
  mListener.swap(listener);
  listener->OnChunkRead(aResult, this);

  return NS_OK;
}

nsresult
CacheFileChunk::OnFileDoomed(CacheFileHandle *aHandle, nsresult aResult)
{
  MOZ_NOT_REACHED("CacheFileChunk::OnFileDoomed should not be called!");
  return NS_ERROR_UNEXPECTED;
}

bool
CacheFileChunk::IsReady()
{
  return mIsReady;
}

void
CacheFileChunk::SetReady(bool aReady)
{
  mIsReady = aReady;
}

bool
CacheFileChunk::IsDirty()
{
  return mIsDirty;
}

char *
CacheFileChunk::Buf()
{
  return mBuf;
}

} // net
} // mozilla
