/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheEntry.h"
#include "CacheStorageService.h"
#include "CacheLog.h"

#include "nsIStorageStream.h"
#include "nsIOutputStream.h"
#include "nsIURI.h"
#include "nsICacheEntryOpenCallback.h"
#include "nsICacheStorage.h"

#include "nsComponentManagerUtils.h"
#include "nsString.h"
#include "nsProxyRelease.h"
#include <math.h>

namespace mozilla {
namespace net {

NS_IMPL_THREADSAFE_ISUPPORTS1(CacheEntry::Handle, nsICacheEntry)

// CacheEntry::Handle

CacheEntry::Handle::Handle(CacheEntry* aEntry)
: mEntry(aEntry)
{
  MOZ_COUNT_CTOR(CacheEntry::Handle);

  LOG(("New CacheEntry::Handle %p for entry %p", this, aEntry));
}

CacheEntry::Handle::~Handle()
{
  mEntry->OnWriterClosed(this);

  MOZ_COUNT_DTOR(CacheEntry::Handle);
}


// Short term hack
NS_IMPL_THREADSAFE_ISUPPORTS1(CacheEntry::OutputStreamHook, nsIOutputStream)


// CacheEntry

NS_IMPL_THREADSAFE_ISUPPORTS2(CacheEntry, nsICacheEntry, nsIRunnable)

CacheEntry::CacheEntry(const nsACString& aStorageID,
                       nsIURI* aURI,
                       const nsACString& aEnhanceID,
                       bool aUseDisk)
: mReportedMemorySize(0)
, mFrecency(0)
, mSortingExpirationTime(uint32_t(-1))
, mLock("CacheEntry")
, mURI(aURI)
, mEnhanceID(aEnhanceID)
, mStorageID(aStorageID)
, mUseDisk(aUseDisk)
, mIsLoading(false)
, mIsLoaded(false)
, mIsReady(false)
, mIsWriting(false)
, mIsDoomed(false)
, mIsRegistered(false)
, mIsRegistrationAllowed(true)
, mFetchCount(0)
, mLastFetched(0)
, mLastModified(0)
, mExpirationTime(0)
, mPredictedDataSize(0)
, mDataSize(0)
, mMetadataMemoryOccupation(0)
{
  MOZ_COUNT_CTOR(CacheEntry);
  mMetadata.Init();

  mService = CacheStorageService::Self();

  CacheStorageService::Self()->RecordMemoryOnlyEntry(
    this, !aUseDisk, true /* overwrite */);
}

CacheEntry::~CacheEntry()
{
  ProxyReleaseMainThread(mURI);

  LOG(("CacheEntry::~CacheEntry [this=%p]", this));
  MOZ_COUNT_DTOR(CacheEntry);
}

void CacheEntry::Load()
{
  LOG(("CacheEntry::Load [this=%p]", this));

  {
    mozilla::MutexAutoLock lock(mLock);
    mIsLoading = true;
  }

  BackgroundOp(Ops::LOAD);
}

void CacheEntry::OnLoaded()
{
  LOG(("CacheEntry::OnLoaded [this=%p]", this));

  {
    mozilla::MutexAutoLock lock(mLock);
    mIsLoaded = true;
    mIsLoading = false;
  }

  InvokeCallbacks();
}

void CacheEntry::AsyncOpen(nsICacheEntryOpenCallback* aCallback, uint32_t aFlags)
{
  LOG(("CacheEntry::AsyncOpen [this=%p, callback=%p]", this, aCallback));
  LOG(("  ready=%d, loaded=%d, writing=%d", (bool)mIsReady, (bool)mIsLoaded, (bool)mIsWriting));

  // Call to this methods means a demand to access this entry by a single consumer.
  // Thus, update frecency.

  MOZ_ASSERT(!(aFlags & nsICacheStorage::OPEN_TRUNCATE) || !(aFlags & nsICacheStorage::OPEN_READONLY));

  bool readonly = aFlags & nsICacheStorage::OPEN_READONLY;
  bool loaded, loading, first;
  {
    mozilla::MutexAutoLock lock(mLock);

    first = !mIsLoaded && !mIsLoading;

    if (aFlags & nsICacheStorage::OPEN_TRUNCATE) {
      mIsLoaded = true;
      mIsReady = false;

      if (mUseDisk) {
        // TODO
        // - async truncate disk file
      }
    }
    else if (!mIsLoading && !mIsLoaded && !mUseDisk) {
      // This is just an inmemory entry, nothing to load from file
      // Just in case this entry has first been searched in disk storage
      // and later someone opens it in memory storage, we must not set the
      // flag since it races with file currently being loaded from disk.
      mIsLoaded = true;
    }

    loaded = mIsLoaded;
    loading = mIsLoading;
  }

  if (first) {
    BackgroundOp(Ops::REGISTER);
  }

  if (!loaded) {
    RememberCallback(aCallback, readonly);
    if (!loading)
      Load();

    return;
  }

  bool called = InvokeCallback(aCallback, readonly);
  if (!called) {
    RememberCallback(aCallback, readonly);
  }
}

void CacheEntry::RememberCallback(nsICacheEntryOpenCallback* aCallback,
                                  bool aReadOnly)
{
  LOG(("CacheEntry::RememberCallback [this=%p, cb=%p]", this, aCallback));

  mozilla::MutexAutoLock lock(mLock);
  if (!aReadOnly)
    mCallbacks.AppendObject(aCallback);
  else
    mReadOnlyCallbacks.AppendObject(aCallback);
}

void CacheEntry::InvokeCallbacks()
{
  LOG(("CacheEntry::InvokeCallbacks START [this=%p]", this));

  mozilla::MutexAutoLock lock(mLock);

  while (mCallbacks.Count()) {
    nsCOMPtr<nsICacheEntryOpenCallback> callback = mCallbacks[0];
    mCallbacks.RemoveElementAt(0);

    {
      mozilla::MutexAutoUnlock unlock(mLock);
      InvokeCallback(callback, false);
    }

    if (!mIsReady) {
      // Stop invoking other callbacks since one of them
      // is now writing to the entry.  No consumers should
      // get this entry until metadata are filled with
      // values downloaded from the server.
      LOG(("CacheEntry::InvokeCallbacks DONE (not ready) [this=%p]", this));
      return;
    }
  }

  while (mReadOnlyCallbacks.Count()) {
    nsCOMPtr<nsICacheEntryOpenCallback> callback = mReadOnlyCallbacks[0];

    {
      mozilla::MutexAutoUnlock unlock(mLock);
      if (!InvokeCallback(callback, true)) {
        // Didn't trigger, so we must stop
        break;
      }
    }

    mReadOnlyCallbacks.RemoveElement(callback);
  }

  LOG(("CacheEntry::InvokeCallbacks DONE [this=%p]", this));
}

bool CacheEntry::InvokeCallback(nsICacheEntryOpenCallback* aCallback,
                                bool aReadOnly)
{
  LOG(("CacheEntry::InvokeCallback [this=%p, cb=%p]", this, aCallback));
  // When we are here, the entry must be loaded from disk
  MOZ_ASSERT(mIsLoaded);
  MOZ_ASSERT(!mIsLoading);

  bool ready, writing, doomed;
  {
    mozilla::MutexAutoLock lock(mLock);
    LOG(("  ready=%d, loaded=%d, writing=%d, doomed=%d",
      (bool)mIsReady, (bool)mIsLoaded, (bool)mIsWriting, (bool)mIsDoomed));

    ready = mIsReady;
    writing = mIsWriting;
    doomed = mIsDoomed;
  }

  if (ready && !doomed) {
    // Metadata present, validate the entry
    bool isValid;
    nsresult rv = aCallback->OnCacheEntryCheck(this, nullptr, &isValid);
    LOG(("  OnCacheEntryCheck result: rv=0x%08x, valid=%d", rv, isValid));

    if ((NS_FAILED(rv) || !isValid) && !aReadOnly) {
      LOG(("  replacing entry %p", this));
      nsRefPtr<CacheEntry> newEntry;

      // The following call dooms this (current) entry and creates a new one for the
      // URL in the same storage.  We then transfer callbacks to that new entry.
      // NOTE: dooming _posts_ InvokeCallbacks() on this entry so that this code
      // is not reentered.
      rv = CacheStorageService::Self()->AddStorageEntry(
        GetStorageID(), GetURI(), GetEnhanceID(),
        mUseDisk,
        true, // always create
        true, // truncate existing (this one)
        getter_AddRefs(newEntry));

      if (NS_SUCCEEDED(rv)) {
        newEntry->AsyncOpen(aCallback, nsICacheStorage::OPEN_TRUNCATE);

        nsCOMArray<nsICacheEntryOpenCallback> callbacks, readOnlyCallbacks;
        {
          mozilla::MutexAutoLock lock(mLock);
          mCallbacks.SwapElements(callbacks);
          mReadOnlyCallbacks.SwapElements(readOnlyCallbacks);
        }
        newEntry->TransferCallbacks(callbacks, readOnlyCallbacks);
      }

      // aCallback transfered to the new entry
      if (NS_SUCCEEDED(rv))
        return true;
    }
  }
  else if (writing) {
    // Not ready and writing, don't let others interfer.
    LOG(("  entry is being written, callback bypassed"));
    return false;
  }

  InvokeAvailableCallback(aCallback, aReadOnly);
  return true;
}

void CacheEntry::InvokeAvailableCallback(nsICacheEntryOpenCallback* aCallback,
                                         bool aReadOnly)
{
  // ifdef log and debug
  {
    mozilla::MutexAutoLock lock(mLock);
    LOG(("CacheEntry::InvokeAvailableCallback [this=%p, cb=%p, ready=%d, r/o=%d]",
      this, aCallback, (bool)mIsReady, aReadOnly));

    // When we are here, the entry must be loaded from disk
    MOZ_ASSERT(mIsLoaded);
  }

  if (!NS_IsMainThread()) {
    // Must happen on the main thread :(
    nsRefPtr<AvailableCallbackRunnable> event =
      new AvailableCallbackRunnable(this, aCallback, aReadOnly);
    NS_DispatchToMainThread(event);
    return;
  }


  bool ready, doomed;
  {
    mozilla::MutexAutoLock lock(mLock);
    ++mFetchCount;
    mLastFetched = uint32_t(PR_Now() / int64_t(PR_USEC_PER_SEC));

    ready = mIsReady;
    doomed = mIsDoomed;
  }

  if (doomed) {
    LOG(("  doomed, notifying OCEA with NS_ERROR_CACHE_KEY_NOT_FOUND"));
    aCallback->OnCacheEntryAvailable(nullptr, false, nullptr, NS_ERROR_CACHE_KEY_NOT_FOUND);
    return;
  }

  if (ready) {
    LOG(("  ready, notifying OCEA with entry and NS_OK"));
    BackgroundOp(Ops::FRECENCYUPDATE);
    aCallback->OnCacheEntryAvailable(this, false, nullptr, NS_OK);
    return;
  }

  if (aReadOnly) {
    LOG(("  r/o and not ready, notifying OCEA with NS_ERROR_CACHE_KEY_NOT_FOUND"));
    aCallback->OnCacheEntryAvailable(nullptr, false, nullptr, NS_ERROR_CACHE_KEY_NOT_FOUND);
    return;
  }

  // This is a new entry and needs to be fetched first.
  // The Handle blocks other consumers until the channel
  // either releases the entry or marks metadata as filled.

  // Consumer will be responsible to fill the entry metadata and data.
  {
    mozilla::MutexAutoLock lock(mLock);
    mIsWriting = true;
  }

  BackgroundOp(Ops::FRECENCYUPDATE);
  nsRefPtr<Handle> handle = new Handle(this);
  nsresult rv = aCallback->OnCacheEntryAvailable(handle, true, nullptr, NS_OK);

  if (NS_FAILED(rv)) {
    LOG(("  writing failed (0x%08x)", rv));

    // Consumer given a new entry failed to take care of the entry.
    OnWriterClosed(handle);
    return;
  }

  // TODO - here is the place to start opening the file, but maybe we can
  // do it even sooner... failures of OnCacheEntryAvailable are IMO uncommon.
  LOG(("  writing"));
}

void CacheEntry::OnWriterClosed(Handle const* aHandle)
{
  LOG(("CacheEntry::OnWriterClosed [this=%p, handle=%p]", this, aHandle));

  {
    BackgroundOp(Ops::REPORTUSAGE);

    mozilla::MutexAutoLock lock(mLock);

    LOG(("  ready=%d, loaded=%d, writing=%d", (bool)mIsReady, (bool)mIsLoaded, (bool)mIsWriting));
    if (!mIsWriting)
      return;

    mIsWriting = false;
  }

  InvokeCallbacks();
}

void CacheEntry::TransferCallbacks(nsCOMArray<nsICacheEntryOpenCallback> const &aCallbacks,
                                   nsCOMArray<nsICacheEntryOpenCallback> const &aReadOnlyCallbacks)
{
  bool invoke;
  {
    mozilla::MutexAutoLock lock(mLock);
    LOG(("CacheEntry::TransferCallbacks [entry=%p, %d, %d, %d, %d]",
      this, mCallbacks.Length(), mReadOnlyCallbacks.Length(), aCallbacks.Length(), aReadOnlyCallbacks.Length()));

    mCallbacks.AppendObjects(aCallbacks);
    mReadOnlyCallbacks.AppendObjects(aReadOnlyCallbacks);

    invoke = aCallbacks.Length() || aReadOnlyCallbacks.Length();
  }

  if (invoke)
    BackgroundOp(Ops::CALLBACKS, true);
}

bool CacheEntry::UsingDisk() const
{
  CacheStorageService::Self()->Lock().AssertCurrentThreadOwns();

  return mUseDisk;
}

bool CacheEntry::SetUsingDisk(bool aUsingDisk)
{
  CacheStorageService::Self()->Lock().AssertCurrentThreadOwns();

  bool changed = mUseDisk != aUsingDisk;
  mUseDisk = aUsingDisk;
  return changed;
}

uint32_t CacheEntry::GetMetadataMemoryOccupation() const
{
  CacheEntry* this_non_const = const_cast<CacheEntry*>(this);
  mozilla::MutexAutoLock lock(this_non_const->mLock);
  return mMetadataMemoryOccupation;
}

uint32_t CacheEntry::GetDataMemoryOccupation() const
{
  uint32_t size;
  CacheEntry* this_non_const = const_cast<CacheEntry*>(this);

  // TODO
  nsresult rv = this_non_const->GetDataSize(&size);
  if (NS_FAILED(rv))
    return 0;

  return size;
}

// nsICacheEntry

NS_IMETHODIMP CacheEntry::GetPersistToDisk(bool *aPersistToDisk)
{
  mozilla::MutexAutoLock lock(CacheStorageService::Self()->Lock());

  *aPersistToDisk = mUseDisk;
  return NS_OK;
}
NS_IMETHODIMP CacheEntry::SetPersistToDisk(bool aPersistToDisk)
{
  mozilla::MutexAutoLock lock(CacheStorageService::Self()->Lock());

  mUseDisk = aPersistToDisk;
  CacheStorageService::Self()->RecordMemoryOnlyEntry(
    this, !aPersistToDisk, false /* don't overwrite */);

  return NS_OK;
}

NS_IMETHODIMP CacheEntry::GetKey(nsACString & aKey)
{
  return mURI->GetAsciiSpec(aKey);
}

NS_IMETHODIMP CacheEntry::GetFetchCount(int32_t *aFetchCount)
{
  *aFetchCount = mFetchCount;
  return NS_OK;
}

NS_IMETHODIMP CacheEntry::GetLastFetched(uint32_t *aLastFetched)
{
  *aLastFetched = mLastFetched;
  return NS_OK;
}

NS_IMETHODIMP CacheEntry::GetLastModified(uint32_t *aLastModified)
{
  *aLastModified = mLastModified;
  return NS_OK;
}

NS_IMETHODIMP CacheEntry::GetExpirationTime(uint32_t *aExpirationTime)
{
  mozilla::MutexAutoLock lock(mLock);
  *aExpirationTime = mExpirationTime;
  return NS_OK;
}

NS_IMETHODIMP CacheEntry::SetExpirationTime(uint32_t expirationTime)
{
  {
    mozilla::MutexAutoLock lock(mLock);
    mExpirationTime = expirationTime;
  }

  BackgroundOp(Ops::EXPTIMEUPDATE);
  return NS_OK;
}

NS_IMETHODIMP CacheEntry::OpenInputStream(uint32_t offset, nsIInputStream * *_retval)
{
  LOG(("CacheEntry::OpenInputStream [this=%p]", this));

  mozilla::MutexAutoLock lock(mLock);

  if (mIsDoomed) {
    LOG(("  doomed..."));
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsresult rv;

  // HACK...
  if (!mStorageStream) {
    LOG(("  create storage stream (read)"));

    mStorageStream = do_CreateInstance("@mozilla.org/storagestream;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mStorageStream->Init(4096, (uint32_t) -1, nullptr);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = mStorageStream->NewInputStream(offset, _retval);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP CacheEntry::OpenOutputStream(uint32_t offset, nsIOutputStream * *_retval)
{
  LOG(("CacheEntry::OpenOutputStream [this=%p]", this));

  mozilla::MutexAutoLock lock(mLock);

  if (mIsDoomed) {
    LOG(("  doomed..."));
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsresult rv;

  // HACK...
  if (mStorageStream) {
    uint32_t dataSize;
    rv = mStorageStream->GetLength(&dataSize);
    NS_ENSURE_SUCCESS(rv, rv);

    if (offset == dataSize) {
      // stream will append (partial content)
      LOG(("  appending"));
    }
    else if (offset == 0) {
      // new data is about to be written
      // HACK - throw the current storage stream away, active read consumers
      // will use the old stream, new ones will use the newly created bellow
      mStorageStream = nullptr;
      // TODO? - file will just rewrite, no need for any special stuff
      LOG(("  rewriting"));
    }
    else {
      // We don't know anything else than rewrite from start or append :(
      // TODO ?
      LOG(("  NOT SUPPORTED - neither append nor rewrite, size=%u, offset=%u", dataSize, offset));
      MOZ_ASSERT(false);
      return NS_ERROR_NOT_IMPLEMENTED;
    }
  }

  if (!mStorageStream) {
    LOG(("  create storage stream"));

    mStorageStream = do_CreateInstance("@mozilla.org/storagestream;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mStorageStream->Init(4096, (uint32_t) -1, nullptr);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = mStorageStream->GetOutputStream(offset, getter_AddRefs(mOutputStream));
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ADDREF(*_retval = new OutputStreamHook(this));
  return NS_OK;
}

NS_IMETHODIMP CacheEntry::GetPredictedDataSize(int64_t *aPredictedDataSize)
{
  *aPredictedDataSize = mPredictedDataSize;
  return NS_OK;
}
NS_IMETHODIMP CacheEntry::SetPredictedDataSize(int64_t aPredictedDataSize)
{
  mPredictedDataSize = aPredictedDataSize;
  return NS_OK;
}

NS_IMETHODIMP CacheEntry::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
  mozilla::MutexAutoLock lock(mLock);
  NS_IF_ADDREF(*aSecurityInfo = mSecurityInfo);
  return NS_OK;
}
NS_IMETHODIMP CacheEntry::SetSecurityInfo(nsISupports *aSecurityInfo)
{
  mozilla::MutexAutoLock lock(mLock);
  mSecurityInfo = aSecurityInfo;
  return NS_OK;
}

NS_IMETHODIMP CacheEntry::GetStorageDataSize(uint32_t *aStorageDataSize)
{
  mozilla::MutexAutoLock lock(mLock);

  if (!mStorageStream)
    return NS_ERROR_NOT_AVAILABLE;

  mStorageStream->GetLength(aStorageDataSize);
  return NS_OK;
}

NS_IMETHODIMP CacheEntry::AsyncDoom(nsICacheEntryDoomCallback *aCallback)
{
  {
    mozilla::MutexAutoLock lock(mLock);

    if (mDoomCallback || mIsDoomed)
      return NS_ERROR_IN_PROGRESS;

    mIsDoomed = true;
    mDoomCallback = aCallback;
  }

  // Immediately remove the entry from the storage hash table
  CacheStorageService::Self()->RemoveEntry(this);

  BackgroundOp(Ops::DOOM);

  return NS_OK;
}

NS_IMETHODIMP CacheEntry::GetMetaDataElement(const char * key, char * *_retval)
{
  mozilla::MutexAutoLock lock(mLock);

  nsCString value;
  if (!mMetadata.Get(nsDependentCString(key), &value))
    return NS_ERROR_NOT_AVAILABLE;

  *_retval = NS_strdup(value.get());
  return NS_OK;
}

NS_IMETHODIMP CacheEntry::SetMetaDataElement(const char * key, const char * value)
{
  mozilla::MutexAutoLock lock(mLock);

  nsCString existingValue;
  if (!mMetadata.Get(nsDependentCString(key), &existingValue)) {
    if (!value) {
      return NS_OK;
    }

    mMetadataMemoryOccupation += strlen(key);
  }
  else {
    mMetadataMemoryOccupation -= existingValue.Length();
  }

  if (!value) {
    mMetadata.Remove(nsDependentCString(key));
    mMetadataMemoryOccupation -= strlen(key);
  }
  else {
    nsDependentCString newValue(value);
    mMetadataMemoryOccupation += newValue.Length();
    mMetadata.Put(nsDependentCString(key), newValue);
  }

  return NS_OK;
}

NS_IMETHODIMP CacheEntry::MetaDataReady()
{
  // Needs a lock..
  LOG(("CacheEntry::MetaDataReady [this=%p, ready=%d]", this, (bool)mIsReady));

  {
    mozilla::MutexAutoLock lock(mLock);
    if (mIsReady)
      return NS_OK;

    mIsReady = true;
    mIsWriting = false;
  }

  InvokeCallbacks();
  return NS_OK;
}

NS_IMETHODIMP CacheEntry::SetDataSize(uint32_t size)
{
  // ?
  mDataSize = size;
  return NS_OK;
}

NS_IMETHODIMP CacheEntry::GetDataSize(uint32_t *aDataSize)
{
  *aDataSize = 0;

  mozilla::MutexAutoLock lock(mLock);
  if (!mStorageStream)
    return NS_OK;

  if (mIsWriting)
    return NS_ERROR_IN_PROGRESS;

  // mayhemer: TODO Problem with compression
  return mStorageStream->GetLength(aDataSize);
}

NS_IMETHODIMP CacheEntry::MarkValid()
{
  // NOT IMPLEMENTED ACTUALLY
  return NS_OK;
}

NS_IMETHODIMP CacheEntry::Close()
{
  // NOT IMPLEMENTED ACTUALLY
  return NS_OK;
}

NS_IMETHODIMP CacheEntry::GetStoragePolicy(nsCacheStoragePolicy *aStoragePolicy)
{
  // NOT IMPLEMENTED ACTUALLY
  return NS_OK;
}
NS_IMETHODIMP CacheEntry::SetStoragePolicy(nsCacheStoragePolicy aStoragePolicy)
{
  // NOT IMPLEMENTED ACTUALLY
  return NS_OK;
}

// nsIRunnable

NS_IMETHODIMP CacheEntry::Run()
{
  MOZ_ASSERT(CacheStorageService::IsOnManagementThread());

  uint32_t ops;
  {
    mozilla::MutexAutoLock lock(mLock);
    ops = mBackgroundOperations.Grab();
  }

  BackgroundOp(ops);
  return NS_OK;
}

// Management methods

double CacheEntry::GetFrecency() const
{
  MOZ_ASSERT(CacheStorageService::IsOnManagementThread());
  return mFrecency;
}

uint32_t CacheEntry::GetExpirationTime() const
{
  MOZ_ASSERT(CacheStorageService::IsOnManagementThread());
  return mSortingExpirationTime;
}

uint32_t& CacheEntry::ReportedMemorySize()
{
  MOZ_ASSERT(CacheStorageService::IsOnManagementThread());
  return mReportedMemorySize;
}

bool CacheEntry::IsRegistered() const
{
  MOZ_ASSERT(CacheStorageService::IsOnManagementThread());
  return mIsRegistered;
}

bool CacheEntry::CanRegister() const
{
  MOZ_ASSERT(CacheStorageService::IsOnManagementThread());
  return !mIsRegistered && mIsRegistrationAllowed;
}

void CacheEntry::SetRegistered(bool aRegistered)
{
  MOZ_ASSERT(CacheStorageService::IsOnManagementThread());
  MOZ_ASSERT(mIsRegistrationAllowed);

  mIsRegistered = aRegistered;

  if (!aRegistered) // Never allow registration again
    mIsRegistrationAllowed = false;
}

bool CacheEntry::Purge(uint32_t aWhat)
{
  LOG(("CacheEntry::Purge [this=%p, what=%d]", this, aWhat));

  MOZ_ASSERT(CacheStorageService::IsOnManagementThread());

  {
    mozilla::MutexAutoLock lock(mLock);
    if (mIsWriting) {
      LOG(("  is writing"));
      return false;
    }

    switch (aWhat) {
    case PURGE_DATA_ONLY_DISK_BACKED:
    case PURGE_WHOLE_ONLY_DISK_BACKED:
      // This is an in-memory only entry, don't purge it
      if (!mUseDisk) {
        LOG(("  not using disk"));
        return false;
      }
    }
  }

  switch (aWhat) {
  case PURGE_WHOLE_ONLY_DISK_BACKED:
  case PURGE_WHOLE:
    {
      CacheStorageService::Self()->UnregisterEntry(this);
      CacheStorageService::Self()->RemoveEntry(this);

      CacheStorageService::Self()->OnMemoryConsumptionChange(this, 0);

      // Entry removed it self from control arrays, return true
      return true;
    }

  case PURGE_DATA_ONLY_DISK_BACKED:
    {
      uint32_t metadataSize;

      {
        mozilla::MutexAutoLock lock(mLock);
        // HACK - throw the stream away
        mStorageStream = nullptr;
        mOutputStream = nullptr;

        metadataSize = mMetadataMemoryOccupation;
      }

      CacheStorageService::Self()->OnMemoryConsumptionChange(this, metadataSize);

      // Entry has been left in control arrays, return false (not purged)
      return false;
    }
  }

  LOG(("  ?"));
  return false;
}

void CacheEntry::PurgeAndDoom()
{
  LOG(("CacheEntry::PurgeAndDoom [this=%p]", this));

  MOZ_ASSERT(CacheStorageService::IsOnManagementThread());

  CacheStorageService::Self()->RemoveEntry(this);
  DoomAlreadyRemoved();
}

void CacheEntry::DoomAlreadyRemoved()
{
  LOG(("CacheEntry::DoomAlreadyRemoved [this=%p]", this));

  {
    mozilla::MutexAutoLock lock(mLock);
    mIsDoomed = true;
  }

  if (!CacheStorageService::IsOnManagementThread()) {
    BackgroundOp(Ops::DOOM);
    return;
  }

  CacheStorageService::Self()->UnregisterEntry(this);
  CacheStorageService::Self()->OnMemoryConsumptionChange(this, 0);

  nsCOMPtr<nsICacheEntryDoomCallback> callback;
  bool invokeCallbacks;
  {
    mozilla::MutexAutoLock lock(mLock);

    // HACK - throw the stream away
    mStorageStream = nullptr;
    // TODO
    // - release buffers
    // - doom the file

    invokeCallbacks = mCallbacks.Length() || mReadOnlyCallbacks.Length();
    mDoomCallback.swap(callback);
  }

  if (invokeCallbacks) {
    // Must force post here since may be indirectly called from
    // InvokeCallback of this entry and we don't want reentrancy here.
    BackgroundOp(Ops::CALLBACKS, true);
  }

  if (callback) {
    nsRefPtr<DoomCallbackRunnable> event =
      new DoomCallbackRunnable(callback, NS_OK);
    NS_DispatchToMainThread(event);
  }
}

void CacheEntry::BackgroundOp(uint32_t aOperations, bool aForceAsync)
{
  if (!CacheStorageService::IsOnManagementThread() || aForceAsync) {
    mozilla::MutexAutoLock lock(mLock);

    if (mBackgroundOperations.Set(aOperations))
      CacheStorageService::Self()->Dispatch(this);

    LOG(("CacheEntry::BackgroundOp this=%p dipatch of %x", this, aOperations));
    return;
  }

  MOZ_ASSERT(CacheStorageService::IsOnManagementThread());

  if (aOperations & Ops::FRECENCYUPDATE) {
    #ifndef M_LN2
    #define M_LN2 0.69314718055994530942
    #endif

    // Half-life is 90 days.
    static double const half_life = 90.0 * (24 * 60 * 60);
    // Must convert from seconds to milliseconds since PR_Now() gives usecs.
    static double const decay = (M_LN2 / half_life) / static_cast<double>(PR_USEC_PER_SEC);

    double now_decay = static_cast<double>(PR_Now()) * decay;

    if (mFrecency == 0) {
      mFrecency = now_decay;
    }
    else {
      // TODO: when C++11 enabled, use std::log1p(n) which is equal to log(n + 1) but
      // more precise.
      mFrecency = log(exp(mFrecency - now_decay) + 1) + now_decay;
    }
    LOG(("CacheEntry FRECENCYUPDATE [this=%p, frecency=%1.10f]", this, mFrecency));
  }

  if (aOperations & Ops::EXPTIMEUPDATE) {
    mozilla::MutexAutoLock lock(mLock);
    mSortingExpirationTime = mExpirationTime;
    LOG(("CacheEntry EXPTIMEUPDATE [this=%p, exptime=%u (now=%u)]",
      this, mSortingExpirationTime, NowInSeconds()));
  }

  if (aOperations & Ops::REGISTER) {
    LOG(("CacheEntry REGISTER [this=%p]", this));

    CacheStorageService::Self()->RegisterEntry(this);
  }

  if (aOperations & Ops::LOAD) {
    LOG(("CacheEntry LOAD [this=%p]", this));

    // HACK
    // Nothing to load (metadata not ready), this is only an inmemory hack
    // TODO Demand the file here?
    {
      mozilla::MutexAutoLock lock(mLock);
      mIsReady = false;
    }

    BackgroundOp(Ops::LOADED, true);
  }

  if (aOperations & Ops::LOADED) {
    LOG(("CacheEntry LOADED [this=%p]", this));

    OnLoaded();
  }

  if (aOperations & Ops::REPORTUSAGE) {
    LOG(("CacheEntry REPORTUSAGE [this=%p]", this));

    uint32_t memorySize;

    // Should get only what is actually consumed in memory..
    // TODO - definitely more here to do
    nsresult rv = GetStorageDataSize(&memorySize);
    if (NS_FAILED(rv))
      memorySize = 0;

    {
      mozilla::MutexAutoLock lock(mLock);
      memorySize += mMetadataMemoryOccupation;
    }

    CacheStorageService::Self()->OnMemoryConsumptionChange(this, memorySize);
  }

  if (aOperations & Ops::DOOM) {
    LOG(("CacheEntry DOOM [this=%p]", this));

    DoomAlreadyRemoved();
  }

  if (aOperations & Ops::CALLBACKS) {
    LOG(("CacheEntry CALLBACKS (invoke) [this=%p]", this));

    InvokeCallbacks();
  }
}

} // net
} // mozilla
