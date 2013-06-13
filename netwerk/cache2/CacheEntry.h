/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheEntry__h__
#define CacheEntry__h__

#include "nsICacheEntry.h"
#include "CacheFile.h"

#include "nsIRunnable.h"
#include "nsIOutputStream.h"
#include "nsICacheEntryOpenCallback.h"
#include "nsICacheEntryDoomCallback.h"

#include "nsCOMPtr.h"
#include "nsRefPtrHashtable.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsString.h"
#include "nsCOMArray.h"
#include "nsThreadUtils.h"
#include "mozilla/Mutex.h"

static inline uint32_t
PRTimeToSeconds(PRTime t_usec)
{
  PRTime usec_per_sec = PR_USEC_PER_SEC;
  return uint32_t(t_usec /= usec_per_sec);
}

#define NowInSeconds() PRTimeToSeconds(PR_Now())

class nsIStorageStream;
class nsIOutputStream;
class nsIURI;

namespace mozilla {
namespace net {

class CacheStorageService;
class CacheStorage;

namespace {
class FrecencyComparator;
class ExpirationComparator;
class EvictionRunnable;
class WalkRunnable;
}

class CacheEntry : public nsICacheEntry
                 , public nsIRunnable
                 , public CacheFileListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICACHEENTRY
  NS_DECL_NSIRUNNABLE

  CacheEntry(const nsACString& aStorageID, nsIURI* aURI, const nsACString& aEnhanceID,
             bool aUseDisk);

  void AsyncOpen(nsICacheEntryOpenCallback* aCallback, uint32_t aFlags);

public:
  uint32_t GetMetadataMemoryOccupation() const;
  uint32_t GetDataMemoryOccupation() const;
  nsCString const &GetStorageID() const { return mStorageID; }
  nsCString const &GetEnhanceID() const { return mEnhanceID; }
  nsIURI* GetURI() const { return mURI; }
  bool UsingDisk() const;
  bool SetUsingDisk(bool aUsingDisk);

  // Methods for entry management (eviction from memory),
  // called only on the management thread.

  // TODO make these inline
  double GetFrecency() const;
  uint32_t GetExpirationTime() const;
  uint32_t& ReportedMemorySize();

  bool IsRegistered() const;
  bool CanRegister() const;
  void SetRegistered(bool aRegistered);

  enum EPurge {
    PURGE_DATA_ONLY_DISK_BACKED,
    PURGE_WHOLE_ONLY_DISK_BACKED,
    PURGE_WHOLE,
  };

  bool Purge(uint32_t aWhat);
  void PurgeAndDoom();
  void DoomAlreadyRemoved();

  nsresult HashingKeyWithStorage(nsACString &aResult);
  nsresult HashingKey(nsACString &aResult);

  static nsresult HashingKey(nsCSubstring const& aStorageID,
                             nsCSubstring const& aEnhanceID,
                             nsIURI* aURI,
                             nsACString &aResult);

  // Accessed only on the service management thread
  uint32_t mReportedMemorySize;
  double mFrecency;
  uint32_t mSortingExpirationTime;

private:
  virtual ~CacheEntry();

  // CacheFileListener
  NS_IMETHOD OnFileReady(nsresult aResult);
  NS_IMETHOD OnFileDoomed(nsresult aResult);

  // Keep the service alive during life-time of an entry
  nsRefPtr<CacheStorageService> mService;

  // We must monitor when a cache entry whose consumer is responsible
  // for writing it the first time gets released.  We must then invoke
  // waiting callbacks to not break the chain.
  class Handle : public nsICacheEntry
  {
  public:
    Handle(CacheEntry* aEntry);
    virtual ~Handle();

    NS_DECL_ISUPPORTS
    NS_FORWARD_NSICACHEENTRY(mEntry->)
  private:
    nsRefPtr<CacheEntry> mEntry;
  };

  // Since OnCacheEntryAvailable must be invoked on the main thread
  // we need a runnable for it...
  class AvailableCallbackRunnable : public nsRunnable
  {
  public:
    AvailableCallbackRunnable(CacheEntry* aEntry,
                              nsICacheEntryOpenCallback* aCallback,
                              bool aReadOnly)
      : mEntry(aEntry), mCallback(aCallback), mReadOnly(aReadOnly) {}

  private:
    NS_IMETHOD Run()
    {
      mEntry->InvokeAvailableCallback(mCallback, mReadOnly);
      return NS_OK;
    }

    nsRefPtr<CacheEntry> mEntry;
    nsCOMPtr<nsICacheEntryOpenCallback> mCallback;
    bool mReadOnly;
  };

  // Since OnCacheEntryDoomed must be invoked on the main thread
  // we need a runnable for it...
  class DoomCallbackRunnable : public nsRunnable
  {
  public:
    DoomCallbackRunnable(nsICacheEntryDoomCallback* aCallback, nsresult aRv)
      : mCallback(aCallback), mRv(aRv) {}

  private:
    NS_IMETHOD Run()
    {
      mCallback->OnCacheEntryDoomed(mRv);
      return NS_OK;
    }

    nsCOMPtr<nsICacheEntryDoomCallback> mCallback;
    nsresult mRv;
  };

  // Loads from disk asynchronously
  void Load(bool aTruncate);
  void OnLoaded();

  void RememberCallback(nsICacheEntryOpenCallback* aCallback, bool aReadOnly);
  void InvokeCallbacks();
  bool InvokeCallback(nsICacheEntryOpenCallback* aCallback, bool aReadOnly);
  void InvokeAvailableCallback(nsICacheEntryOpenCallback* aCallback, bool aReadOnly);
  void OnWriterClosed(Handle const* aHandle);

  // Schedules a background operation on the management thread.
  // When executed on the management thread directly, the operation(s)
  // is (are) executed immediately.
  void BackgroundOp(uint32_t aOperation, bool aForceAsync = false);
  void TransferCallbacks(nsCOMArray<nsICacheEntryOpenCallback> const &aCallbacks,
                         nsCOMArray<nsICacheEntryOpenCallback> const &aReadOnlyCallbacks);

  already_AddRefed<CacheFile> File();

  mozilla::Mutex mLock;

  nsCOMArray<nsICacheEntryOpenCallback> mCallbacks, mReadOnlyCallbacks;
  nsCOMPtr<nsICacheEntryDoomCallback> mDoomCallback;

  nsRefPtr<CacheFile> mFile;
  nsresult mFileLoadResult;
  nsCOMPtr<nsIURI> mURI;
  nsCString mEnhanceID;
  nsCString mStorageID;

  // Whether it's allowed to persist the data to disk
  // Synchronized by the service management lock.
  // Hence, leave it as a standalone boolean.
  bool mUseDisk;

  // Whether entry is in process of loading
  bool mIsLoading : 1;
  // Whether the entry is in process of being loaded from disk
  // Initially true, until we check the disk whether we have the entry
  bool mIsLoaded : 1;
  // Whether the entry metadata are present complete in memory
  bool mIsReady : 1;
  // Whether the entry is in process of being written (by a channel)
  bool mIsWriting : 1;
  // Doomed entry, don't let new consumers use it
  bool mIsDoomed : 1;
  // Whether this entry is registered in the storage service helper arrays
  bool mIsRegistered : 1;
  // After deregistration entry is no allowed to register again
  bool mIsRegistrationAllowed : 1;
  // Whether security info has already been looked up in metadata
  bool mSecurityInfoLoaded : 1;

  // Background thread scheduled operation.  Set (under the lock) one
  // of this flags to tell the background thread what to do.
  class Ops {
  public:
    static uint32_t const REGISTER =          1 << 0;
    static uint32_t const REPORTUSAGE =       1 << 1;
    static uint32_t const FRECENCYUPDATE =    1 << 2;
    static uint32_t const DOOM =              1 << 3;
    static uint32_t const CALLBACKS =         1 << 4;

    Ops() : mFlags(0) { }
    uint32_t Grab() { uint32_t flags = mFlags; mFlags = 0; return flags; }
    bool Set(uint32_t aFlags) { if (mFlags & aFlags) return false; mFlags |= aFlags; return true; }
  private:
    uint32_t mFlags;
  } mBackgroundOperations;

  nsCOMPtr<nsISupports> mSecurityInfo;

  int64_t mPredictedDataSize;
  uint32_t mDataSize; // ???

  uint32_t mMetadataMemoryOccupation;
};

} // net
} // mozilla

#endif
