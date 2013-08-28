// Stuff to link the old imp to the new api - will go away!

#include "OldWrappers.h"
#include "CacheStorage.h"
#include "CacheStorageService.h"
#include "CacheLog.h"
#include "LoadContextInfo.h"

#include "nsIURI.h"
#include "nsICacheService.h"
#include "nsICacheSession.h"
#include "nsIApplicationCache.h"
#include "nsIApplicationCacheService.h"
#include "nsIStreamTransportService.h"
#include "nsIFile.h"
#include "nsICacheEntryDoomCallback.h"
#include "nsICacheListener.h"
#include "nsICacheStorageVisitor.h"

#include "nsServiceManagerUtils.h"
#include "nsNetCID.h"
#include "nsProxyRelease.h"

static NS_DEFINE_CID(kStreamTransportServiceCID,
                     NS_STREAMTRANSPORTSERVICE_CID);

namespace mozilla {
namespace net {

namespace { // anon

// Fires the doom callback back on the main thread
// after the cache I/O thread is looped.

class DoomCallbackSynchronizer : public nsRunnable
{
public:
  DoomCallbackSynchronizer(nsICacheEntryDoomCallback* cb) : mCB(cb)
  {
    MOZ_COUNT_CTOR(DoomCallbackSynchronizer);
  }
  nsresult Dispatch();

private:
  virtual ~DoomCallbackSynchronizer()
  {
    MOZ_COUNT_DTOR(DoomCallbackSynchronizer);
  }

  NS_DECL_NSIRUNNABLE
  nsCOMPtr<nsICacheEntryDoomCallback> mCB;
};

nsresult DoomCallbackSynchronizer::Dispatch()
{
  nsresult rv;

  nsCOMPtr<nsICacheService> serv =
      do_GetService(NS_CACHESERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIEventTarget> eventTarget;
  rv = serv->GetCacheIOTarget(getter_AddRefs(eventTarget));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = eventTarget->Dispatch(this, NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP DoomCallbackSynchronizer::Run()
{
  if (!NS_IsMainThread()) {
    NS_DispatchToMainThread(this);
  }
  else {
    if (mCB)
      mCB->OnCacheEntryDoomed(NS_OK);
  }
  return NS_OK;
}

// Receives doom callback from the old API and forwards to the new API

class DoomCallbackWrapper : public nsICacheListener
{
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICACHELISTENER

  DoomCallbackWrapper(nsICacheEntryDoomCallback* cb) : mCB(cb)
  {
    MOZ_COUNT_CTOR(DoomCallbackWrapper);
  }

private:
  virtual ~DoomCallbackWrapper()
  {
    MOZ_COUNT_DTOR(DoomCallbackWrapper);
  }

  nsCOMPtr<nsICacheEntryDoomCallback> mCB;
};

NS_IMPL_ISUPPORTS1(DoomCallbackWrapper, nsICacheListener);

NS_IMETHODIMP DoomCallbackWrapper::OnCacheEntryAvailable(nsICacheEntryDescriptor *descriptor,
                                                         nsCacheAccessMode accessGranted,
                                                         nsresult status)
{
  return NS_OK;
}

NS_IMETHODIMP DoomCallbackWrapper::OnCacheEntryDoomed(nsresult status)
{
  if (!mCB)
    return NS_ERROR_NULL_POINTER;

  mCB->OnCacheEntryDoomed(status);
  mCB = nullptr;
  return NS_OK;
}

// Receives visit callbacks from the old API and forwards it to the new API

class VisitCallbackWrapper : public nsICacheVisitor
{
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICACHEVISITOR

  VisitCallbackWrapper(char* const deviceID,
                       nsICacheStorageVisitor* cb,
                       bool visitEntries)
  : mCB(cb)
  , mVisitEntries(visitEntries)
  , mDeviceID(deviceID)
  {
    MOZ_COUNT_CTOR(VisitCallbackWrapper);
  }

private:
  virtual ~VisitCallbackWrapper();
  nsCOMPtr<nsICacheStorageVisitor> mCB;
  bool mVisitEntries;
  char* const mDeviceID;
};

NS_IMPL_ISUPPORTS1(VisitCallbackWrapper, nsICacheVisitor)

VisitCallbackWrapper::~VisitCallbackWrapper()
{
  if (mVisitEntries)
    mCB->OnCacheEntryVisitCompleted();

  MOZ_COUNT_DTOR(VisitCallbackWrapper);
}

NS_IMETHODIMP VisitCallbackWrapper::VisitDevice(const char * deviceID,
                                                nsICacheDeviceInfo *deviceInfo,
                                                bool *_retval)
{
  if (!mCB)
    return NS_ERROR_NULL_POINTER;

  *_retval = false;
  if (strcmp(deviceID, mDeviceID)) {
    // Not the device we want to visit
    return NS_OK;
  }

  nsresult rv;

  uint32_t entryCount;
  rv = deviceInfo->GetEntryCount(&entryCount);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t totalSize;
  rv = deviceInfo->GetTotalSize(&totalSize);
  NS_ENSURE_SUCCESS(rv, rv);

  mCB->OnCacheStorageInfo(entryCount, totalSize);
  *_retval = mVisitEntries;

  return NS_OK;
}

NS_IMETHODIMP VisitCallbackWrapper::VisitEntry(const char * deviceID,
                                               nsICacheEntryInfo *entryInfo,
                                               bool *_retval)
{
  MOZ_ASSERT(!strcmp(deviceID, mDeviceID));

  nsRefPtr<_OldCacheEntryWrapper> wrapper = new _OldCacheEntryWrapper(entryInfo);
  nsresult rv = mCB->OnCacheEntryInfo(wrapper);
  *_retval = NS_SUCCEEDED(rv);

  return NS_OK;
}

} // anon


// _OldCacheEntryWrapper

_OldCacheEntryWrapper::_OldCacheEntryWrapper(nsICacheEntryDescriptor* desc)
: mOldDesc(desc), mOldInfo(desc)
{
}

_OldCacheEntryWrapper::_OldCacheEntryWrapper(nsICacheEntryInfo* info)
: mOldInfo(info)
{
}

_OldCacheEntryWrapper::~_OldCacheEntryWrapper()
{
}

NS_IMPL_ISUPPORTS1(_OldCacheEntryWrapper, nsICacheEntry)

NS_IMETHODIMP _OldCacheEntryWrapper::AsyncDoom(nsICacheEntryDoomCallback* listener)
{
  nsRefPtr<DoomCallbackWrapper> cb = new DoomCallbackWrapper(listener);
  return AsyncDoom(cb);
}

NS_IMETHODIMP _OldCacheEntryWrapper::GetDataSize(int64_t *aSize)
{
  uint32_t size;
  nsresult rv = GetDataSize(&size);
  if (NS_FAILED(rv))
    return rv;

  *aSize = size;
  return NS_OK;
}

NS_IMETHODIMP _OldCacheEntryWrapper::GetPersistToDisk(bool *aPersistToDisk)
{
  if (!mOldDesc) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rv;

  nsCacheStoragePolicy policy;
  rv = mOldDesc->GetStoragePolicy(&policy);
  NS_ENSURE_SUCCESS(rv, rv);

  *aPersistToDisk = policy != nsICache::STORE_IN_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP _OldCacheEntryWrapper::SetPersistToDisk(bool aPersistToDisk)
{
  if (!mOldDesc) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rv;

  nsCacheStoragePolicy policy;
  rv = mOldDesc->GetStoragePolicy(&policy);
  NS_ENSURE_SUCCESS(rv, rv);

  if (policy == nsICache::STORE_OFFLINE) {
    return aPersistToDisk
      ? NS_OK
      : NS_ERROR_NOT_AVAILABLE;
  }

  policy = aPersistToDisk
    ? nsICache::STORE_ON_DISK
    : nsICache::STORE_IN_MEMORY;
  rv = mOldDesc->SetStoragePolicy(policy);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP _OldCacheEntryWrapper::Recreate(nsICacheEntry** aResult)
{
#ifdef DEBUG
  if (mOldDesc) {
    nsCacheAccessMode mode;
    nsresult rv = mOldDesc->GetAccessGranted(&mode);
    MOZ_ASSERT(NS_SUCCEEDED(rv) && (mode & nsICache::ACCESS_WRITE));
  }
#endif

  nsCOMPtr<nsICacheEntry> self(this);
  self.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP _OldCacheEntryWrapper::OpenInputStream(int64_t offset,
                                                     nsIInputStream * *_retval)
{
  if (offset > PR_UINT32_MAX)
    return NS_ERROR_INVALID_ARG;

  return OpenInputStream(uint32_t(offset), _retval);
}
NS_IMETHODIMP _OldCacheEntryWrapper::OpenOutputStream(int64_t offset,
                                                      nsIOutputStream * *_retval)
{
  if (offset > PR_UINT32_MAX)
    return NS_ERROR_INVALID_ARG;

  return OpenOutputStream(uint32_t(offset), _retval);
}


namespace { // anon

void
GetCacheSessionNameForStoragePolicy(
        nsCacheStoragePolicy storagePolicy,
        bool isPrivate,
        uint32_t appId,
        bool inBrowser,
        nsACString& sessionName)
{
  MOZ_ASSERT(!isPrivate || storagePolicy == nsICache::STORE_IN_MEMORY);

  switch (storagePolicy) {
    case nsICache::STORE_IN_MEMORY:
      sessionName.AssignASCII(isPrivate ? "HTTP-memory-only-PB" : "HTTP-memory-only");
      break;
    case nsICache::STORE_OFFLINE:
      sessionName.AssignLiteral("HTTP-offline");
      break;
    default:
      sessionName.AssignLiteral("HTTP");
      break;
  }
  if (appId != nsILoadContextInfo::NO_APP_ID || inBrowser) {
    sessionName.Append('~');
    sessionName.AppendInt(appId);
    sessionName.Append('~');
    sessionName.AppendInt(inBrowser);
  }
}

nsresult
GetCacheSession(bool aWriteToDisk,
                nsILoadContextInfo* aLoadInfo,
                nsIApplicationCache* aAppCache,
                nsICacheSession** _result)
{
  nsresult rv;

  nsCacheStoragePolicy storagePolicy;
  if (aAppCache)
    storagePolicy = nsICache::STORE_OFFLINE;
  else if (!aWriteToDisk || aLoadInfo->IsPrivate())
    storagePolicy = nsICache::STORE_IN_MEMORY;
  else
    storagePolicy = nsICache::STORE_ANYWHERE;

  nsAutoCString clientId;
  if (aAppCache) {
    aAppCache->GetClientID(clientId);
  }
  else {
    GetCacheSessionNameForStoragePolicy(
      storagePolicy,
      aLoadInfo->IsPrivate(),
      aLoadInfo->AppId(),
      aLoadInfo->IsInBrowserElement(),
      clientId);
  }

  LOG(("  GetCacheSession for client=%s, policy=%d", clientId.get(), storagePolicy));

  nsCOMPtr<nsICacheService> serv =
      do_GetService(NS_CACHESERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsICacheSession> session;
  rv = serv->CreateSession(clientId.get(),
                           storagePolicy,
                           nsICache::STREAM_BASED,
                           getter_AddRefs(session));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = session->SetIsPrivate(aLoadInfo->IsPrivate());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = session->SetDoomEntriesIfExpired(false);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aAppCache) {
    nsCOMPtr<nsIFile> profileDirectory;
    aAppCache->GetProfileDirectory(getter_AddRefs(profileDirectory));
    if (profileDirectory)
      rv = session->SetProfileDirectory(profileDirectory);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  session.forget(_result);
  return NS_OK;
}

} // anon


NS_IMPL_ISUPPORTS_INHERITED1(_OldCacheLoad, nsRunnable, nsICacheListener)

_OldCacheLoad::_OldCacheLoad(nsCSubstring const& aCacheKey,
                             nsICacheEntryOpenCallback* aCallback,
                             nsIApplicationCache* aAppCache,
                             nsILoadContextInfo* aLoadInfo,
                             bool aWriteToDisk,
                             uint32_t aFlags)
  : mCacheKey(aCacheKey)
  , mCallback(aCallback)
  , mLoadInfo(GetLoadContextInfo(aLoadInfo))
  , mFlags(aFlags)
  , mWriteToDisk(aWriteToDisk)
  , mMainThreadOnly(true)
  , mNew(true)
  , mStatus(NS_ERROR_UNEXPECTED)
  , mRunCount(0)
  , mAppCache(aAppCache)
{
  MOZ_COUNT_CTOR(_OldCacheLoad);
}

_OldCacheLoad::~_OldCacheLoad()
{
  ProxyReleaseMainThread(mAppCache);
  MOZ_COUNT_DTOR(_OldCacheLoad);
}

nsresult _OldCacheLoad::Start()
{
  MOZ_ASSERT(NS_IsMainThread());

  bool mainThreadOnly;
  if (mCallback && (
      NS_SUCCEEDED(mCallback->GetMainThreadOnly(&mainThreadOnly)) &&
      !mainThreadOnly)) {
    mMainThreadOnly = false;
  }

  nsresult rv;

  // XXX: Start the cache service; otherwise DispatchToCacheIOThread will
  // fail.
  nsCOMPtr<nsICacheService> service =
    do_GetService(NS_CACHESERVICE_CONTRACTID, &rv);

  // Ensure the stream transport service gets initialized on the main thread
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIStreamTransportService> sts =
      do_GetService(kStreamTransportServiceCID, &rv);
  }

  if (NS_SUCCEEDED(rv)) {
    rv = service->GetCacheIOTarget(getter_AddRefs(mCacheThread));
  }

  if (NS_SUCCEEDED(rv)) {
    rv = mCacheThread->Dispatch(this, NS_DISPATCH_NORMAL);
  }

  return rv;
}

NS_IMETHODIMP
_OldCacheLoad::Run()
{
  LOG(("_OldCacheLoad::Run [this=%p, cb=%p]", this, mCallback.get()));

  nsresult rv;

  if (!NS_IsMainThread()) {
    //AssertOnCacheThread();

    nsCOMPtr<nsICacheSession> session;
    rv = GetCacheSession(mWriteToDisk, mLoadInfo, mAppCache, getter_AddRefs(session));
    if (NS_SUCCEEDED(rv)) {
      // AsyncOpenCacheEntry isn't really async when its called on the
      // cache service thread.

      nsCacheAccessMode cacheAccess;
      if (mFlags & nsICacheStorage::OPEN_TRUNCATE)
        cacheAccess = nsICache::ACCESS_WRITE;
      else if ((mFlags & nsICacheStorage::OPEN_READONLY) || mAppCache)
        cacheAccess = nsICache::ACCESS_READ;
      else
        cacheAccess = nsICache::ACCESS_READ_WRITE;


      LOG(("  AsyncOpenCacheEntry with access=%d", cacheAccess));

      bool bypassBusy = mFlags & nsICacheStorage::OPEN_BYPASS_IF_BUSY;
      rv = session->AsyncOpenCacheEntry(mCacheKey, cacheAccess, this, bypassBusy);
    }

    // TODO fix...
    if (NS_FAILED(rv)) {
      rv = OnCacheEntryAvailable(nullptr, 0, rv);
    }
  } else {
    if (mMainThreadOnly)
      Check();

    // break cycles
    nsCOMPtr<nsICacheEntryOpenCallback> cb = mCallback.forget();
    mCacheThread = nullptr;
    nsCOMPtr<nsICacheEntry> entry = mCacheEntry.forget();

    rv = cb->OnCacheEntryAvailable(entry, mNew, mAppCache, mStatus);

    if (NS_FAILED(rv) && entry) {
      LOG(("  OnCacheEntryAvailable failed with rv=0x%08x", rv));
      if (mNew)
        entry->AsyncDoom(nullptr);
      else
        entry->Close();
    }
  }

  return rv;
}

NS_IMETHODIMP
_OldCacheLoad::OnCacheEntryAvailable(nsICacheEntryDescriptor *entry,
                                     nsCacheAccessMode access,
                                     nsresult status)
{
  LOG(("_OldCacheLoad::OnCacheEntryAvailable ent=%p, cb=%p, appcache=%p, access=%x",
    entry, mCallback.get(), mAppCache.get(), access));

  // XXX Bug 759805: Sometimes we will call this method directly from
  // HttpCacheQuery::Run when AsyncOpenCacheEntry fails, but
  // AsyncOpenCacheEntry will also call this method. As a workaround, we just
  // ensure we only execute this code once.
  NS_ENSURE_TRUE(mRunCount == 0, NS_ERROR_UNEXPECTED);
  ++mRunCount;

  //AssertOnCacheThread();

  mCacheEntry = entry ? new _OldCacheEntryWrapper(entry) : nullptr;
  mStatus = status;

#if 0
  mNew = NS_SUCCEEDED(mStatus) && !(access & nsICache::ACCESS_READ);
#endif
  mNew = access == nsICache::ACCESS_WRITE;

  if (!mMainThreadOnly)
    Check();

  return NS_DispatchToMainThread(this);
}

void
_OldCacheLoad::Check()
{
  if (mCacheEntry && !mNew) {
    uint32_t valid;
    nsresult rv = mCallback->OnCacheEntryCheck(mCacheEntry, mAppCache, &valid);
    LOG(("OnCacheEntryCheck result ent=%p, cb=%p, appcache=%p, rv=0x%08x",
      mCacheEntry.get(), mCallback.get(), mAppCache.get(), rv));

    if (NS_FAILED(rv)) {
      NS_WARNING("cache check failed");
    }
    if (!valid) {
      NS_WARNING("not valid");
    }
  }
}

NS_IMETHODIMP
_OldCacheLoad::OnCacheEntryDoomed(nsresult)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

// nsICacheStorage old cache wrapper

NS_IMPL_ISUPPORTS1(_OldStorage, nsICacheStorage)

_OldStorage::_OldStorage(nsILoadContextInfo* aInfo,
                         bool aAllowDisk,
                         bool aLookupAppCache,
                         bool aOfflineStorage,
                         nsIApplicationCache* aAppCache)
: mLoadInfo(GetLoadContextInfo(aInfo))
, mAppCache(aAppCache)
, mWriteToDisk(aAllowDisk)
, mLookupAppCache(aLookupAppCache)
, mOfflineStorage(aOfflineStorage)
{
  MOZ_COUNT_CTOR(_OldStorage);
}

_OldStorage::~_OldStorage()
{
  MOZ_COUNT_DTOR(_OldStorage);
}

NS_IMETHODIMP _OldStorage::AsyncOpenURI(nsIURI *aURI,
                                        const nsACString & aIdExtension,
                                        uint32_t aFlags,
                                        nsICacheEntryOpenCallback *aCallback)
{
  LOG(("_OldStorage::AsyncOpenURI"));

  NS_ENSURE_ARG(aURI);
  NS_ENSURE_ARG(aCallback);

  nsresult rv;

  nsAutoCString cacheKey;
  rv = AssembleCacheKey(aURI, aIdExtension, cacheKey);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mAppCache && (mLookupAppCache || mOfflineStorage)) {
    MOZ_ASSERT(!(aFlags & nsICacheStorage::OPEN_TRUNCATE));

    rv = ChooseApplicationCache(cacheKey, getter_AddRefs(mAppCache));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsRefPtr<_OldCacheLoad> cacheLoad =
    new _OldCacheLoad(cacheKey, aCallback, mAppCache,
                      mLoadInfo, mWriteToDisk, aFlags);

  rv = cacheLoad->Start();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP _OldStorage::AsyncDoomURI(nsIURI *aURI, const nsACString & aIdExtension,
                                        nsICacheEntryDoomCallback* aCallback)
{
  LOG(("_OldStorage::AsyncDoomURI"));

  nsresult rv;

  nsAutoCString cacheKey;
  rv = AssembleCacheKey(aURI, aIdExtension, cacheKey);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsICacheSession> session;
  rv = GetCacheSession(mWriteToDisk, mLoadInfo, mAppCache, getter_AddRefs(session));
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<DoomCallbackWrapper> cb = aCallback
    ? new DoomCallbackWrapper(aCallback)
    : nullptr;
  rv = session->DoomEntry(cacheKey, cb);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP _OldStorage::AsyncEvictStorage(nsICacheEntryDoomCallback* aCallback)
{
  LOG(("_OldStorage::AsyncEvictStorage"));

  nsresult rv;

  if (!mAppCache && mOfflineStorage) {
    // Special casing for pure offline storage
    if (mLoadInfo->AppId() == nsILoadContextInfo::NO_APP_ID &&
        !mLoadInfo->IsInBrowserElement()) {

      // Clear everything.
      nsCOMPtr<nsICacheService> serv =
          do_GetService(NS_CACHESERVICE_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = serv->EvictEntries(nsICache::STORE_OFFLINE);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
      // Clear app or inbrowser staff.
      nsCOMPtr<nsIApplicationCacheService> appCacheService =
        do_GetService(NS_APPLICATIONCACHESERVICE_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = appCacheService->DiscardByAppId(mLoadInfo->AppId(),
                                           mLoadInfo->IsInBrowserElement());
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  else {
    nsCOMPtr<nsICacheSession> session;
    rv = GetCacheSession(mWriteToDisk, mLoadInfo, mAppCache, getter_AddRefs(session));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = session->EvictEntries();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (aCallback) {
    nsRefPtr<DoomCallbackSynchronizer> sync =
      new DoomCallbackSynchronizer(aCallback);
    rv = sync->Dispatch();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP _OldStorage::AsyncVisitStorage(nsICacheStorageVisitor* aVisitor,
                                             bool aVisitEntries)
{
  LOG(("_OldStorage::AsyncVisitStorage"));

  nsresult rv;

  nsCOMPtr<nsICacheService> serv =
      do_GetService(NS_CACHESERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  char* deviceID;
  if (mAppCache || mOfflineStorage) {
    deviceID = const_cast<char*>("offline");
  } else if (!mWriteToDisk || mLoadInfo->IsPrivate()) {
    deviceID = const_cast<char*>("memory");
  } else {
    deviceID = const_cast<char*>("disk");
  }

  nsRefPtr<VisitCallbackWrapper> cb = new VisitCallbackWrapper(
    deviceID, aVisitor, aVisitEntries);
  rv = serv->VisitEntries(cb);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

// Internal

nsresult _OldStorage::AssembleCacheKey(nsIURI *aURI,
                                       nsACString const & aIdExtension,
                                       nsACString & aCacheKey)
{
  // Copied from nsHttpChannel::AssembleCacheKey

  aCacheKey.Truncate();

  if (mLoadInfo->IsAnonymous()) { // if we add (&& !mAppCache), we fix 687758
    aCacheKey.AssignLiteral("anon&");
  }

  if (!aIdExtension.IsEmpty()) {
    aCacheKey.AppendPrintf("id=%s&", aIdExtension.BeginReading());
  }

  nsresult rv;

  nsCOMPtr<nsIURI> noRefURI;
  rv = aURI->CloneIgnoringRef(getter_AddRefs(noRefURI));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString uriSpec;
  rv = noRefURI->GetAsciiSpec(uriSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aCacheKey.IsEmpty()) {
    aCacheKey.AppendLiteral("uri=");
  }
  aCacheKey.Append(uriSpec);

  return NS_OK;
}

nsresult _OldStorage::ChooseApplicationCache(nsCSubstring const &cacheKey,
                                             nsIApplicationCache** aCache)
{
  nsresult rv;

  nsCOMPtr<nsIApplicationCacheService> appCacheService =
    do_GetService(NS_APPLICATIONCACHESERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = appCacheService->ChooseApplicationCache(cacheKey, mLoadInfo, aCache);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

} // net
} // mozilla
