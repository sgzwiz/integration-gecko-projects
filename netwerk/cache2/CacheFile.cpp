/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheLog.h"
#include "CacheFile.h"

#include "CacheFileChunk.h"
#include "CacheFileInputStream.h"
#include "CacheFileOutputStream.h"
#include "nsThreadUtils.h"
#include "mozilla/DebugOnly.h"
#include <algorithm>

namespace mozilla {
namespace net {

class NotifyCacheFileListenerEvent : public nsRunnable {
public:
  NotifyCacheFileListenerEvent(CacheFileListener *aCallback,
                               nsresult aResult,
                               bool aIsNew)
    : mCallback(aCallback)
    , mRV(aResult)
    , mIsNew(aIsNew)
  {
    LOG(("NotifyCacheFileListenerEvent::NotifyCacheFileListenerEvent() "
         "[this=%p]", this));
    MOZ_COUNT_CTOR(NotifyCacheFileListenerEvent);
  }

  ~NotifyCacheFileListenerEvent()
  {
    LOG(("NotifyCacheFileListenerEvent::~NotifyCacheFileListenerEvent() "
         "[this=%p]", this));
    MOZ_COUNT_DTOR(NotifyCacheFileListenerEvent);
  }

  NS_IMETHOD Run()
  {
    LOG(("NotifyCacheFileListenerEvent::Run() [this=%p]", this));

    mCallback->OnFileReady(mRV, mIsNew);
    return NS_OK;
  }

protected:
  nsCOMPtr<CacheFileListener> mCallback;
  nsresult                    mRV;
  bool                        mIsNew;
};

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
    LOG(("NotifyChunkListenerEvent::NotifyChunkListenerEvent() [this=%p]",
         this));
    MOZ_COUNT_CTOR(NotifyChunkListenerEvent);
  }

  ~NotifyChunkListenerEvent()
  {
    LOG(("NotifyChunkListenerEvent::~NotifyChunkListenerEvent() [this=%p]",
         this));
    MOZ_COUNT_DTOR(NotifyChunkListenerEvent);
  }

  NS_IMETHOD Run()
  {
    LOG(("NotifyChunkListenerEvent::Run() [this=%p]", this));

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
  , mOpeningFile(false)
  , mReady(false)
  , mMemoryOnly(false)
  , mDataAccessed(false)
  , mWritingMetadata(false)
  , mDoomRequested(false)
  , mDataSize(-1)
  , mOutput(nullptr)
{
  LOG(("CacheFile::CacheFile() [this=%p]", this));

  MOZ_COUNT_CTOR(CacheFile);
  mChunks.Init();
  mCachedChunks.Init();
  mChunkListeners.Init();
}

CacheFile::~CacheFile()
{
  LOG(("CacheFile::~CacheFile() [this=%p]", this));

  MOZ_COUNT_DTOR(CacheFile);
}

nsresult
CacheFile::Init(const nsACString &aKey,
                bool aCreateNew,
                bool aMemoryOnly,
                CacheFileListener *aCallback)
{
  MOZ_ASSERT(!mListener);
  MOZ_ASSERT(!mHandle);

  nsresult rv;

  mKey = aKey;
  mMemoryOnly = aMemoryOnly;

  LOG(("CacheFile::Init() [this=%p, key=%s, createNew=%d, memoryOnly=%d, "
       "listener=%p]", this, mKey.get(), aCreateNew, aMemoryOnly, aCallback));

  if (mMemoryOnly) {
    MOZ_ASSERT(!aCallback);

    mMetadata = new CacheFileMetadata(mKey);
    mReady = true;
    mDataSize = mMetadata->Offset();
    return NS_OK;
  }
  else {
    uint32_t flags;
    if (aCreateNew) {
      MOZ_ASSERT(!aCallback);
      flags = CacheFileIOManager::CREATE_NEW;

      // make sure we can use this entry immediately
      mMetadata = new CacheFileMetadata(mKey);
      mReady = true;
      mDataSize = mMetadata->Offset();
    }
    else
      flags = CacheFileIOManager::CREATE;

    mOpeningFile = true;
    mListener = aCallback;
    rv = CacheFileIOManager::OpenFile(mKey, flags, this);
    if (NS_FAILED(rv)) {
      mListener = nullptr;
      mOpeningFile = false;

      if (aCreateNew) {
        NS_WARNING("Forcing memory-only entry since OpenFile failed");
        LOG(("CacheFile::Init() - CacheFileIOManager::OpenFile() failed "
             "synchronously. We can continue in memory-only mode since "
             "aCreateNew == true. [this=%p]", this));

        mMemoryOnly = true;
      }
      else if (rv == NS_ERROR_NOT_INITIALIZED) {
        NS_WARNING("Forcing memory-only entry since CacheIOManager isn't "
                   "initialized.");
        LOG(("CacheFile::Init() - CacheFileIOManager isn't initialized, "
             "initializing entry as memory-only. [this=%p]", this));

        mMemoryOnly = true;
        mMetadata = new CacheFileMetadata(mKey);
        mReady = true;
        mDataSize = mMetadata->Offset();

        nsRefPtr<NotifyCacheFileListenerEvent> ev;
        ev = new NotifyCacheFileListenerEvent(aCallback, NS_OK, true);
        rv = NS_DispatchToCurrentThread(ev);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      else {
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
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

  nsresult rv;

  uint32_t index = aChunk->Index();

  LOG(("CacheFile::OnChunkRead() [this=%p, rv=0x%08x, chunk=%p, idx=%d]",
       this, aResult, index));

  if (NS_SUCCEEDED(aResult))
    aChunk->SetReady(true);

  if (HaveChunkListeners(index)) {
    rv = NotifyChunkListeners(index, aResult, aChunk);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
CacheFile::OnChunkWritten(nsresult aResult, CacheFileChunk *aChunk)
{
  CacheFileAutoLock lock(this);

  MOZ_ASSERT(!mMemoryOnly);

  LOG(("CacheFile::OnChunkWritten() [this=%p, rv=0x%08x, chunk=%p, idx=%d]",
       this, aResult, aChunk, aChunk->Index()));

  nsresult rv;

  if (NS_FAILED(aResult)) {
    // TODO ??? doom entry
    // TODO mark this chunk as memory only, since it wasn't written to disk and
    // therefore cannot be released from memory
    // LOG
  }

  // update hash value in metadata
  mMetadata->SetHash(aChunk->Index(), aChunk->Hash());

  // notify listeners if there is any
  if (HaveChunkListeners(aChunk->Index())) {
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

  MOZ_ASSERT(aChunk->mRefCnt == 2);

  LOG(("CacheFile::OnChunkWritten() - Caching unused chunk "
       "[this=%p, chunk=%p]", this, aChunk));

  aChunk->SetReady(true);
  aChunk->mRemovingChunk = true;
  aChunk->mFile = nullptr;
  mCachedChunks.Put(aChunk->Index(), aChunk);
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
  nsresult rv;

  nsCOMPtr<CacheFileListener> listener;
  bool doomListener = true;
  bool isNew = false;
  nsresult retval = NS_OK;

  {
    CacheFileAutoLock lock(this);

    MOZ_ASSERT(mOpeningFile);
    MOZ_ASSERT((NS_SUCCEEDED(aResult) && aHandle) ||
               (NS_FAILED(aResult) && !aHandle));
    MOZ_ASSERT(
      (mListener && !mMetadata && !mDoomRequested) || // !createNew
      (!mListener && mMetadata && !mDoomRequested) || // createNew
      (mListener && mMetadata && mDoomRequested)   || // createNew, doomed
      (!mListener && mMetadata && mDoomRequested));   // createNew, doomed, !cb
    MOZ_ASSERT(mOpeningFile);
    MOZ_ASSERT(!mMemoryOnly || mMetadata); // memory-only was set on new entry

    LOG(("CacheFile::OnFileOpened() [this=%p, rv=0x%08x, handle=%p]",
         this, aResult, aHandle));

    mOpeningFile = false;

    if (mMemoryOnly) {
      // We can be here only in case the entry was initilized as createNew and
      // SetMemoryOnly() was called.

      // The file could also be doomed before SetMemoryOnly() was called.
      if (mDoomRequested) {
        mDoomRequested = false;

        retval = NS_ERROR_NOT_AVAILABLE;

        if (aHandle) {
          // Doom the file
          rv = CacheFileIOManager::DoomFile(aHandle,
                                            mListener ? this : nullptr);
          if (NS_SUCCEEDED(rv)) {
            // If there is a listener, it will be notifed from OnFileDoomed()
            return NS_OK;
          }

          retval = rv;
        }

        if (!mListener)
          // Nobody is interested about the dooming result
          return NS_OK;

        doomListener = true;
        mListener.swap(listener);
      }
      else {
        // Just don't store the handle into mHandle and exit
        return NS_OK;
      }
    }
    else if (NS_FAILED(aResult)) {
      if (mMetadata) {
        // This entry was initialized as createNew, just switch to memory-only
        // mode. If there is a listener, it waits for doom notification.
        NS_WARNING("Forcing memory-only entry since OpenFile failed");
        LOG(("CacheFile::OnFileOpened() - CacheFileIOManager::OpenFile() "
             "failed asynchronously. We can continue in memory-only mode since "
             "aCreateNew == true. [this=%p]", this));

        MOZ_ASSERT(!mListener || mDoomRequested);

        mMemoryOnly = true;
        mDoomRequested = false;

        if (!mListener)
          return NS_OK;

        doomListener = true;
        retval = NS_ERROR_NOT_AVAILABLE;
      }
      else if (aResult == NS_ERROR_FILE_INVALID_PATH) {
        // CacheFileIOManager doesn't have mCacheDirectory, switch to
        // memory-only mode.
        NS_WARNING("Forcing memory-only entry since CacheFileIOManager doesn't "
                   "have mCacheDirectory.");
        LOG(("CacheFile::OnFileOpened() - CacheFileIOManager doesn't have "
             "mCacheDirectory, initializing entry as memory-only. [this=%p]",
             this));

        MOZ_ASSERT(!mDoomRequested);
        MOZ_ASSERT(mListener);

        mMemoryOnly = true;
        mMetadata = new CacheFileMetadata(mKey);
        mReady = true;
        mDataSize = mMetadata->Offset();

        doomListener = false;
        isNew = true;
        retval = NS_OK;
      }
      else {
        // CacheFileIOManager::OpenFile() failed for another reason.
        MOZ_ASSERT(!mDoomRequested);
        MOZ_ASSERT(mListener);

        doomListener = false;
        isNew = false;
        retval = aResult;
      }

      mListener.swap(listener);
    }
    else {
      mHandle = aHandle;

      if (mMetadata) {
        MOZ_ASSERT(!mListener);

        // The entry was initialized as createNew, don't try to read metadata.
        mMetadata->SetHandle(mHandle);

        // Write all cached chunks, otherwise thay may stay unwritten.
        mCachedChunks.Enumerate(&CacheFile::WriteAllCachedChunks, this);

        if (mDoomRequested) {
          mDoomRequested = false;

          rv = CacheFileIOManager::DoomFile(mHandle,
                                            mListener ? this : nullptr);
          if (NS_SUCCEEDED(rv)) {
            // If there is a listener, it will be notifed from OnFileDoomed()
            return NS_OK;
          }

          if (!mListener)
            return NS_OK;

          doomListener = true;
          retval = rv;
          mListener.swap(listener);
        }
        else {
          return NS_OK;
        }
      }
    }
  }

  if (listener) {
    if (doomListener)
      listener->OnFileDoomed(retval);
    else
      listener->OnFileReady(retval, isNew);

    return NS_OK;
  }

  MOZ_ASSERT(NS_SUCCEEDED(aResult));
  MOZ_ASSERT(!mMetadata);
  MOZ_ASSERT(mListener);
  MOZ_ASSERT(!mDoomRequested);

  mMetadata = new CacheFileMetadata(mHandle, mKey);

  rv = mMetadata->ReadMetadata(this);
  if (NS_FAILED(rv)) {
    mListener.swap(listener);
    listener->OnFileReady(rv, false);
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

  LOG(("CacheFile::OnMetadataRead() [this=%p, rv=0x%08x]", this, aResult));

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

  LOG(("CacheFile::OnMetadataWritten() [this=%p, rv=0x%08x]", this, aResult));

  MOZ_ASSERT(mWritingMetadata);
  mWritingMetadata = false;

  if (NS_FAILED(aResult)) {
    // TODO close streams with an error ???
  }

  if (!mMemoryOnly)
    WriteMetadataIfNeeded();

  if (!mOutput && !mInputs.Length() && !mChunks.Count() && !mWritingMetadata) {
    LOG(("CacheFile::OnMetadataWritten() - Releasing file handle [this=%p]",
         this));
    CacheFileIOManager::ReleaseNSPRHandle(mHandle);
  }

  return NS_OK;
}

nsresult
CacheFile::OnFileDoomed(CacheFileHandle *aHandle, nsresult aResult)
{
  nsCOMPtr<CacheFileListener> listener;

  {
    CacheFileAutoLock lock(this);

    MOZ_ASSERT(mListener);

    LOG(("CacheFile::OnFileDoomed() [this=%p, rv=0x%08x, handle=%p]",
         this, aResult, aHandle));

    mListener.swap(listener);
  }

  listener->OnFileDoomed(aResult);
  return NS_OK;
}

nsresult
CacheFile::OnEOFSet(CacheFileHandle *aHandle, nsresult aResult)
{
  MOZ_NOT_REACHED("CacheFile::OnEOFSet should not be called!");
  return NS_ERROR_UNEXPECTED;
}

nsresult
CacheFile::OpenInputStream(nsIInputStream **_retval)
{
  CacheFileAutoLock lock(this);

  MOZ_ASSERT(mHandle || mMemoryOnly || mOpeningFile);

  if (!mReady) {
    LOG(("CacheFile::OpenInputStream() - CacheFile is not ready [this=%p]",
         this));

    return NS_ERROR_NOT_AVAILABLE;
  }

  CacheFileInputStream *input = new CacheFileInputStream(this);

  LOG(("CacheFile::OpenInputStream() - Creating new input stream %p [this=%p]",
       input, this));

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

  MOZ_ASSERT(mHandle || mMemoryOnly || mOpeningFile);

  if (!mReady) {
    LOG(("CacheFile::OpenOutputStream() - CacheFile is not ready [this=%p]",
         this));

    return NS_ERROR_NOT_AVAILABLE;
  }

  if (mOutput) {
    LOG(("CacheFile::OpenOutputStream() - We already have output stream %p "
         "[this=%p]", mOutput, this));

    return NS_ERROR_NOT_AVAILABLE;
  }

  mOutput = new CacheFileOutputStream(this);

  LOG(("CacheFile::OpenOutputStream() - Creating new output stream %p "
       "[this=%p]", mOutput, this));

  mDataAccessed = true;
  NS_ADDREF(*_retval = mOutput);
  return NS_OK;
}

nsresult
CacheFile::SetMemoryOnly()
{
  LOG(("CacheFile::SetMemoryOnly() aMemoryOnly=%d [this=%p]",
       mMemoryOnly, this));

  if (mMemoryOnly)
    return NS_OK;

  MOZ_ASSERT(mReady);

  if (!mReady) {
    LOG(("CacheFile::SetMemoryOnly() - CacheFile is not ready [this=%p]",
         this));

    return NS_ERROR_NOT_AVAILABLE;
  }

  if (mDataAccessed) {
    LOG(("CacheFile::SetMemoryOnly() - Data was already accessed [this=%p]", this));
    return NS_ERROR_NOT_AVAILABLE;
  }

  // TODO what to do when this isn't a new entry and has an existing metadata???
  mMemoryOnly = true;
  return NS_OK;
}

nsresult
CacheFile::Doom(CacheFileListener *aCallback)
{
  CacheFileAutoLock lock(this);

  MOZ_ASSERT(!mListener);
  MOZ_ASSERT(mHandle || mMemoryOnly || mOpeningFile);

  LOG(("CacheFile::Doom() [this=%p, listener=%p]", this, aCallback));

  nsresult rv;

  if (mDoomRequested) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (mMemoryOnly) {
    // TODO what exactly to do here?
    return NS_ERROR_NOT_AVAILABLE;
  }
  else if (mOpeningFile) {
    MOZ_ASSERT(!mListener);

    mDoomRequested = true;
    mListener = aCallback;

    return NS_OK;
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

nsresult
CacheFile::ThrowMemoryCachedData()
{
  CacheFileAutoLock lock(this);

  LOG(("CacheFile::ThrowMemoryCachedData() [this=%p]", this));

  if (mMemoryOnly) {
    LOG(("CacheFile::ThrowMemoryCachedData() - Ignoring request because the "
         "entry is memory-only [this=%p]", this));

    return NS_OK;
  }

  if (mOpeningFile) {
    LOG(("CacheFile::ThrowMemoryCachedData() - Ignoring request because the "
         "entry is still opening the file [this=%p]", this));

    return NS_OK;
  }

  mCachedChunks.Clear();
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
                    CacheFileChunkListener *aCallback, CacheFileChunk **_retval)
{
  CacheFileAutoLock lock(this);
  return GetChunkLocked(aIndex, aWriter, aCallback, _retval);
}

nsresult
CacheFile::GetChunkLocked(uint32_t aIndex, bool aWriter,
                          CacheFileChunkListener *aCallback,
                          CacheFileChunk **_retval)
{
  AssertOwnsLock();

  LOG(("CacheFile::GetChunkLocked() [this=%p, idx=%d, writer=%d, listener=%p]",
       this, aIndex, aWriter, aCallback));

  MOZ_ASSERT(mReady);
  MOZ_ASSERT(mHandle || mMemoryOnly || mOpeningFile);
  MOZ_ASSERT((aWriter && !aCallback) || (!aWriter && aCallback));

  nsresult rv;

  nsRefPtr<CacheFileChunk> chunk;
  if (mChunks.Get(aIndex, getter_AddRefs(chunk))) {
    LOG(("CacheFile::GetChunkLocked() - Found chunk %p in mChunks [this=%p]",
         chunk.get(), this));

    if (chunk->IsReady() || aWriter) {
      chunk.swap(*_retval);
    }
    else {
      rv = QueueChunkListener(aIndex, aCallback);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    return NS_OK;
  }

  if (mCachedChunks.Get(aIndex, getter_AddRefs(chunk))) {
    LOG(("CacheFile::GetChunkLocked() - Reusing cached chunk %p [this=%p]",
         chunk.get(), this));

    mChunks.Put(aIndex, chunk);
    mCachedChunks.Remove(aIndex);
    chunk->mFile = this;
    chunk->mRemovingChunk = false;

    MOZ_ASSERT(chunk->IsReady());

    chunk.swap(*_retval);
    return NS_OK;
  }

  int64_t off = aIndex * kChunkSize;

  if (off < mDataSize) {
    // We cannot be here if this is memory only entry since the chunk must exist
    MOZ_ASSERT(!mMemoryOnly);

    chunk = new CacheFileChunk(this, aIndex);
    mChunks.Put(aIndex, chunk);

    LOG(("CacheFile::GetChunkLocked() - Reading newly created chunk %p from "
         "the disk [this=%p]", chunk.get(), this));

    // Read the chunk from the disk
    rv = chunk->Read(mHandle, std::min(static_cast<uint32_t>(mDataSize - off),
                     static_cast<uint32_t>(kChunkSize)),
                     mMetadata->GetHash(aIndex), this);
    if (NS_FAILED(rv)) {
      chunk->mRemovingChunk = true;
      chunk->mFile = nullptr;
      mChunks.Remove(aIndex);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if (aWriter) {
      chunk.swap(*_retval);
    }
    else {
      rv = QueueChunkListener(aIndex, aCallback);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    return NS_OK;
  }
  else if (off == mDataSize) {
    if (aWriter) {
      // this listener is going to write to the chunk
      chunk = new CacheFileChunk(this, aIndex);
      mChunks.Put(aIndex, chunk);

      LOG(("CacheFile::GetChunkLocked() - Created new empty chunk %p [this=%p]",
           chunk.get(), this));

      chunk->InitNew(this);
      chunk->SetReady(true);
      mMetadata->SetHash(aIndex, chunk->Hash());

      if (HaveChunkListeners(aIndex)) {
        rv = NotifyChunkListeners(aIndex, NS_OK, chunk);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      chunk.swap(*_retval);
      return NS_OK;
    }
  }
  else {
    if (aWriter) {
      // this chunk was requested by writer, but we need to fill the gap first

      // Fill with zero the last chunk if it is incomplete
      if (mDataSize % kChunkSize) {
        rv = PadChunkWithZeroes(mDataSize / kChunkSize);
        NS_ENSURE_SUCCESS(rv, rv);

        MOZ_ASSERT(!(mDataSize % kChunkSize));
      }

      uint32_t startChunk = mDataSize / kChunkSize;

      if (mMemoryOnly) {
        // We need to create all missing CacheFileChunks if this is memory-only
        // entry
        for (uint32_t i = startChunk ; i < aIndex ; i++) {
          rv = PadChunkWithZeroes(i);
          NS_ENSURE_SUCCESS(rv, rv);
        }
      }
      else {
        // We don't need to create CacheFileChunk for other empty chunks unless
        // there is some input stream waiting for this chunk.

        if (startChunk != aIndex) {
          // Make sure the file contains zeroes at the end of the file
          rv = CacheFileIOManager::TruncateSeekSetEOF(mHandle,
                                                      startChunk * kChunkSize,
                                                      aIndex * kChunkSize,
                                                      nullptr);
          NS_ENSURE_SUCCESS(rv, rv);
        }

        for (uint32_t i = startChunk ; i < aIndex ; i++) {
          if (HaveChunkListeners(i)) {
            rv = PadChunkWithZeroes(i);
            NS_ENSURE_SUCCESS(rv, rv);
          }
          else {
            mMetadata->SetHash(i, kEmptyChunkHash);
            mDataSize = (i + 1) * kChunkSize;
          }
        }
      }

      MOZ_ASSERT(mDataSize == off);
      rv = GetChunkLocked(aIndex, true, nullptr, getter_AddRefs(chunk));
      NS_ENSURE_SUCCESS(rv, rv);

      chunk.swap(*_retval);
      return NS_OK;
    }
  }

  if (mOutput) {
    // the chunk doesn't exist but mOutput may create it
    rv = QueueChunkListener(aIndex, aCallback);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    return NS_ERROR_NOT_AVAILABLE;
  }

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

    LOG(("CacheFile::RemoveChunk() [this=%p, chunk=%p, idx=%d]",
         this, aChunk, aChunk->Index()));

    MOZ_ASSERT(mReady);
    MOZ_ASSERT(mHandle || mMemoryOnly || mOpeningFile);

    if (aChunk->mRefCnt != 2) {
      LOG(("CacheFile::RemoveChunk() - Chunk is still used [this=%p, chunk=%p, "
           "refcnt=%d]", this, aChunk, aChunk->mRefCnt));

      // somebody got the reference before the lock was acquired
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

    if (chunk->IsDirty() && !mMemoryOnly && !mOpeningFile) {
      LOG(("CacheFile::RemoveChunk() - Writing dirty chunk to the disk "
           "[this=%p]", this));

      aChunk->SetReady(false);
      rv = chunk->Write(mHandle, this);
      if (NS_FAILED(rv)) {
        // TODO ??? doom entry
        // TODO mark this chunk as memory only, since it wasn't written to disk
        // and therefore cannot be released from memory
        // LOG
      }
      else {
        // Chunk will be removed in OnChunkWritten if it is still unused
        return NS_OK;
      }
    }

    LOG(("CacheFile::RemoveChunk() - Caching unused chunk [this=%p, chunk=%p]",
         this, chunk.get()));

    chunk->mRemovingChunk = true;
    chunk->mFile = nullptr;
    mCachedChunks.Put(chunk->Index(), chunk);
    mChunks.Remove(chunk->Index());
    if (!mMemoryOnly)
      WriteMetadataIfNeeded();
  }

  return NS_OK;
}

nsresult
CacheFile::RemoveInput(CacheFileInputStream *aInput)
{
  CacheFileAutoLock lock(this);

  LOG(("CacheFile::RemoveInput() [this=%p, input=%p]", this, aInput));

  DebugOnly<bool> found;
  found = mInputs.RemoveElement(aInput);
  MOZ_ASSERT(found);

  ReleaseOutsideLock(static_cast<nsIInputStream*>(aInput));

  if (!mMemoryOnly)
    WriteMetadataIfNeeded();

  return NS_OK;
}

nsresult
CacheFile::RemoveOutput(CacheFileOutputStream *aOutput)
{
  AssertOwnsLock();

  LOG(("CacheFile::RemoveOutput() [this=%p, output=%p]", this, aOutput));

  if (mOutput != aOutput) {
    LOG(("CacheFile::RemoveOutput() - This output was already removed, ignoring"
         " call [this=%p]", this));
    return NS_OK;
  }

  mOutput = nullptr;

  // Cancel all queued chunk and update listeners that cannot be satisfied
  NotifyListenersAboutOutputRemoval();

  if (!mMemoryOnly)
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
  LOG(("CacheFile::NotifyChunkListener() [this=%p, listener=%p, target=%p, "
       "rv=0x%08x, idx=%d, chunk=%p]", this, aCallback, aTarget, aResult,
       aChunkIdx, aChunk));

  nsresult rv;
  nsRefPtr<NotifyChunkListenerEvent> ev;
  ev = new NotifyChunkListenerEvent(aCallback, aResult, aChunkIdx, aChunk);
  if (aTarget)
    rv = aTarget->Dispatch(ev, NS_DISPATCH_NORMAL);
  else
//    rv = NS_DispatchToCurrentThread(ev);  // temporary HACK (see below)
    rv = NS_DispatchToMainThread(ev);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
CacheFile::QueueChunkListener(uint32_t aIndex,
                              CacheFileChunkListener *aCallback)
{
  LOG(("CacheFile::QueueChunkListener() [this=%p, idx=%d, listener=%p]",
       this, aIndex, aCallback));

  AssertOwnsLock();

  MOZ_ASSERT(aCallback);

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
  LOG(("CacheFile::NotifyChunkListeners() [this=%p, idx=%d, rv=0x%08x, "
       "chunk=%p]", this, aIndex, aResult, aChunk));

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

bool
CacheFile::HaveChunkListeners(uint32_t aIndex)
{
  ChunkListeners *listeners;
  mChunkListeners.Get(aIndex, &listeners);
  return !!listeners;
}

void
CacheFile::NotifyListenersAboutOutputRemoval()
{
  LOG(("CacheFile::NotifyListenersAboutOutputRemoval() [this=%p]", this));

  AssertOwnsLock();

  // First fail all chunk listeners that wait for non-existent chunk
  mChunkListeners.Enumerate(&CacheFile::FailListenersIfNonExistentChunk,
                            this);

  // Fail all update listeners
  mChunks.Enumerate(&CacheFile::FailUpdateListeners, this);
}

bool
CacheFile::DataSize(int64_t* aSize)
{
  CacheFileAutoLock lock(this);

  if (mOutput)
    return false;

  *aSize = mDataSize;
  return true;
}

void
CacheFile::WriteMetadataIfNeeded()
{
  LOG(("CacheFile::WriteMetadataIfNeeded() [this=%p]", this));

  AssertOwnsLock();
  MOZ_ASSERT(!mMemoryOnly);

  nsresult rv;

  if (!mOutput && !mInputs.Length() && !mChunks.Count() && mMetadata->IsDirty() &&
      !mWritingMetadata && !mOpeningFile) {
    LOG(("CacheFile::WriteMetadataIfNeeded() - Writing metadata [this=%p]",
         this));

    rv = mMetadata->WriteMetadata(mDataSize, this);
    if (NS_SUCCEEDED(rv)) {
      mWritingMetadata = true;
    }
    else {
      LOG(("CacheFile::WriteMetadataIfNeeded() - Writing synchronously failed "
           "[this=%p]", this));
      // TODO: close streams with error
    }
  }
}

PLDHashOperator
CacheFile::WriteAllCachedChunks(const uint32_t& aIdx,
                                nsRefPtr<CacheFileChunk>& aChunk,
                                void* aClosure)
{
  CacheFile *file = static_cast<CacheFile*>(aClosure);

  LOG(("CacheFile::WriteAllCachedChunks() [this=%p, idx=%d, chunk=%p]",
       file, aIdx, aChunk.get()));

  file->mChunks.Put(aIdx, aChunk);
  aChunk->mFile = file;
  aChunk->mRemovingChunk = false;

  MOZ_ASSERT(aChunk->IsReady());

  NS_ADDREF(aChunk);
  file->ReleaseOutsideLock(aChunk);

  return PL_DHASH_REMOVE;
}

PLDHashOperator
CacheFile::FailListenersIfNonExistentChunk(
  const uint32_t& aIdx,
  nsAutoPtr<ChunkListeners>& aListeners,
  void* aClosure)
{
  CacheFile *file = static_cast<CacheFile*>(aClosure);

  LOG(("CacheFile::FailListenersIfNonExistentChunk() [this=%p, idx=%d]",
       file, aIdx));

  nsRefPtr<CacheFileChunk> chunk;
  file->mChunks.Get(aIdx, getter_AddRefs(chunk));
  if (chunk) {
    MOZ_ASSERT(!chunk->IsReady());
    return PL_DHASH_NEXT;
  }

  for (uint32_t i = 0 ; i < aListeners->mItems.Length() ; i++) {
    ChunkListenerItem *item = aListeners->mItems[i];
    file->NotifyChunkListener(item->mCallback, item->mTarget,
                              NS_ERROR_NOT_AVAILABLE, aIdx, nullptr);
    delete item;
  }

  return PL_DHASH_REMOVE;
}

PLDHashOperator
CacheFile::FailUpdateListeners(
  const uint32_t& aIdx,
  nsRefPtr<CacheFileChunk>& aChunk,
  void* aClosure)
{
#ifdef PR_LOGGING
  CacheFile *file = static_cast<CacheFile*>(aClosure);
#endif

  LOG(("CacheFile::FailUpdateListeners() [this=%p, idx=%d]",
       file, aIdx));

  if (aChunk->IsReady()) {
    aChunk->NotifyUpdateListeners();
  }

  return PL_DHASH_NEXT;
}

nsresult
CacheFile::PadChunkWithZeroes(uint32_t aChunkIdx)
{
  AssertOwnsLock();

  // This method is used to pad last incomplete chunk with zeroes or create
  // a new chunk full of zeroes
  MOZ_ASSERT(mDataSize / kChunkSize == aChunkIdx);

  nsresult rv;
  nsRefPtr<CacheFileChunk> chunk;
  rv = GetChunkLocked(aChunkIdx, true, nullptr, getter_AddRefs(chunk));
  NS_ENSURE_SUCCESS(rv, rv);

  LOG(("CacheFile::PadChunkWithZeroes() - Zeroing hole in chunk %d, range %d-%d"
       " [this=%p]", aChunkIdx, chunk->DataSize(), kChunkSize - 1, this));

  chunk->EnsureBufSize(kChunkSize);
  memset(chunk->Buf() + chunk->DataSize(), 0, kChunkSize - chunk->DataSize());

  chunk->UpdateDataSize(chunk->DataSize(), kChunkSize - chunk->DataSize(),
                        false);

  ReleaseOutsideLock(chunk.forget().get());

  return NS_OK;
}

} // net
} // mozilla
