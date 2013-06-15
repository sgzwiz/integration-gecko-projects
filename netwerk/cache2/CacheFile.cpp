/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheFile.h"

#include "CacheFileChunk.h"
#include "CacheFileInputStream.h"
#include "CacheFileOutputStream.h"
#include "CacheFileUtils.h"
#include "nsThreadUtils.h"
#include <algorithm>

namespace mozilla {
namespace net {

class NotifyChunkListenerEvent : public nsRunnable {
public:
  NotifyChunkListenerEvent(CacheFileChunkListener *aCallback,
                           nsresult aResult,
                           uint32_t aChunkIdx,
                           CacheFileChunk *aChunk)
    : mCallback(aCallback)
    , mRV(aResult)
    , mChunkIdx(aChunkIdx)
    , mChunk(aChunk)
  {
    MOZ_COUNT_CTOR(NotifyChunkListenerEvent);
  }

  ~NotifyChunkListenerEvent()
  {
    MOZ_COUNT_DTOR(NotifyChunkListenerEvent);
  }

  NS_IMETHOD Run()
  {
    mCallback->OnChunkAvailable(mRV, mChunkIdx, mChunk);
    return NS_OK;
  }

protected:
  nsCOMPtr<CacheFileChunkListener> mCallback;
  nsresult                         mRV;
  uint32_t                         mChunkIdx;
  nsRefPtr<CacheFileChunk>         mChunk;
};

NS_IMPL_THREADSAFE_ISUPPORTS3(CacheFile,
                              CacheFileChunkListener,
                              CacheFileIOListener,
                              CacheFileMetadataListener)

CacheFile::CacheFile()
  : mLock("CacheFile.mLock")
  , mReady(false)
  , mMemoryOnly(false)
  , mDataAccessed(false)
  , mWritingMetadata(false)
  , mDataSize(-1)
{
  MOZ_COUNT_CTOR(CacheFile);
  mChunks.Init();
  mChunkListeners.Init();
}

CacheFile::~CacheFile()
{
  MOZ_COUNT_DTOR(CacheFile);
}

nsresult
CacheFile::Init(const nsACString &aKey,
                bool aCreateNew,
                CacheFileListener *aCallback)
{
  MOZ_ASSERT(!mListener);
  MOZ_ASSERT(!mHandle);

  nsresult rv;

  mKey = aKey;

  SHA1Sum sum;
  SHA1Sum::Hash hash;
  sum.update(mKey.get(), mKey.Length());
  sum.finish(hash);

  uint32_t flags;
  if (aCreateNew)
    flags = CacheFileIOManager::CREATE_NEW;
  else
    flags = CacheFileIOManager::CREATE;

  mListener = aCallback;
  rv = CacheFileIOManager::OpenFile(&hash, flags, this);
  if (NS_FAILED(rv)) {
    mListener = nullptr;
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

CacheFileMetadata*
CacheFile::Metadata()
{
  AssertOwnsLock();
  return mMetadata;
}

nsresult
CacheFile::OnChunkRead(nsresult aResult, CacheFileChunk *aChunk)
{
  CacheFileAutoLock lock(this);

  uint32_t index = aChunk->Index();

  if (NS_SUCCEEDED(aResult)) {
    MOZ_ASSERT(aChunk->DataSize() != 0);
    // check that hash matches
    if (aChunk->Hash() != mMetadata->GetHash(index)) {
      aResult = NS_ERROR_FILE_CORRUPTED;
      aChunk->mRemovingChunk = true;
      mChunks.Remove(index);
      aChunk = nullptr;
      // TODO notify other streams ???
    }
  }

  if (NS_SUCCEEDED(aResult))
    aChunk->SetReady(true);

  return NotifyChunkListeners(index, aResult, aChunk);
}

nsresult
CacheFile::OnChunkWritten(nsresult aResult, CacheFileChunk *aChunk)
{
  CacheFileAutoLock lock(this);

  nsresult rv;

  if (NS_FAILED(aResult)) {
    // TODO ??? doom entry
    NotifyChunkListeners(aChunk->Index(), aResult, nullptr);
  }
  else {
    // update hash value in metadata
    mMetadata->SetHash(aChunk->Index(), aChunk->Hash());

    // notify listeners if there is any
    ChunkListeners *listeners;
    mChunkListeners.Get(aChunk->Index(), &listeners);
    if (listeners) {
      // don't release the chunk since there are some listeners queued
      rv = NotifyChunkListeners(aChunk->Index(), NS_OK, aChunk);
      if (NS_SUCCEEDED(rv)) {
        MOZ_ASSERT(aChunk->mRefCnt != 2);
        aChunk->SetReady(true);
        return NS_OK;
      }

      NS_WARNING("NotifyChunkListeners failed???");
      if (aChunk->mRefCnt != 2) {
        // Some of the listeners didn't fail and got the reference
        aChunk->SetReady(true);
        return NS_OK;
      }
    }
  }

  MOZ_ASSERT(aChunk->mRefCnt == 2);
  aChunk->mRemovingChunk = true;
  mChunks.Remove(aChunk->Index());
  WriteMetadataIfNeeded();

  return NS_OK;
}

nsresult
CacheFile::OnChunkAvailable(nsresult aResult, uint32_t aChunkIdx,
                            CacheFileChunk *aChunk)
{
  MOZ_NOT_REACHED("CacheFile::OnChunkAvailable should not be called!");
  return NS_ERROR_UNEXPECTED;
}

nsresult
CacheFile::OnChunkUpdated(CacheFileChunk *aChunk)
{
  MOZ_NOT_REACHED("CacheFile::OnChunkUpdated should not be called!");
  return NS_ERROR_UNEXPECTED;
}

nsresult
CacheFile::OnFileOpened(CacheFileHandle *aHandle, nsresult aResult)
{
  MOZ_ASSERT(mListener);

  nsresult rv;
  nsCOMPtr<CacheFileListener> listener;

  if (NS_FAILED(aResult)) {
    mListener.swap(listener);
    listener->OnFileReady(aResult, false);
    return NS_OK;
  }

  mHandle = aHandle;

  mMetadata = new CacheFileMetadata(mHandle, mKey);

  rv = mMetadata->ReadMetadata(this);
  if (NS_FAILED(rv)) {
    mListener.swap(listener);
    listener->OnFileReady(rv, false);
    return NS_OK;
  }

  return NS_OK;
}

nsresult
CacheFile::OnDataWritten(CacheFileHandle *aHandle, const char *aBuf,
                         nsresult aResult)
{
  MOZ_NOT_REACHED("CacheFile::OnDataWritten should not be called!");
  return NS_ERROR_UNEXPECTED;
}

nsresult
CacheFile::OnDataRead(CacheFileHandle *aHandle, char *aBuf, nsresult aResult)
{
  MOZ_NOT_REACHED("CacheFile::OnDataRead should not be called!");
  return NS_ERROR_UNEXPECTED;
}

nsresult
CacheFile::OnMetadataRead(nsresult aResult)
{
  MOZ_ASSERT(mListener);

  bool isNew = false;
  if (NS_SUCCEEDED(aResult)) {
    mReady = true;
    mDataSize = mMetadata->Offset();
    if (mDataSize == 0 && mMetadata->ElementsSize() == 0) {
      isNew = true;
      mMetadata->MarkDirty();
    }
  }

  nsCOMPtr<CacheFileListener> listener;
  mListener.swap(listener);
  listener->OnFileReady(aResult, isNew);
  return NS_OK;
}

nsresult
CacheFile::OnMetadataWritten(nsresult aResult)
{
  CacheFileAutoLock lock(this);

  MOZ_ASSERT(mWritingMetadata);
  mWritingMetadata = false;

  if (NS_FAILED(aResult)) {

    // TODO close streams with an error ???
  }

  WriteMetadataIfNeeded();

  return NS_OK;
}

nsresult
CacheFile::OnFileDoomed(CacheFileHandle *aHandle, nsresult aResult)
{
  MOZ_ASSERT(mListener);

  nsCOMPtr<CacheFileListener> listener;
  mListener.swap(listener);
  listener->OnFileDoomed(aResult);
  return NS_OK;
}

nsresult
CacheFile::OpenInputStream(nsIInputStream **_retval)
{
  CacheFileAutoLock lock(this);

  MOZ_ASSERT(mHandle);

  if (!mReady)
    return NS_ERROR_NOT_AVAILABLE;

  CacheFileInputStream *input = new CacheFileInputStream(this);

  mInputs.AppendElement(input);
  NS_ADDREF(input);

  mDataAccessed = true;
  NS_ADDREF(*_retval = input);
  return NS_OK;
}

nsresult
CacheFile::OpenOutputStream(nsIOutputStream **_retval)
{
  CacheFileAutoLock lock(this);

  MOZ_ASSERT(mHandle);

  if (!mReady)
    return NS_ERROR_NOT_AVAILABLE;

  if (mOutput)
    return NS_ERROR_NOT_AVAILABLE;

  mOutput = new CacheFileOutputStream(this);

  mDataAccessed = true;
  NS_ADDREF(*_retval = mOutput);
  return NS_OK;
}

nsresult
CacheFile::SetMemoryOnly(bool aMemoryOnly)
{
  if (mMemoryOnly == aMemoryOnly)
    return NS_OK;

  MOZ_ASSERT(mReady);
  // MemoryOnly can be set only before we start reading/writting data
  MOZ_ASSERT(!mDataAccessed);

  if (!mReady || mDataAccessed)
    return NS_ERROR_UNEXPECTED;

  // TODO what to do when this isn't a new entry and has an existing metadata???
  mMemoryOnly = aMemoryOnly;
  return NS_OK;
}

nsresult
CacheFile::Doom(CacheFileListener *aCallback)
{
  MOZ_ASSERT(!mListener);
  MOZ_ASSERT(mHandle || mMemoryOnly);

  nsresult rv;

  if (mMemoryOnly) {
    // TODO what exactly to do here?
    return NS_ERROR_NOT_AVAILABLE;
  }
  else {
    mListener = aCallback;
    rv = CacheFileIOManager::DoomFile(mHandle, aCallback ? this : nullptr);
    if (NS_FAILED(rv)) {
      mListener = nullptr;
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

void
CacheFile::Lock()
{
  mLock.Lock();
}

void
CacheFile::Unlock()
{
  nsTArray<nsISupports*> objs;
  objs.SwapElements(mObjsToRelease);

  mLock.Unlock();

  for (uint32_t i = 0; i < objs.Length(); i++)
    objs[i]->Release();
}

void
CacheFile::AssertOwnsLock()
{
  mLock.AssertCurrentThreadOwns();
}

void 
CacheFile::ReleaseOutsideLock(nsISupports *aObject)
{
  AssertOwnsLock();

  mObjsToRelease.AppendElement(aObject);
}

nsresult
CacheFile::GetChunk(uint32_t aIndex, bool aWriter,
                    CacheFileChunkListener *aCallback)
{
  CacheFileAutoLock lock(this);
  return GetChunkLocked(aIndex, aWriter, aCallback);
}

nsresult
CacheFile::GetChunkLocked(uint32_t aIndex, bool aWriter,
                          CacheFileChunkListener *aCallback)
{
  AssertOwnsLock();

  MOZ_ASSERT(mReady);
  MOZ_ASSERT(mHandle || mMemoryOnly);

  nsresult rv;

  nsRefPtr<CacheFileChunk> chunk;
  if (mChunks.Get(aIndex, getter_AddRefs(chunk))) {
    if (chunk->IsReady())
      rv = NotifyChunkListener(aCallback, nullptr, NS_OK, aIndex, chunk);
    else
      rv = QueueChunkListener(aIndex, aCallback);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  int64_t off = aIndex * kChunkSize;

  if (off < mDataSize) {
    // We cannot be here if this is memory only entry since the chunk must exist
    MOZ_ASSERT(!mMemoryOnly);

    rv = QueueChunkListener(aIndex, aCallback);
    NS_ENSURE_SUCCESS(rv, rv);

    chunk = new CacheFileChunk(this, aIndex);
    mChunks.Put(aIndex, chunk);

    // Read the chunk from the disk
    rv = chunk->Read(mHandle, std::min(static_cast<uint32_t>(mDataSize - off),
                     static_cast<uint32_t>(kChunkSize)), this);
    if (NS_FAILED(rv)) {
      chunk->mRemovingChunk = true;
      mChunks.Remove(aIndex);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    return NS_OK;
  }
  else if (off == mDataSize) {
    if (aWriter) {
      // this listener is going to write to the chunk
      rv = QueueChunkListener(aIndex, aCallback);
      NS_ENSURE_SUCCESS(rv, rv);

      chunk = new CacheFileChunk(this, aIndex);
      mChunks.Put(aIndex, chunk);
      chunk->InitNew(this);
      chunk->SetReady(true);

      rv = NotifyChunkListeners(aIndex, NS_OK, chunk);
      NS_ENSURE_SUCCESS(rv, rv);

      return NS_OK;
    }
  }
  else {
    if (aWriter) {
      // this chunk was requested by writer, but we need to fill the gap first
      if (mGapFiller) {
        rv = NotifyChunkListener(aCallback, nullptr, NS_ERROR_NOT_AVAILABLE,
                                 aIndex, nullptr);
        NS_ENSURE_SUCCESS(rv, rv);

        return NS_OK;
      }

      mGapFiller = new GapFiller(off / kChunkSize, aIndex, aCallback, this);
      NS_DispatchToCurrentThread(mGapFiller);

      return NS_OK;
    }
  }

  if (mOutput)
    // the chunk doesn't exist but mOutput may create it
    rv = QueueChunkListener(aIndex, aCallback);
  else
    // the chunk doesn't exist and nobody is going to create it
    rv = NotifyChunkListener(aCallback, nullptr, NS_ERROR_NOT_AVAILABLE, aIndex,
                             nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
CacheFile::RemoveChunk(CacheFileChunk *aChunk)
{
  nsresult rv;

  // Avoid lock reentrancy by increasing the RefCnt
  nsRefPtr<CacheFileChunk> chunk = aChunk;

  {
    CacheFileAutoLock lock(this);

    MOZ_ASSERT(mReady);
    MOZ_ASSERT(mHandle || mMemoryOnly);

    if (aChunk->mRefCnt != 2) {
      // somebody got the reference before the lock was acquired
      return NS_OK;
    }

    if (mMemoryOnly) {
      // cannot release chunks in case of memory only entry
      return NS_OK;
    }

#ifdef DEBUG
    {
      // We can be here iff the chunk is in the hash table
      nsRefPtr<CacheFileChunk> chunkCheck;
      mChunks.Get(chunk->Index(), getter_AddRefs(chunkCheck));
      MOZ_ASSERT(chunkCheck == chunk);

      // We also shouldn't have any queued listener for this chunk
      ChunkListeners *listeners;
      mChunkListeners.Get(chunk->Index(), &listeners);
      MOZ_ASSERT(!listeners);
    }
#endif

    if (chunk->IsDirty()) {
      aChunk->SetReady(false);
      rv = chunk->Write(mHandle, this);
      if (NS_FAILED(rv)) {
        MOZ_ASSERT(false, "Unexpected failure while writting chunk");
      }
      else {
        // Chunk will be removed in OnChunkWritten if it is still unused
        return NS_OK;
      }
    }

    chunk->mRemovingChunk = true;
    mChunks.Remove(chunk->Index());
    WriteMetadataIfNeeded();
  }

  return NS_OK;
}

nsresult
CacheFile::RemoveInput(CacheFileInputStream *aInput)
{
  CacheFileAutoLock lock(this);

#ifdef DEBUG
  bool found =
#endif
  mInputs.RemoveElement(aInput);
  MOZ_ASSERT(found);

  ReleaseOutsideLock(static_cast<nsIInputStream*>(aInput));

  WriteMetadataIfNeeded();

  return NS_OK;
}

nsresult
CacheFile::RemoveOutput(CacheFileOutputStream *aOutput)
{
  CacheFileAutoLock lock(this);

  // TODO cancel all queued chunk listeners that cannot be satisfied

  MOZ_ASSERT(mOutput == aOutput);

  if (mOutput != aOutput)
    return NS_ERROR_FAILURE;

  ReleaseOutsideLock(static_cast<nsIOutputStream*>(mOutput.forget().get()));

  WriteMetadataIfNeeded();

  return NS_OK;
}

nsresult
CacheFile::NotifyChunkListener(CacheFileChunkListener *aCallback,
                               nsIEventTarget *aTarget,
                               nsresult aResult,
                               uint32_t aChunkIdx,
                               CacheFileChunk *aChunk)
{
  nsresult rv;
  nsRefPtr<NotifyChunkListenerEvent> ev;
  ev = new NotifyChunkListenerEvent(aCallback, aResult, aChunkIdx, aChunk);
  if (aTarget)
    rv = aTarget->Dispatch(ev, NS_DISPATCH_NORMAL);
  else
    rv = NS_DispatchToCurrentThread(ev);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
CacheFile::QueueChunkListener(uint32_t aIndex,
                              CacheFileChunkListener *aCallback)
{
  AssertOwnsLock();

  ChunkListenerItem *item = new ChunkListenerItem();
//  item->mTarget = NS_GetCurrentThread();
  nsCOMPtr<nsIThread> mainThread;               // temporary HACK
  NS_GetMainThread(getter_AddRefs(mainThread)); // there are long delays when
  item->mTarget = mainThread;                   // using streamcopier's thread
  item->mCallback = aCallback;

  ChunkListeners *listeners;
  if (!mChunkListeners.Get(aIndex, &listeners)) {
    listeners = new ChunkListeners();
    mChunkListeners.Put(aIndex, listeners);
  }

  listeners->mItems.AppendElement(item);
  return NS_OK;
}

nsresult
CacheFile::NotifyChunkListeners(uint32_t aIndex, nsresult aResult,
                                CacheFileChunk *aChunk)
{
  AssertOwnsLock();

  nsresult rv, rv2;

  ChunkListeners *listeners;
  mChunkListeners.Get(aIndex, &listeners);
  MOZ_ASSERT(listeners);

  rv = NS_OK;
  for (uint32_t i = 0 ; i < listeners->mItems.Length() ; i++) {
    ChunkListenerItem *item = listeners->mItems[i];
    rv2 = NotifyChunkListener(item->mCallback, item->mTarget, aResult, aIndex,
                              aChunk);
    if (NS_FAILED(rv2) && NS_SUCCEEDED(rv))
      rv = rv2;
    delete item;
  }

  mChunkListeners.Remove(aIndex);

  return rv;
}

int64_t
CacheFile::DataSize()
{
  CacheFileAutoLock lock(this);
  return mDataSize;
}

void
CacheFile::WriteMetadataIfNeeded()
{
  AssertOwnsLock();

  nsresult rv;

  if (!mOutput && !mInputs.Length() && !mChunks.Count() && mMetadata->IsDirty() &&
      !mWritingMetadata) {
    rv = mMetadata->WriteMetadata(mDataSize, this);
    if (NS_SUCCEEDED(rv)) {
      mWritingMetadata = true;
    }
    else {
      // TODO: close streams with error
    }
  }
}

} // net
} // mozilla
