/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheFileIOManager.h"

#include "CacheLog.h"
#include "../cache/nsCacheUtils.h"
#include "CacheHashUtils.h"
#include "nsThreadUtils.h"
#include "nsIFile.h"
#include "mozilla/Telemetry.h"
#include "mozilla/DebugOnly.h"
#include "nsDirectoryServiceUtils.h"
#include "nsAppDirectoryServiceDefs.h"

namespace mozilla {
namespace net {


NS_IMPL_THREADSAFE_ADDREF(CacheFileHandle)
NS_IMETHODIMP_(nsrefcnt)
CacheFileHandle::Release()
{
  nsrefcnt count;
  NS_PRECONDITION(0 != mRefCnt, "dup release");
  count = NS_AtomicDecrementRefcnt(mRefCnt);
  NS_LOG_RELEASE(this, count, "CacheFileHandle");

  if (0 == count) {
    mRefCnt = 1;
    delete (this);
    return 0;
  }

  if (!mRemovingHandle && count == 1) {
    CacheFileIOManager::gInstance->CloseHandle(this);
  }

  return count;
}

NS_INTERFACE_MAP_BEGIN(CacheFileHandle)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END_THREADSAFE

CacheFileHandle::CacheFileHandle(const SHA1Sum::Hash *aHash)
  : mHash(aHash)
  , mIsDoomed(false)
  , mRemovingHandle(false)
  , mFileSize(-1)
  , mFD(nullptr)
{
  MOZ_COUNT_CTOR(CacheFileHandle);
  PR_INIT_CLIST(this);
}

CacheFileHandle::~CacheFileHandle()
{
  MOZ_COUNT_DTOR(CacheFileHandle);
}


/******************************************************************************
 *  CacheFileHandles
 *****************************************************************************/

class CacheFileHandlesEntry : public PLDHashEntryHdr
{
public:
  PRCList      *mHandles;
  SHA1Sum::Hash mHash;
};

PLDHashTableOps CacheFileHandles::mOps =
{
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  HashKey,
  MatchEntry,
  MoveEntry,
  ClearEntry,
  PL_DHashFinalizeStub
};

CacheFileHandles::CacheFileHandles()
  : mInitialized(false)
{
  MOZ_COUNT_CTOR(CacheFileHandles);
}

CacheFileHandles::~CacheFileHandles()
{
  MOZ_COUNT_DTOR(CacheFileHandles);
  if (mInitialized)
    Shutdown();
}

nsresult
CacheFileHandles::Init()
{
  LOG(("CacheFileHandles::Init() %p", this));

  MOZ_ASSERT(!mInitialized);
  mInitialized = PL_DHashTableInit(&mTable, &mOps, nullptr,
                                   sizeof(CacheFileHandlesEntry), 512);

  return mInitialized ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

void
CacheFileHandles::Shutdown()
{
  LOG(("CacheFileHandles::Shutdown() %p", this));

  if (mInitialized) {
    PL_DHashTableFinish(&mTable);
    mInitialized = false;
  }
}

PLDHashNumber
CacheFileHandles::HashKey(PLDHashTable *table, const void *key)
{
  const SHA1Sum::Hash *hash = static_cast<const SHA1Sum::Hash *>(key);
  return static_cast<PLDHashNumber>(((*hash)[0] << 24) | ((*hash)[1] << 16) |
                                    ((*hash)[2] << 8) | (*hash)[3]);
}

bool
CacheFileHandles::MatchEntry(PLDHashTable *table,
                             const PLDHashEntryHdr *header,
                             const void *key)
{
  const CacheFileHandlesEntry *entry;

  entry = static_cast<const CacheFileHandlesEntry *>(header);

  return (memcmp(&entry->mHash, key, sizeof(SHA1Sum::Hash)) == 0);
}

void
CacheFileHandles::MoveEntry(PLDHashTable *table,
                            const PLDHashEntryHdr *from,
                            PLDHashEntryHdr *to)
{
  const CacheFileHandlesEntry *src;
  CacheFileHandlesEntry *dst;

  src = static_cast<const CacheFileHandlesEntry *>(from);
  dst = static_cast<CacheFileHandlesEntry *>(to);

  dst->mHandles = src->mHandles;
  memcpy(&dst->mHash, &src->mHash, sizeof(SHA1Sum::Hash));

  LOG(("CacheFileHandles::MoveEntry() hash=%08x%08x%08x%08x%08x "
       "moving from %p to %p", LOGSHA1(src->mHash), from, to));

  // update pointer to mHash in all handles
  CacheFileHandle *handle = (CacheFileHandle *)PR_LIST_HEAD(dst->mHandles);
  while (handle != dst->mHandles) {
    handle->mHash = &dst->mHash;
    handle = (CacheFileHandle *)PR_NEXT_LINK(handle);
  }
}

void
CacheFileHandles::ClearEntry(PLDHashTable *table,
                             PLDHashEntryHdr *header)
{
  CacheFileHandlesEntry *entry = static_cast<CacheFileHandlesEntry *>(header);
  delete entry->mHandles;
  entry->mHandles = nullptr;
}

nsresult
CacheFileHandles::GetHandle(const SHA1Sum::Hash *aHash,
                            CacheFileHandle **_retval)
{
  MOZ_ASSERT(mInitialized);

  // find hash entry for key
  CacheFileHandlesEntry *entry;
  entry = static_cast<CacheFileHandlesEntry *>(
    PL_DHashTableOperate(&mTable,
                         (void *)aHash,
                         PL_DHASH_LOOKUP));
  if (PL_DHASH_ENTRY_IS_FREE(entry)) {
    LOG(("CacheFileHandles::GetHandle() hash=%08x%08x%08x%08x%08x "
         "no handle found", LOGSHA1(aHash)));
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Check if the entry is doomed
  CacheFileHandle *handle = static_cast<CacheFileHandle *>(
                              PR_LIST_HEAD(entry->mHandles));
  if (handle->IsDoomed()) {
    LOG(("CacheFileHandles::GetHandle() hash=%08x%08x%08x%08x%08x "
         "found doomed handle %p, entry %p", LOGSHA1(aHash), handle, entry));
    return NS_ERROR_NOT_AVAILABLE;
  }

  LOG(("CacheFileHandles::GetHandle() hash=%08x%08x%08x%08x%08x "
       "found handle %p, entry %p", LOGSHA1(aHash), handle, entry));
  NS_ADDREF(*_retval = handle);
  return NS_OK;
}


nsresult
CacheFileHandles::NewHandle(const SHA1Sum::Hash *aHash,
                            CacheFileHandle **_retval)
{
  MOZ_ASSERT(mInitialized);

  // find hash entry for key
  CacheFileHandlesEntry *entry;
  entry = static_cast<CacheFileHandlesEntry *>(
    PL_DHashTableOperate(&mTable,
                         (void *)aHash,
                         PL_DHASH_ADD));
  if (!entry) return NS_ERROR_OUT_OF_MEMORY;

  if (!entry->mHandles) {
    // new entry
    entry->mHandles = new PRCList;
    memcpy(&entry->mHash, aHash, sizeof(SHA1Sum::Hash));
    PR_INIT_CLIST(entry->mHandles);

    LOG(("CacheFileHandles::NewHandle() hash=%08x%08x%08x%08x%08x "
         "created new entry %p, list %p", LOGSHA1(aHash), entry,
         entry->mHandles));
  }
#if DEBUG
  else {
    MOZ_ASSERT(!PR_CLIST_IS_EMPTY(entry->mHandles));
    CacheFileHandle *handle = (CacheFileHandle *)PR_LIST_HEAD(entry->mHandles);
    MOZ_ASSERT(handle->IsDoomed());
  }
#endif

  nsRefPtr<CacheFileHandle> handle = new CacheFileHandle(&entry->mHash);
  PR_APPEND_LINK(handle, entry->mHandles);

  LOG(("CacheFileHandles::NewHandle() hash=%08x%08x%08x%08x%08x "
       "created new handle %p, entry=%p", LOGSHA1(aHash), handle.get(), entry));

  NS_ADDREF(*_retval = handle);
  handle.forget();
  return NS_OK;
}

void
CacheFileHandles::RemoveHandle(CacheFileHandle *aHandle)
{
  MOZ_ASSERT(mInitialized);

  void *key = (void *)aHandle->Hash();

  CacheFileHandlesEntry *entry;
  entry = static_cast<CacheFileHandlesEntry *>(
    PL_DHashTableOperate(&mTable, key, PL_DHASH_LOOKUP));

  MOZ_ASSERT(PL_DHASH_ENTRY_IS_BUSY(entry));

#if DEBUG
  {
    CacheFileHandle *handle = (CacheFileHandle *)PR_LIST_HEAD(entry->mHandles);
    bool handleFound = false;
    while (handle != entry->mHandles) {
      if (handle == aHandle) {
        handleFound = true;
        break;
      }
      handle = (CacheFileHandle *)PR_NEXT_LINK(handle);
    }
    MOZ_ASSERT(handleFound);
  }
#endif

  LOG(("CacheFileHandles::RemoveHandle() hash=%08x%08x%08x%08x%08x "
       "removing handle %p", LOGSHA1(&entry->mHash), aHandle));

  PR_REMOVE_AND_INIT_LINK(aHandle);
  NS_RELEASE(aHandle);

  if (PR_CLIST_IS_EMPTY(entry->mHandles)) {
    LOG(("CacheFileHandles::RemoveHandle() hash=%08x%08x%08x%08x%08x "
         "list %p is empty, removing entry %p", LOGSHA1(&entry->mHash),
         entry->mHandles, entry));
    PL_DHashTableOperate(&mTable, key, PL_DHASH_REMOVE);
  }
}

// Events

class OpenFileEvent : public nsRunnable {
public:
  OpenFileEvent(const SHA1Sum::Hash *aHash,
                uint32_t aFlags,
                CacheFileIOListener *aCallback)
    : mFlags(aFlags)
    , mCallback(aCallback)
    , mRV(NS_ERROR_FAILURE)
  {
    MOZ_COUNT_CTOR(OpenFileEvent);
    memcpy(&mHash, aHash, sizeof(SHA1Sum::Hash));
//    mTarget = static_cast<nsIEventTarget*>(NS_GetCurrentThread());
    nsCOMPtr<nsIThread> mainThread;               // temporary HACK
    NS_GetMainThread(getter_AddRefs(mainThread)); // there are long delays when
    mTarget = mainThread;                         // using streamcopier's thread
    MOZ_ASSERT(mTarget);
  }

  ~OpenFileEvent()
  {
    MOZ_COUNT_DTOR(OpenFileEvent);
  }

  NS_IMETHOD Run()
  {
    if (mTarget) {
      mRV = CacheFileIOManager::gInstance->OpenFileInternal(
        &mHash, mFlags, getter_AddRefs(mHandle));

      nsCOMPtr<nsIEventTarget> target;
      mTarget.swap(target);
      target->Dispatch(this, nsIEventTarget::DISPATCH_NORMAL);
    }
    else {
      mCallback->OnFileOpened(mHandle, mRV);
    }
    return NS_OK;
  }

protected:
  SHA1Sum::Hash                 mHash;
  bool                          mFlags;
  nsCOMPtr<CacheFileIOListener> mCallback;
  nsCOMPtr<nsIEventTarget>      mTarget;
  nsRefPtr<CacheFileHandle>     mHandle;
  nsresult                      mRV;
};

class CloseHandleEvent : public nsRunnable {
public:
  CloseHandleEvent(CacheFileHandle *aHandle)
    : mHandle(aHandle)
  {
    MOZ_COUNT_CTOR(CloseHandleEvent);
  }

  ~CloseHandleEvent()
  {
    MOZ_COUNT_DTOR(CloseHandleEvent);
  }

  NS_IMETHOD Run()
  {
    CacheFileIOManager::gInstance->CloseHandleInternal(mHandle);
    return NS_OK;
  }

protected:
  nsRefPtr<CacheFileHandle> mHandle;
};

class ReadEvent : public nsRunnable {
public:
  ReadEvent(CacheFileHandle *aHandle, int64_t aOffset, char *aBuf,
            int32_t aCount, CacheFileIOListener *aCallback)
    : mHandle(aHandle)
    , mOffset(aOffset)
    , mBuf(aBuf)
    , mCount(aCount)
    , mCallback(aCallback)
    , mRV(NS_ERROR_FAILURE)
  {
    MOZ_COUNT_CTOR(ReadEvent);
//    mTarget = static_cast<nsIEventTarget*>(NS_GetCurrentThread());
    nsCOMPtr<nsIThread> mainThread;               // temporary HACK
    NS_GetMainThread(getter_AddRefs(mainThread)); // there are long delays when
    mTarget = mainThread;                         // using streamcopier's thread
    MOZ_ASSERT(mTarget);
  }

  ~ReadEvent()
  {
    MOZ_COUNT_DTOR(ReadEvent);
  }

  NS_IMETHOD Run()
  {
    if (mTarget) {
      mRV = CacheFileIOManager::gInstance->ReadInternal(
        mHandle, mOffset, mBuf, mCount);

      nsCOMPtr<nsIEventTarget> target;
      mTarget.swap(target);
      target->Dispatch(this, nsIEventTarget::DISPATCH_NORMAL);
    }
    else {
      if (mCallback)
        mCallback->OnDataRead(mHandle, mBuf, mRV);
    }
    return NS_OK;
  }

protected:
  nsRefPtr<CacheFileHandle>     mHandle;
  int64_t                       mOffset;
  char                         *mBuf;
  int32_t                       mCount;
  nsCOMPtr<CacheFileIOListener> mCallback;
  nsCOMPtr<nsIEventTarget>      mTarget;
  nsresult                      mRV;
};

class WriteEvent : public nsRunnable {
public:
  WriteEvent(CacheFileHandle *aHandle, int64_t aOffset, const char *aBuf,
             int32_t aCount, CacheFileIOListener *aCallback)
    : mHandle(aHandle)
    , mOffset(aOffset)
    , mBuf(aBuf)
    , mCount(aCount)
    , mCallback(aCallback)
    , mRV(NS_ERROR_FAILURE)
  {
    MOZ_COUNT_CTOR(WriteEvent);
//    mTarget = static_cast<nsIEventTarget*>(NS_GetCurrentThread());
    nsCOMPtr<nsIThread> mainThread;               // temporary HACK
    NS_GetMainThread(getter_AddRefs(mainThread)); // there are long delays when
    mTarget = mainThread;                         // using streamcopier's thread
    MOZ_ASSERT(mTarget);
  }

  ~WriteEvent()
  {
    MOZ_COUNT_DTOR(WriteEvent);
  }

  NS_IMETHOD Run()
  {
    if (mTarget) {
      mRV = CacheFileIOManager::gInstance->WriteInternal(
        mHandle, mOffset, mBuf, mCount);

      nsCOMPtr<nsIEventTarget> target;
      mTarget.swap(target);
      target->Dispatch(this, nsIEventTarget::DISPATCH_NORMAL);
    }
    else {
      if (mCallback)
        mCallback->OnDataWritten(mHandle, mBuf, mRV);
    }
    return NS_OK;
  }

protected:
  nsRefPtr<CacheFileHandle>     mHandle;
  int64_t                       mOffset;
  const char                   *mBuf;
  int32_t                       mCount;
  nsCOMPtr<CacheFileIOListener> mCallback;
  nsCOMPtr<nsIEventTarget>      mTarget;
  nsresult                      mRV;
};

class DoomFileEvent : public nsRunnable {
public:
  DoomFileEvent(CacheFileHandle *aHandle,
                CacheFileIOListener *aCallback)
    : mCallback(aCallback)
    , mHandle(aHandle)
    , mRV(NS_ERROR_FAILURE)
  {
    MOZ_COUNT_CTOR(DoomFileEvent);
//    mTarget = static_cast<nsIEventTarget*>(NS_GetCurrentThread());
    nsCOMPtr<nsIThread> mainThread;               // temporary HACK
    NS_GetMainThread(getter_AddRefs(mainThread)); // there are long delays when
    mTarget = mainThread;                         // using streamcopier's thread
    MOZ_ASSERT(mTarget);
  }

  ~DoomFileEvent()
  {
    MOZ_COUNT_DTOR(DoomFileEvent);
  }

  NS_IMETHOD Run()
  {
    if (mTarget) {
      mRV = CacheFileIOManager::gInstance->DoomFileInternal(mHandle);

      nsCOMPtr<nsIEventTarget> target;
      mTarget.swap(target);
      target->Dispatch(this, nsIEventTarget::DISPATCH_NORMAL);
    }
    else {
      if (mCallback)
        mCallback->OnFileDoomed(mHandle, mRV);
    }
    return NS_OK;
  }

protected:
  bool                          mFlags;
  nsCOMPtr<CacheFileIOListener> mCallback;
  nsCOMPtr<nsIEventTarget>      mTarget;
  nsRefPtr<CacheFileHandle>     mHandle;
  nsresult                      mRV;
};


CacheFileIOManager * CacheFileIOManager::gInstance = nullptr;

CacheFileIOManager::CacheFileIOManager()
  : mTreeCreated(false)
{
  MOZ_COUNT_CTOR(CacheFileIOManager);
  NS_ASSERTION(gInstance==nullptr, "multiple CacheFileIOManager instances!");
}

CacheFileIOManager::~CacheFileIOManager()
{
  MOZ_COUNT_DTOR(CacheFileIOManager);
  gInstance = nullptr;
}

nsresult
CacheFileIOManager::Init()
{
  if (gInstance)
    return NS_ERROR_ALREADY_INITIALIZED;

  gInstance = new CacheFileIOManager();

  nsresult rv = gInstance->InitInternal();
  if (NS_FAILED(rv)) {
    delete gInstance;
    return rv;
  }

  return NS_OK;
}

nsresult
CacheFileIOManager::OnProfile()
{
  nsresult rv;
  nsCOMPtr<nsIFile> directory;
  rv = NS_GetSpecialDirectory(NS_APP_CACHE_PARENT_DIR,
                              getter_AddRefs(directory));

  if (!directory) {
    rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_LOCAL_50_DIR,
                                getter_AddRefs(directory));
  }

  if (directory) {
    rv = directory->Clone(getter_AddRefs(gInstance->mCacheDirectory));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = gInstance->mCacheDirectory->Append(NS_LITERAL_STRING("cache2"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
CacheFileIOManager::InitInternal()
{
  nsresult rv;
  rv = NS_NewNamedThread("Cache IO thread",
                         getter_AddRefs(gInstance->mIOThread));
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Can't create background thread");
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mHandles.Init();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
CacheFileIOManager::Shutdown()
{
  if (!gInstance)
    return NS_ERROR_NOT_INITIALIZED;

  if (gInstance->mIOThread)
    nsShutdownThread::BlockingShutdown(gInstance->mIOThread);
  gInstance->mIOThread = nullptr;

  delete gInstance;

  return NS_OK;
}

nsresult
CacheFileIOManager::OpenFile(const SHA1Sum::Hash *aHash,
                             uint32_t aFlags,
                             CacheFileIOListener *aCallback)
{
  MOZ_ASSERT(gInstance);

  DebugOnly<nsresult> rv;
  rv = gInstance->mIOThread->Dispatch(
    new OpenFileEvent(aHash, aFlags, aCallback),
    nsIEventTarget::DISPATCH_NORMAL);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  return NS_OK;
}

nsresult
CacheFileIOManager::OpenFileInternal(const SHA1Sum::Hash *aHash,
                                     uint32_t aFlags,
                                     CacheFileHandle **_retval)
{
  nsresult rv;

  if (!mTreeCreated) {
    rv = CreateCacheTree();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIFile> file;
  rv = GetFile(aHash, getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<CacheFileHandle> handle;
  mHandles.GetHandle(aHash, getter_AddRefs(handle));

  if (aFlags == CREATE_NEW) {
    if (handle) {
      rv = DoomFileInternal(handle);
      NS_ENSURE_SUCCESS(rv, rv);
      handle = nullptr;
    }

    rv = mHandles.NewHandle(aHash, getter_AddRefs(handle));
    NS_ENSURE_SUCCESS(rv, rv);

    bool exists;
    rv = file->Exists(&exists);
    NS_ENSURE_SUCCESS(rv, rv);

    if (exists) {
      rv = file->Remove(false);
      if (NS_FAILED(rv)) {
        NS_WARNING("Cannot remove old entry from the disk");
        // TODO log
      }
    }

    handle->mFile.swap(file);
    handle->mFileSize = 0;
  }

  if (handle) {
    handle.swap(*_retval);
    return NS_OK;
  }

  bool exists;
  rv = file->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!exists && aFlags == OPEN)
    return NS_ERROR_NOT_AVAILABLE;

  rv = mHandles.NewHandle(aHash, getter_AddRefs(handle));
  NS_ENSURE_SUCCESS(rv, rv);

  if (exists) {
    rv = file->GetFileSize(&handle->mFileSize);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = file->OpenNSPRFileDesc(PR_RDWR, 0600, &handle->mFD);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    handle->mFileSize = 0;
  }

  handle->mFile.swap(file);
  handle.swap(*_retval);
  return NS_OK;
}

nsresult
CacheFileIOManager::CloseHandle(CacheFileHandle *aHandle)
{
  MOZ_ASSERT(gInstance);

  DebugOnly<nsresult> rv;
  rv = gInstance->mIOThread->Dispatch(new CloseHandleEvent(aHandle),
                                      nsIEventTarget::DISPATCH_NORMAL);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  return NS_OK;
}

nsresult
CacheFileIOManager::CloseHandleInternal(CacheFileHandle *aHandle)
{
  // This method should be called only from CloseHandleEvent. If this handle is
  // still unused then mRefCnt should be 2 (reference in hashtable and in
  // CacheHandleEvent)

  if (aHandle->mRefCnt != 2) {
    // someone got this handle between calls to CloseHandle() and
    // CloseHandleInternal()
    return NS_OK;
  }

  // We're going to remove this handle, don't call CloseHandle() again
  aHandle->mRemovingHandle = true;

  // Close file handle
  if (aHandle->mFD) {
    PR_Close(aHandle->mFD);
  }

  // If the entry was doomed delete the file
  if (aHandle->IsDoomed()) {
    aHandle->mFile->Remove(false);
  }

  // Remove the handle from hashtable
  mHandles.RemoveHandle(aHandle);

  return NS_OK;
}

nsresult
CacheFileIOManager::Read(CacheFileHandle *aHandle, int64_t aOffset,
                         char *aBuf, int32_t aCount,
                         CacheFileIOListener *aCallback)
{
  MOZ_ASSERT(gInstance);

  DebugOnly<nsresult> rv;
  rv = gInstance->mIOThread->Dispatch(
    new ReadEvent(aHandle, aOffset, aBuf, aCount, aCallback),
    nsIEventTarget::DISPATCH_NORMAL);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  return NS_OK;
}

nsresult
CacheFileIOManager::ReadInternal(CacheFileHandle *aHandle, int64_t aOffset,
                                 char *aBuf, int32_t aCount)
{
  if (!aHandle->mFD) {
    NS_WARNING("Trying to read from non-existent file");
    return NS_ERROR_NOT_AVAILABLE;
  }

  int64_t offset = PR_Seek64(aHandle->mFD, aOffset, PR_SEEK_SET);
  if (offset == -1)
    return NS_ERROR_FAILURE;

  int32_t bytesRead = PR_Read(aHandle->mFD, aBuf, aCount);
  if (bytesRead != aCount)
    return NS_ERROR_FAILURE;

  return NS_OK;
}

nsresult
CacheFileIOManager::Write(CacheFileHandle *aHandle, int64_t aOffset,
                          const char *aBuf, int32_t aCount,
                          CacheFileIOListener *aCallback)
{
  MOZ_ASSERT(gInstance);

  DebugOnly<nsresult> rv;
  rv = gInstance->mIOThread->Dispatch(
    new WriteEvent(aHandle, aOffset, aBuf, aCount, aCallback),
    nsIEventTarget::DISPATCH_NORMAL);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  return NS_OK;
}

nsresult
CacheFileIOManager::WriteInternal(CacheFileHandle *aHandle, int64_t aOffset,
                                  const char *aBuf, int32_t aCount)
{
  nsresult rv;

  if (!aHandle->mFD) {
    rv = CreateFile(aHandle);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  int64_t offset = PR_Seek64(aHandle->mFD, aOffset, PR_SEEK_SET);
  if (offset == -1)
    return NS_ERROR_FAILURE;

  int32_t bytesWritten = PR_Write(aHandle->mFD, aBuf, aCount);

  if (bytesWritten != -1 && aHandle->mFileSize < aOffset+bytesWritten)
      aHandle->mFileSize = aOffset+bytesWritten;

  if (bytesWritten != aCount)
    return NS_ERROR_FAILURE;

  return NS_OK;
}

nsresult
CacheFileIOManager::DoomFile(CacheFileHandle *aHandle,
                             CacheFileIOListener *aCallback)
{
  MOZ_ASSERT(gInstance);

  DebugOnly<nsresult> rv;
  rv = gInstance->mIOThread->Dispatch(
    new DoomFileEvent(aHandle, aCallback),
    nsIEventTarget::DISPATCH_NORMAL);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  return NS_OK;
}

nsresult
CacheFileIOManager::DoomFileInternal(CacheFileHandle *aHandle)
{
  nsresult rv;

  if (aHandle->IsDoomed())
    return NS_OK;

  if (aHandle->mFD) {
    // we need to move the current file to the doomed directory
    PR_Close(aHandle->mFD);
    aHandle->mFD = nullptr;

    // find unused filename
    nsCOMPtr<nsIFile> file;
    rv = GetDoomedFile(getter_AddRefs(file));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFile> parentDir;
    rv = file->GetParent(getter_AddRefs(parentDir));
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoCString leafName;
    rv = file->GetNativeLeafName(leafName);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aHandle->mFile->MoveToNative(parentDir, leafName);
    NS_ENSURE_SUCCESS(rv, rv);

    aHandle->mFile.swap(file);

    rv = aHandle->mFile->OpenNSPRFileDesc(PR_RDWR, 0600, &aHandle->mFD);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  aHandle->mIsDoomed = true;
  return NS_OK;
}

nsresult
CacheFileIOManager::CreateFile(CacheFileHandle *aHandle)
{
  MOZ_ASSERT(!aHandle->mFD);

  nsresult rv;

  nsCOMPtr<nsIFile> file;
  if (aHandle->IsDoomed()) {
    rv = GetDoomedFile(getter_AddRefs(file));
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    rv = GetFile(aHandle->Hash(), getter_AddRefs(file));
    NS_ENSURE_SUCCESS(rv, rv);

    bool exists;
    if (NS_SUCCEEDED(file->Exists(&exists)) && exists) {
      NS_WARNING("Found a file that should not exist!");
    }
  }

  // TODO log
  rv = file->OpenNSPRFileDesc(PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE, 0600,
                              &aHandle->mFD);
  NS_ENSURE_SUCCESS(rv, rv);

  aHandle->mFileSize = 0;
  return NS_OK;
}

void
CacheFileIOManager::GetHashStr(const SHA1Sum::Hash *aHash, nsACString &_retval)
{
  _retval.Assign("");
  const char hexChars[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                           '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
  for (uint32_t i=0 ; i<sizeof(SHA1Sum::Hash) ; i++) {
    _retval.Append(hexChars[(*aHash)[i] >> 4]);
    _retval.Append(hexChars[(*aHash)[i] & 0xF]);
  }
}

nsresult
CacheFileIOManager::GetFile(const SHA1Sum::Hash *aHash, nsIFile **_retval)
{
  nsresult rv;
  nsCOMPtr<nsIFile> file;
  rv = mCacheDirectory->Clone(getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = file->AppendNative(NS_LITERAL_CSTRING("entries"));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString leafName;
  GetHashStr(aHash, leafName);

  rv = file->AppendNative(leafName);
  NS_ENSURE_SUCCESS(rv, rv);

  file.swap(*_retval);
  return NS_OK;
}

nsresult
CacheFileIOManager::GetDoomedFile(nsIFile **_retval)
{
  nsresult rv;
  nsCOMPtr<nsIFile> file;
  rv = mCacheDirectory->Clone(getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = file->AppendNative(NS_LITERAL_CSTRING("doomed"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = file->AppendNative(NS_LITERAL_CSTRING("dummyleaf"));
  NS_ENSURE_SUCCESS(rv, rv);

  srand(static_cast<unsigned>(PR_Now()));
  nsAutoCString leafName;
  uint32_t iter=0;
  while (true) {
    iter++;
    leafName.AppendInt(rand());
    rv = file->SetNativeLeafName(leafName);
    NS_ENSURE_SUCCESS(rv, rv);

    bool exists;
    if (NS_SUCCEEDED(file->Exists(&exists)) && !exists)
      break;

    leafName.Truncate();
  }

//  Telemetry::Accumulate(Telemetry::DISK_CACHE_GETDOOMEDFILE_ITERATIONS, iter);

  file.swap(*_retval);
  return NS_OK;
}

nsresult
CacheFileIOManager::CheckAndCreateDir(nsIFile *aFile, const char *aDir)
{
  nsresult rv;
  bool exists;

  nsCOMPtr<nsIFile> file;
  if (!aDir) {
    file = aFile;
  } else {
    nsAutoCString dir(aDir);
    rv = aFile->Clone(getter_AddRefs(file));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = file->AppendNative(dir);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = file->Exists(&exists);
  if (NS_SUCCEEDED(rv) && !exists)
    rv = file->Create(nsIFile::DIRECTORY_TYPE, 0700);
  if (NS_FAILED(rv)) {
    NS_WARNING("Cannot create directory");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
CacheFileIOManager::CreateCacheTree()
{
  MOZ_ASSERT(!mTreeCreated);

  nsresult rv;

  // ensure parent directory exists
  nsCOMPtr<nsIFile> parentDir;
  rv = mCacheDirectory->GetParent(getter_AddRefs(parentDir));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = CheckAndCreateDir(parentDir, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  // ensure cache directory exists
  rv = CheckAndCreateDir(mCacheDirectory, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  // ensure entries directory exists
  rv = CheckAndCreateDir(mCacheDirectory, "entries");
  NS_ENSURE_SUCCESS(rv, rv);

  // ensure doomed directory exists
  rv = CheckAndCreateDir(mCacheDirectory, "doomed");
  NS_ENSURE_SUCCESS(rv, rv);

  mTreeCreated = true;
  return NS_OK;
}

} // net
} // mozilla
