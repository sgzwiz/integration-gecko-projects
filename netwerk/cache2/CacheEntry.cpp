/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheEntry.h"
#include "CacheStorageService.h"
#include "CacheLog.h"

#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsISeekableStream.h"
#include "nsIURI.h"
#include "nsICacheEntryOpenCallback.h"
#include "nsICacheStorage.h"
#include "nsISerializable.h"
#include "nsIStreamTransportService.h"
#include "nsIPipe.h"

#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsProxyRelease.h"
#include "nsSerializationHelper.h"
#include "nsStreamUtils.h"
#include <math.h>

namespace mozilla {
namespace net {

static uint32_t const ENTRY_NOT_VALID = nsICacheEntryOpenCallback::ENTRY_NOT_VALID;
static uint32_t const ENTRY_VALID = nsICacheEntryOpenCallback::ENTRY_VALID;
static uint32_t const ENTRY_NEEDS_REVALIDATION = nsICacheEntryOpenCallback::ENTRY_NEEDS_REVALIDATION;

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

// CacheEntry

NS_IMPL_THREADSAFE_ISUPPORTS3(CacheEntry,
                              nsICacheEntry,
                              nsIRunnable,
                              CacheFileListener)

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
, mIsDoomed(false)
, mSecurityInfoLoaded(false)
, mPreventCallbacks(false)
, mIsRegistered(false)
, mIsRegistrationAllowed(true)
, mState(NOTLOADED)
, mPredictedDataSize(0)
, mDataSize(0)
, mMetadataMemoryOccupation(0)
{
  MOZ_COUNT_CTOR(CacheEntry);

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

#ifdef MOZ_LOGGING

char const * CacheEntry::StateString(uint32_t aState)
{
  switch (aState) {
  case NOTLOADED:     return "NOTLOADED";
  case LOADING:       return "LOADING";
  case EMPTY:         return "EMPTY";
  case WRITING:       return "WRITING";
  case HASMETADATA:   return "HASMETADATA";
  case READY:         return "READY";
  case REVALIDATING:  return "REVALIDATING";
  }

  return "?";
}

#endif

nsresult CacheEntry::HashingKeyWithStorage(nsACString &aResult)
{
  return HashingKey(mStorageID, mEnhanceID, mURI, aResult);
}

nsresult CacheEntry::HashingKey(nsACString &aResult)
{
  return HashingKey(EmptyCString(), mEnhanceID, mURI, aResult);
}

// static
nsresult CacheEntry::HashingKey(nsCSubstring const& aStorageID,
                                nsCSubstring const& aEnhanceID,
                                nsIURI* aURI,
                                nsACString &aResult)
{
  /**
   * This key is used to salt hash that is a base for disk file name.
   * Changing it will cause we will not be able to find files on disk.
   */

  if (aStorageID.Length()) {
    aResult.Append(aStorageID);
    aResult.Append(':');
  }

  if (aEnhanceID.Length()) {
    aResult.Append(aEnhanceID);
    aResult.Append(':');
  }

  nsAutoCString spec;
  nsresult rv = aURI->GetAsciiSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  aResult.Append(spec);

  return NS_OK;
}

bool CacheEntry::Load(bool aTruncate)
{
  if (mState > LOADING)
    return false;
  if (mState == LOADING)
    return true;

  LOG(("CacheEntry::Load [this=%p, trunc=%d]", this, aTruncate));

  nsRefPtr<CacheFile> file;
  {
    mozilla::MutexAutoLock lock(mLock);

    // Do the same check under the lock to preserve full load
    // logic atomicity.  It would be very unfortunate to load
    // a cache entry twice.
    if (mState > LOADING) {
      LOG(("  already loaded"));
      return false;
    }

    if (mState == LOADING) {
      LOG(("  already loading"));
      return true;
    }

    MOZ_ASSERT(mState == NOTLOADED);
    MOZ_ASSERT(!mFile);

    if (aTruncate || !mUseDisk) {
      // Just fake the load has already been done as "new".
      mState = EMPTY;
    }
    else {
      mState = LOADING;
    }

    mFile = new CacheFile();
    file = mFile;
  }

  BackgroundOp(Ops::REGISTER);

  nsresult rv;

  nsAutoCString fileKey;
  rv = HashingKeyWithStorage(fileKey);

  LOG(("  performing load"));
  if (NS_SUCCEEDED(rv))
    rv = file->Init(fileKey,
                    aTruncate,
                    !mUseDisk,
                    (aTruncate || !mUseDisk) ? nullptr : this);

  if (NS_FAILED(rv)) {
    AsyncDoom(nullptr);
    return false;
  }

  return mState == LOADING;
}

NS_IMETHODIMP CacheEntry::OnFileReady(nsresult aResult, bool aIsNew)
{
  LOG(("CacheEntry::OnFileReady [this=%p, rv=0x%08x, new=%d]",
      this, aResult, aIsNew));

  // OnFileReady, that is the only code that can transit from LOADING
  // to any follow-on state, can only be invoked ones on an entry,
  // thus no need to lock.  Until this moment there is no consumer that
  // could manipulate the entry state.
  MOZ_ASSERT(mState == LOADING);

  mState = aIsNew ? EMPTY : READY;

  if (NS_FAILED(aResult))
    AsyncDoom(nullptr);

  InvokeCallbacks();
  return NS_OK;
}

NS_IMETHODIMP CacheEntry::OnFileDoomed(nsresult aResult)
{
  nsCOMPtr<nsICacheEntryDoomCallback> callback;
  {
    mozilla::MutexAutoLock lock(mLock);
    mDoomCallback.swap(callback);
  }

  if (callback) {
    nsRefPtr<DoomCallbackRunnable> event =
      new DoomCallbackRunnable(callback, NS_OK);
    NS_DispatchToMainThread(event);
  }

  return NS_OK;
}

void CacheEntry::AsyncOpen(nsICacheEntryOpenCallback* aCallback, uint32_t aFlags)
{
  LOG(("CacheEntry::AsyncOpen [this=%p, state=%s, flags=%d, callback=%p]",
    this, StateString(mState), aFlags, aCallback));

  bool readonly = aFlags & nsICacheStorage::OPEN_READONLY;
  bool truncate = aFlags & nsICacheStorage::OPEN_TRUNCATE;

  MOZ_ASSERT(!readonly || !truncate, "Bad flags combination");
  MOZ_ASSERT(!(truncate && mState > LOADING), "Must not call truncate on already loaded entry");

  if (Load(truncate) || !InvokeCallback(aCallback, readonly)) {
    // Load in progress or callback bypassed...
    RememberCallback(aCallback, readonly);
  }
}

already_AddRefed<CacheEntry> CacheEntry::ReopenTruncated(nsICacheEntryOpenCallback* aCallback)
{
  LOG(("CacheEntry::ReopenTruncated [this=%p]", this));
  mozilla::MutexAutoLock lock(mLock);

  // Hold callbacks invocation, AddStorageEntry would invoke from doom prematurly
  mPreventCallbacks = true;

  nsRefPtr<CacheEntry> newEntry;
  {
    mozilla::MutexAutoUnlock unlock(mLock);

    // The following call dooms this entry (calls DoomAlreadyRemoved on us)
    nsresult rv = CacheStorageService::Self()->AddStorageEntry(
      GetStorageID(), GetURI(), GetEnhanceID(),
      mUseDisk,
      true, // always create
      true, // truncate existing (this one)
      getter_AddRefs(newEntry));

    LOG(("  exchanged entry %p by entry %p, rv=0x%08x", this, newEntry.get(), rv));

    if (NS_SUCCEEDED(rv)) {
      newEntry->AsyncOpen(aCallback, nsICacheStorage::OPEN_TRUNCATE);
    }
    else {
      AsyncDoom(nullptr);
    }
  }

  mPreventCallbacks = false;

  if (!newEntry)
    return nullptr;

  newEntry->TransferCallbacks(mCallbacks, mReadOnlyCallbacks);
  mCallbacks.Clear();
  mReadOnlyCallbacks.Clear();
  return newEntry.forget();
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

    invoke = mCallbacks.Length() || mReadOnlyCallbacks.Length();
  }

  if (invoke)
    BackgroundOp(Ops::CALLBACKS, true);
}

void CacheEntry::RememberCallback(nsICacheEntryOpenCallback* aCallback,
                                  bool aReadOnly)
{
  // AsyncOpen can be called w/o a callback reference (when this is a new/truncated entry)
  if (!aCallback)
    return;

  LOG(("CacheEntry::RememberCallback [this=%p, cb=%p]", this, aCallback));

  mozilla::MutexAutoLock lock(mLock);
  if (!aReadOnly)
    mCallbacks.AppendObject(aCallback);
  else
    mReadOnlyCallbacks.AppendObject(aCallback);
}

void CacheEntry::InvokeCallbacks()
{
  LOG(("CacheEntry::InvokeCallbacks BEGIN [this=%p]", this));

  mozilla::MutexAutoLock lock(mLock);

  do {
    if (mPreventCallbacks) {
      LOG(("CacheEntry::InvokeCallbacks END callbacks prevented!"));
      return;
    }

    if (!mCallbacks.Count()) {
      LOG(("  no r/w callbacks"));
      break;
    }

    nsCOMPtr<nsICacheEntryOpenCallback> callback = mCallbacks[0];
    mCallbacks.RemoveElementAt(0);

    bool called;
    {
      mozilla::MutexAutoUnlock unlock(mLock);
      called = InvokeCallback(callback, false);
    }

    if (!called) {
      mCallbacks.InsertElementAt(0, callback);
      LOG(("CacheEntry::InvokeCallbacks END callback bypassed"));
      return;
    }
  } while (true);

  while (mReadOnlyCallbacks.Count()) {
    nsCOMPtr<nsICacheEntryOpenCallback> callback = mReadOnlyCallbacks[0];
    mReadOnlyCallbacks.RemoveElementAt(0);

    bool called;
    {
      mozilla::MutexAutoUnlock unlock(mLock);
      called = InvokeCallback(callback, true);
    }

    if (!called) {
      // Didn't trigger, so we must stop
      mReadOnlyCallbacks.InsertElementAt(0, callback);
      break;
    }
  }

  LOG(("CacheEntry::InvokeCallbacks END [this=%p]", this));
}

bool CacheEntry::InvokeCallback(nsICacheEntryOpenCallback* aCallback,
                                bool aReadOnly)
{
  LOG(("CacheEntry::InvokeCallback [this=%p, state=%s, cb=%p]", this, StateString(mState), aCallback));

  if (!mIsDoomed) {
    mozilla::MutexAutoLock lock(mLock);

    // When we are here, the entry must be loaded from disk
    MOZ_ASSERT(mState > LOADING);

    if (mState == WRITING || mState == HASMETADATA || mState == REVALIDATING) {
      // Prevent invoking other callbacks since one of them is now writing
      // or revalidating this entry.  No consumers should get this entry
      // until metadata are filled with values downloaded from the server
      // or the entry revalidated and output stream has been opened.
      LOG(("  entry is being written/revalidated, callback bypassed"));
      return false;
    }

    if (!aReadOnly) {
      if (mState == EMPTY) {
        // Advance to writing state, we expect to invoke the callback and let
        // it fill content of this entry.  Must set and check the state here
        // to prevent more then one
        mState = WRITING;
        LOG(("  advancing to WRITING state"));
      }

      if (!aCallback) {
        // We can be given no callback only in case of recreate, it is ok
        // to advance to WRITING state since the caller of recreate is expected
        // to write this entry now.
        return true;
      }

      if (mState == READY) {
        // Metadata present, validate the entry
        uint32_t validityState;
        {
          mozilla::MutexAutoUnlock unlock(mLock);

          nsresult rv = aCallback->OnCacheEntryCheck(this, nullptr, &validityState);
          LOG(("  OnCacheEntryCheck result: rv=0x%08x, validity=%d", rv, validityState));

          if (NS_FAILED(rv))
            validityState = ENTRY_NOT_VALID;
        }

        switch (validityState) {
        case ENTRY_NOT_VALID:
        {
          mozilla::MutexAutoUnlock unlock(mLock);

          // Entry found not valid, break callback execution.  This result
          // causes this entry replacement with a new truncated one.
          nsRefPtr<CacheEntry> newEntry = ReopenTruncated(aCallback);
          if (newEntry) {
            // This new entry now grabbed all our callbacks, indicate callback
            // had been invoked.
            return true;
          }
          break;
        }

        case ENTRY_NEEDS_REVALIDATION:
          LOG(("  will be holding callbacks until entry is revalidated"));
          // State is READY now and from that state entry cannot transit to any other
          // state then REVALIDATING for which cocurrency is not an issue.  No need
          // to lock here.
          mState = REVALIDATING;
          // NO BREAK

        case ENTRY_VALID:
          // Nothing more to do here, proceed to callback...
          break;
        }
      }
    }
  }

  if (aCallback)
    InvokeAvailableCallback(aCallback, aReadOnly);

  return true;
}

void CacheEntry::InvokeAvailableCallback(nsICacheEntryOpenCallback* aCallback,
                                         bool aReadOnly)
{
  LOG(("CacheEntry::InvokeAvailableCallback [this=%p, state=%s, cb=%p, r/o=%d]",
    this, StateString(mState), aCallback, aReadOnly));

  if (!NS_IsMainThread()) {
    // Must happen on the main thread :(
    nsRefPtr<AvailableCallbackRunnable> event =
      new AvailableCallbackRunnable(this, aCallback, aReadOnly);
    NS_DispatchToMainThread(event);
    return;
  }

  // This happens only on the main thread / :( /

  uint32_t const state = mState;

  // When we are here, the entry must be loaded from disk
  MOZ_ASSERT(state > LOADING);

  if (mIsDoomed) {
    LOG(("  doomed, notifying OCEA with NS_ERROR_CACHE_KEY_NOT_FOUND"));
    aCallback->OnCacheEntryAvailable(nullptr, false, nullptr, NS_ERROR_CACHE_KEY_NOT_FOUND);
    return;
  }

  if (state == READY) {
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

  // This is a new or potentially non-valid entry and needs to be fetched first.
  // The Handle blocks other consumers until the channel
  // either releases the entry or marks metadata as filled or whole entry valid,
  // i.e. until SetValid() on the entry is called.

  // Consumer will be responsible to fill or validate the entry metadata and data.

  BackgroundOp(Ops::FRECENCYUPDATE);

  nsRefPtr<Handle> handle = new Handle(this);
  nsresult rv = aCallback->OnCacheEntryAvailable(handle, state == WRITING, nullptr, NS_OK);

  if (NS_FAILED(rv)) {
    LOG(("  writing/revalidating failed (0x%08x)", rv));

    // Consumer given a new entry failed to take care of the entry.
    OnWriterClosed(handle);
    return;
  }

  LOG(("  writing/revalidating"));
}

void CacheEntry::OnWriterClosed(Handle const* aHandle)
{
  LOG(("CacheEntry::OnWriterClosed [this=%p, state=%s, handle=%p]", this, StateString(mState), aHandle));

  // There can only be one writer at a time (i.e. only a single consumer that
  // could manipulate with the state), so no need to lock here.
  if (mState == WRITING) {
    LOG(("  reverting to state EMPTY - write failed"));
    mState = EMPTY;
  }

  BackgroundOp(Ops::REPORTUSAGE);
  InvokeCallbacks();
}

already_AddRefed<CacheFile> CacheEntry::File()
{
  mozilla::MutexAutoLock lock(mLock);
  nsRefPtr<CacheFile> file(mFile);
  return file.forget();
}

bool CacheEntry::UsingDisk() const
{
  CacheStorageService::Self()->Lock().AssertCurrentThreadOwns();

  return mUseDisk;
}

bool CacheEntry::SetUsingDisk(bool aUsingDisk)
{
  // Called by the service when this entry is reopen to reflect
  // demanded storage target.
  CacheStorageService::Self()->Lock().AssertCurrentThreadOwns();

  bool changed = mUseDisk != aUsingDisk;
  mUseDisk = aUsingDisk;
  return changed;
}

uint32_t CacheEntry::GetMetadataMemoryOccupation() const
{
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
  // No need to sync when only reading.
  // When consumer needs to be consistent with state of the memory storage entries
  // table, then let it use GetUseDisk getter that must be called under the service lock.
  *aPersistToDisk = mUseDisk;
  return NS_OK;
}
NS_IMETHODIMP CacheEntry::SetPersistToDisk(bool aPersistToDisk)
{
  if (mUseDisk == aPersistToDisk)
    return NS_OK;

  mozilla::MutexAutoLock lock(CacheStorageService::Self()->Lock());

  mUseDisk = aPersistToDisk;
  CacheStorageService::Self()->RecordMemoryOnlyEntry(
    this, !aPersistToDisk, false /* don't overwrite */);

  // File persistence is setup just before we open output stream on it.

  return NS_OK;
}

NS_IMETHODIMP CacheEntry::GetKey(nsACString & aKey)
{
  return mURI->GetAsciiSpec(aKey);
}

NS_IMETHODIMP CacheEntry::GetFetchCount(int32_t *aFetchCount)
{
  nsRefPtr<CacheFile> file(File());
  if (!file)
    return NS_ERROR_NOT_AVAILABLE;

  CacheFileAutoLock lock(mFile);

  CacheFileMetadata* metadata = file->Metadata();
  MOZ_ASSERT(metadata);
  NS_ENSURE_TRUE(metadata, NS_ERROR_UNEXPECTED);

  return metadata->GetFetchCount(reinterpret_cast<uint32_t*>(aFetchCount));
}

NS_IMETHODIMP CacheEntry::GetLastFetched(uint32_t *aLastFetched)
{
  nsRefPtr<CacheFile> file(File());
  if (!file)
    return NS_ERROR_NOT_AVAILABLE;

  CacheFileAutoLock lock(mFile);

  CacheFileMetadata* metadata = file->Metadata();
  MOZ_ASSERT(metadata);
  NS_ENSURE_TRUE(metadata, NS_ERROR_UNEXPECTED);

  return metadata->GetLastFetched(aLastFetched);
}

NS_IMETHODIMP CacheEntry::GetLastModified(uint32_t *aLastModified)
{
  nsRefPtr<CacheFile> file(File());
  if (!file)
    return NS_ERROR_NOT_AVAILABLE;

  CacheFileAutoLock lock(mFile);

  CacheFileMetadata* metadata = file->Metadata();
  MOZ_ASSERT(metadata);
  NS_ENSURE_TRUE(metadata, NS_ERROR_UNEXPECTED);

  return metadata->GetLastModified(aLastModified);
  return NS_OK;
}

NS_IMETHODIMP CacheEntry::GetExpirationTime(uint32_t *aExpirationTime)
{
  nsRefPtr<CacheFile> file(File());
  if (!file)
    return NS_ERROR_NOT_AVAILABLE;

  CacheFileAutoLock lock(mFile);

  CacheFileMetadata* metadata = file->Metadata();
  MOZ_ASSERT(metadata);
  NS_ENSURE_TRUE(metadata, NS_ERROR_UNEXPECTED);

  return metadata->GetExpirationTime(aExpirationTime);
}

NS_IMETHODIMP CacheEntry::SetExpirationTime(uint32_t aExpirationTime)
{
  nsRefPtr<CacheFile> file(File());
  if (!file)
    return NS_ERROR_NOT_AVAILABLE;

  {
    CacheFileAutoLock lock(mFile);

    CacheFileMetadata* metadata = file->Metadata();
    MOZ_ASSERT(metadata);
    NS_ENSURE_TRUE(metadata, NS_ERROR_UNEXPECTED);

    nsresult rv = metadata->SetExpirationTime(aExpirationTime);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Aligned assignment, thus atomic.
  mSortingExpirationTime = aExpirationTime;
  return NS_OK;
}

NS_IMETHODIMP CacheEntry::OpenInputStream(uint32_t offset, nsIInputStream * *_retval)
{
  LOG(("CacheEntry::OpenInputStream [this=%p]", this));

  if (mIsDoomed) {
    LOG(("  doomed..."));
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsRefPtr<CacheFile> file(File());
  if (!file)
    return NS_ERROR_NOT_AVAILABLE;

  nsresult rv;

  nsCOMPtr<nsIInputStream> stream;
  rv = file->OpenInputStream(getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISeekableStream> seekable =
    do_QueryInterface(stream, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = seekable->Seek(nsISeekableStream::NS_SEEK_SET, offset);
  NS_ENSURE_SUCCESS(rv, rv);

  stream.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP CacheEntry::OpenOutputStream(uint32_t offset, nsIOutputStream * *_retval)
{
  LOG(("CacheEntry::OpenOutputStream [this=%p]", this));

  if (mIsDoomed) {
    LOG(("  doomed..."));
    return NS_ERROR_NOT_AVAILABLE;
  }

  MOZ_ASSERT(mState == HASMETADATA);

  nsRefPtr<CacheFile> file(File());
  if (!file)
    return NS_ERROR_NOT_AVAILABLE;

  nsresult rv;

  // No need to sync on mUseDisk here, we don't need to be consistent
  // with content of the memory storage entries hash table.
  if (!mUseDisk) {
    rv = file->SetMemoryOnly();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIOutputStream> stream;
  rv = file->OpenOutputStream(getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISeekableStream> seekable =
    do_QueryInterface(stream, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = seekable->Seek(nsISeekableStream::NS_SEEK_SET, offset);
  NS_ENSURE_SUCCESS(rv, rv);

  // MEMORY COPYING !
  // Unfortunatelly, our consumer here is nsStreamTee, that requires
  // blocking stream as its sink.  The stream we get from CacheFile
  // is non-blocking, so we need to buffer :(

  // Speed vs memory consumption based on predicted data size
  uint32_t segment;
  if (mPredictedDataSize > 1024 * 1024)
    segment = 256 * 1024; // use 256k segments for 1MB+ content
  else if (mPredictedDataSize > 10 * 1024)
    segment = 64 * 1024;  // use 64k segments for 10kB+ content
  else
    segment = 4 * 1024;   // use 4k segments for smaller then 10kB or unknown (chunked)

  nsCOMPtr<nsIAsyncOutputStream> pipeOut;
  nsCOMPtr<nsIAsyncInputStream> pipeIn;
  rv = NS_NewPipe2(getter_AddRefs(pipeIn),
                   getter_AddRefs(pipeOut),
                   true, // non-blocking input
                   false, // blocking output
                   segment, (uint32_t)-1);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIEventTarget> streamTransportThread =
    do_GetService("@mozilla.org/network/stream-transport-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_AsyncCopy(pipeIn, stream, streamTransportThread,
                    NS_ASYNCCOPY_VIA_READSEGMENTS, segment);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = CallQueryInterface(pipeOut, _retval);
  NS_ENSURE_SUCCESS(rv, rv);

  // Since CacheFile logic is kept very simple, we can claim the entry is ready
  // and usable by waiting concurent readers only after output stream has been
  // opened.  Only when output stream exists for a CacheFile, concurent readers
  // are getting WOULD_BLOCK when reading after the end of the data.
  mState = READY;

  // Invoke any pending readers now.
  InvokeCallbacks();

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
  if (mSecurityInfoLoaded) {
    NS_IF_ADDREF(*aSecurityInfo = mSecurityInfo);
    return NS_OK;
  }

  nsRefPtr<CacheFile> file(File());
  if (!file)
    return NS_ERROR_NOT_AVAILABLE;

  char const* info;
  nsCOMPtr<nsISupports> secInfo;
  {
    CacheFileAutoLock lock(mFile);

    CacheFileMetadata* metadata = file->Metadata();
    MOZ_ASSERT(metadata);
    NS_ENSURE_TRUE(metadata, NS_ERROR_UNEXPECTED);

    info = metadata->GetElement("security-info");
  }

  if (info) {
    nsresult rv = NS_DeserializeObject(nsDependentCString(info),
                                       getter_AddRefs(secInfo));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  {
    mozilla::MutexAutoLock lock(mLock);

    mSecurityInfoLoaded = true;
    mSecurityInfo.swap(secInfo);

    NS_IF_ADDREF(*aSecurityInfo = mSecurityInfo);
  }

  return NS_OK;
}
NS_IMETHODIMP CacheEntry::SetSecurityInfo(nsISupports *aSecurityInfo)
{
  nsresult rv;

  nsRefPtr<CacheFile> file;
  {
    mozilla::MutexAutoLock lock(mLock);

    mSecurityInfo = aSecurityInfo;
    mSecurityInfoLoaded = true;

    if (!mFile)
      return NS_ERROR_NOT_AVAILABLE;

    file = mFile;
  }

  nsCOMPtr<nsISerializable> serializable =
    do_QueryInterface(aSecurityInfo);
  if (aSecurityInfo && !serializable)
    return NS_ERROR_UNEXPECTED;

  nsCString info;
  if (serializable) {
    rv = NS_SerializeToString(serializable, info);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  {
    CacheFileAutoLock lock(mFile);

    CacheFileMetadata* metadata = file->Metadata();
    MOZ_ASSERT(metadata);
    NS_ENSURE_TRUE(metadata, NS_ERROR_UNEXPECTED);

    rv = metadata->SetElement("security-info", info.Length() ? info.get() : nullptr);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP CacheEntry::GetStorageDataSize(uint32_t *aStorageDataSize)
{
  mozilla::MutexAutoLock lock(mLock);

  *aStorageDataSize = 0; // will be reported in callback from file or a getter on file will be exposed
  return NS_OK;
}

NS_IMETHODIMP CacheEntry::AsyncDoom(nsICacheEntryDoomCallback *aCallback)
{
  {
    mozilla::MutexAutoLock lock(mLock);

    if (mIsDoomed || mDoomCallback)
      return NS_ERROR_IN_PROGRESS; // to aggregate have DOOMING state

    mIsDoomed = true;
    mDoomCallback = aCallback;
  }

  // Immediately remove the entry from the storage hash table
  CacheStorageService::Self()->RemoveEntry(this);
  BackgroundOp(Ops::DOOM);

  return NS_OK;
}

NS_IMETHODIMP CacheEntry::GetMetaDataElement(const char * aKey, char * *aRetval)
{
  nsRefPtr<CacheFile> file(File());
  if (!file)
    return NS_ERROR_NOT_AVAILABLE;

  CacheFileAutoLock lock(file);

  CacheFileMetadata* metadata = file->Metadata();
  MOZ_ASSERT(metadata);
  NS_ENSURE_TRUE(metadata, NS_ERROR_UNEXPECTED);

  char const *value = metadata->GetElement(aKey);
  if (!value)
    return NS_ERROR_NOT_AVAILABLE;

  *aRetval = NS_strdup(value);
  return NS_OK;
}

NS_IMETHODIMP CacheEntry::SetMetaDataElement(const char * aKey, const char * aValue)
{
  nsRefPtr<CacheFile> file(File());
  if (!file)
    return NS_ERROR_NOT_AVAILABLE;

  CacheFileAutoLock lock(file);

  CacheFileMetadata* metadata = file->Metadata();
  MOZ_ASSERT(metadata);
  NS_ENSURE_TRUE(metadata, NS_ERROR_UNEXPECTED);

  return metadata->SetElement(aKey, aValue);
}

NS_IMETHODIMP CacheEntry::MetaDataReady()
{
#if defined(DEBUG) || defined(MOZ_LOGGING)
  uint32_t const state = mState;
#endif

  LOG(("CacheEntry::MetaDataReady [this=%p, state=%s]", this, StateString(state)));

  MOZ_ASSERT(state == WRITING);

  // No need to lock here, there is only one consumer (the writer) that
  // can manipulate state of this entry.
  mState = HASMETADATA;

  BackgroundOp(Ops::REPORTUSAGE);

  return NS_OK;
}

NS_IMETHODIMP CacheEntry::SetValid()
{
#if defined(DEBUG) || defined(MOZ_LOGGING)
  uint32_t const state = mState;
#endif

  LOG(("CacheEntry::SetValid [this=%p, state=%s]", this, StateString(state)));

  MOZ_ASSERT(state == REVALIDATING);

  // No need to lock here, since from READY we can transit only to
  // REVALIDATING and there is no issue to race those two states.
  mState = READY;

  BackgroundOp(Ops::REPORTUSAGE);
  InvokeCallbacks();

  return NS_OK;
}

NS_IMETHODIMP CacheEntry::Recreate(nsICacheEntry **_retval)
{
  LOG(("CacheEntry::Recreate [this=%p, state=%s]", this, StateString(mState)));

  nsRefPtr<CacheEntry> newEntry = ReopenTruncated(nullptr);
  if (newEntry)
    newEntry.forget(_retval);
  else
    BackgroundOp(Ops::CALLBACKS, true);

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

  if (mState == WRITING)
    return NS_ERROR_IN_PROGRESS;

  nsRefPtr<CacheFile> file(File());
  if (!file)
    return NS_OK; // really OK?

  // mayhemer: TODO Problem with compression
  *aDataSize = file->DataSize();
  return NS_OK;
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
    switch (aWhat) {
    case PURGE_DATA_ONLY_DISK_BACKED:
    case PURGE_WHOLE_ONLY_DISK_BACKED:
      // This is an in-memory only entry, don't purge it
      if (!mUseDisk) {
        LOG(("  not using disk"));
        return false;
      }
    }

    if (mState == WRITING || mState == LOADING || mFrecency == 0) {
      // In-progress (write or load) entries should (at least for consistency and from
      // the logical point of view) stay in memory.
      // Zero-frecency entries are those which have never been given to any consumer, those
      // are actually very fresh and should not go just because frecency had not been set
      // so far.
      LOG(("  state=%s, frecency=%1.10f", StateString(mState), mFrecency));
      return false;
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
      nsRefPtr<CacheFile> file(File());
      if (file) {
        // TODO
        // file->ThrowMemoryCachedData();
      }

      CacheStorageService::Self()->OnMemoryConsumptionChange(this, mMetadataMemoryOccupation);

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

  mIsDoomed = true;

  if (!CacheStorageService::IsOnManagementThread()) {
    BackgroundOp(Ops::DOOM);
    return;
  }

  CacheStorageService::Self()->UnregisterEntry(this);
  CacheStorageService::Self()->OnMemoryConsumptionChange(this, 0);

  nsCOMPtr<nsICacheEntryDoomCallback> callback;
  nsRefPtr<CacheFile> file;
  bool invokeCallbacks;
  {
    mozilla::MutexAutoLock lock(mLock);

    invokeCallbacks = mCallbacks.Length() || mReadOnlyCallbacks.Length();

    // Otherwise wait for the file to be doomed
    if (!mFile)
      mDoomCallback.swap(callback);
    else
      file = mFile;
  }

  if (invokeCallbacks) {
    // Must force post here since may be indirectly called from
    // InvokeCallbacks of this entry and we don't want reentrancy here.
    BackgroundOp(Ops::CALLBACKS, true);
  }

  if (file) {
    file->Doom(this);
  }
  else if (callback) {
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

  if (aOperations & Ops::REGISTER) {
    LOG(("CacheEntry REGISTER [this=%p]", this));

    CacheStorageService::Self()->RegisterEntry(this);
  }

  if (aOperations & Ops::REPORTUSAGE) {
    LOG(("CacheEntry REPORTUSAGE [this=%p]", this));

    nsRefPtr<CacheFile> file(File());
    if (!file)
      return;

    uint32_t memorySize;
    {
      // TODO: The following line is wrong, need API on CacheFile to get
      // actual allocation consumption, best with file->Metadata()->ElementsSize() included
      memorySize = (uint32_t)std::max(uint64_t(file->DataSize()), uint64_t(PR_UINT32_MAX));

      CacheFileAutoLock lock(file);

      CacheFileMetadata* metadata = file->Metadata();
      if (metadata) {
        mMetadataMemoryOccupation = metadata->ElementsSize();
        memorySize += mMetadataMemoryOccupation;
      }
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
