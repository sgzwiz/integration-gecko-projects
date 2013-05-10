// Stuff to link the old imp to the new api - will go away!

#include "OldWrappers.h"
#include "CacheStorage.h"
#include "CacheLog.h"

#include "nsILoadContextInfo.h"
#include "nsIURI.h"
#include "nsICacheService.h"
#include "nsICacheSession.h"
#include "nsIApplicationCache.h"
#include "nsIApplicationCacheService.h"
#include "nsIStreamTransportService.h"
#include "nsIFile.h"

#include "nsServiceManagerUtils.h"
#include "nsNetCID.h"
#include "nsProxyRelease.h"

static NS_DEFINE_CID(kStreamTransportServiceCID,
                     NS_STREAMTRANSPORTSERVICE_CID);

namespace mozilla { namespace net {

namespace {
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
}

// _OldApplicationCacheLoad

NS_IMPL_THREADSAFE_ISUPPORTS1(_OldDescriptorWrapper, nsICacheEntry /*, nsICacheEntryInfo */)

NS_IMPL_ISUPPORTS_INHERITED1(_OldGenericCacheLoad, nsRunnable, nsICacheListener)

_OldGenericCacheLoad::~_OldGenericCacheLoad()
{
  nsCOMPtr<nsIThread> mainThread;
  NS_GetMainThread(getter_AddRefs(mainThread));

  nsIURI* uri;
  mURI.forget(&uri);
  NS_ProxyRelease(mainThread, uri);

  nsIApplicationCache* appcache;
  mAppCache.forget(&appcache);
  NS_ProxyRelease(mainThread, appcache);
}

nsresult _OldGenericCacheLoad::Start()
{
  MOZ_ASSERT(NS_IsMainThread());

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
_OldGenericCacheLoad::Run()
{
  nsresult rv;

  if (!NS_IsMainThread()) {
    //AssertOnCacheThread();

    nsCacheStoragePolicy storagePolicy;
    if (!mStorage->WriteToDisk() || mStorage->LoadInfo()->IsPrivate())
      storagePolicy = nsICache::STORE_IN_MEMORY;
    else
      storagePolicy = nsICache::STORE_ANYWHERE;

    nsAutoCString clientId;
    GetCacheSessionNameForStoragePolicy(
      storagePolicy,
      mStorage->LoadInfo()->IsPrivate(),
      mStorage->LoadInfo()->AppId(),
      mStorage->LoadInfo()->IsInBrowserElement(),
      clientId);

    nsCOMPtr<nsICacheService> serv =
        do_GetService(NS_CACHESERVICE_CONTRACTID, &rv);

    nsCOMPtr<nsICacheSession> session;
    if (NS_SUCCEEDED(rv)) {
      rv = serv->CreateSession(clientId.get(),
                               storagePolicy,
                               nsICache::STREAM_BASED,
                               getter_AddRefs(session));
    }
    if (NS_SUCCEEDED(rv)) {
      rv = session->SetIsPrivate(mStorage->LoadInfo()->IsPrivate());
    }
    if (NS_SUCCEEDED(rv)) {
      rv = session->SetDoomEntriesIfExpired(false);
    }
    if (NS_SUCCEEDED(rv)) {
      nsAutoCString cacheKey;
      mURI->GetAsciiSpec(cacheKey);
      // AsyncOpenCacheEntry isn't really async when its called on the
      // cache service thread.

      nsCacheAccessMode cacheAccess;
      if (mTruncate)
        cacheAccess = nsICache::ACCESS_WRITE;
      else
        cacheAccess = nsICache::ACCESS_READ_WRITE;

      rv = session->AsyncOpenCacheEntry(cacheKey, cacheAccess, this, false);
    }

    if (NS_FAILED(rv)) {
      rv = OnCacheEntryAvailable(nullptr, 0, rv);
    }
  } else {
    // break cycles
    nsCOMPtr<nsICacheEntryOpenCallback> cb = mCallback.forget();
    mCacheThread = nullptr;
    nsCOMPtr<nsICacheEntry> entry = mCacheEntry.forget();

    rv = cb->OnCacheEntryAvailable(entry, mNew, nullptr, mStatus);
  }

  return rv;
}

NS_IMETHODIMP
_OldGenericCacheLoad::OnCacheEntryAvailable(nsICacheEntryDescriptor *entry,
                                            nsCacheAccessMode access,
                                            nsresult status)
{
  LOG(("_OldGenericCacheLoad::OnCacheEntryAvailable ent=%p, cb=%p, appcache=%p, access=%x",
    entry, mCallback.get(), mAppCache.get(), access));

  // XXX Bug 759805: Sometimes we will call this method directly from
  // HttpCacheQuery::Run when AsyncOpenCacheEntry fails, but
  // AsyncOpenCacheEntry will also call this method. As a workaround, we just
  // ensure we only execute this code once.
  NS_ENSURE_TRUE(mRunCount == 0, NS_ERROR_UNEXPECTED);
  ++mRunCount;

  //AssertOnCacheThread();

  mCacheEntry = entry ? new _OldDescriptorWrapper(entry) : nullptr;
  mStatus = status;

  mNew = !entry || !(access & nsICache::ACCESS_READ);

  if (entry && !mNew) {
    bool valid; // what to do with this ? :)
    nsresult rv = mCallback->OnCacheEntryCheck(mCacheEntry, mAppCache, &valid);
    LOG(("OnCacheEntryCheck result ent=%p, cb=%p, appcache=%p, valid=%d, rv=0x%08x",
      mCacheEntry.get(), mCallback.get(), mAppCache.get(), valid, rv));

    if (NS_FAILED(rv)) {
      NS_WARNING("cache check failed");
      valid = false;
    }
  }

  return NS_DispatchToMainThread(this);
}

NS_IMETHODIMP
_OldGenericCacheLoad::OnCacheEntryDoomed(nsresult)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}




NS_IMETHODIMP
_OldApplicationCacheLoad::Run()
{
  nsresult rv;

  if (!NS_IsMainThread()) {
    //AssertOnCacheThread();

    nsAutoCString clientID;
    mAppCache->GetClientID(clientID);

    nsCOMPtr<nsICacheService> serv =
        do_GetService(NS_CACHESERVICE_CONTRACTID, &rv);
    nsCOMPtr<nsICacheSession> session;
    if (NS_SUCCEEDED(rv)) {
      rv = serv->CreateSession(clientID.get(),
                               nsICache::STORE_OFFLINE,
                               nsICache::STREAM_BASED,
                               getter_AddRefs(session));
    }
    if (NS_SUCCEEDED(rv)) {
      rv = session->SetIsPrivate(mStorage->LoadInfo()->IsPrivate());
    }
    if (NS_SUCCEEDED(rv)) {
      rv = session->SetDoomEntriesIfExpired(false);
    }
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIFile> profileDirectory;
      mAppCache->GetProfileDirectory(getter_AddRefs(profileDirectory));
      if (profileDirectory)
        rv = session->SetProfileDirectory(profileDirectory);
    }
    if (NS_SUCCEEDED(rv)) {
      nsAutoCString cacheKey;
      mURI->GetAsciiSpec(cacheKey);
      // AsyncOpenCacheEntry isn't really async when its called on the
      // cache service thread.
      nsCacheAccessMode cacheAccess;
      if (mTruncate)
        cacheAccess = nsICache::ACCESS_WRITE;
      else
        cacheAccess = nsICache::ACCESS_READ;

      rv = session->AsyncOpenCacheEntry(cacheKey, cacheAccess, this, false);
    }
    if (NS_FAILED(rv)) {
      rv = OnCacheEntryAvailable(nullptr, 0, rv);
    }
  } else {
    // break cycles
    nsCOMPtr<nsICacheEntryOpenCallback> cb = mCallback.forget();
    mCacheThread = nullptr;
    nsCOMPtr<nsICacheEntry> entry = mCacheEntry.forget();

    rv = cb->OnCacheEntryAvailable(entry, mNew, mAppCache, mStatus);
  }

  return rv;
}


}}
