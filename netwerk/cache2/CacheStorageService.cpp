/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheStorageService.h"
#include "CacheLog.h"

#include "CacheStorage.h"
#include "OldWrappers.h"

#include "nsIURI.h"
#include "nsIApplicationCache.h"
#include "nsIApplicationCacheService.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS1(CacheStorageService, nsICacheStorageService)

CacheStorageService* CacheStorageService::sSelf = nullptr;

CacheStorageService::CacheStorageService()
{
  sSelf = this;
}

CacheStorageService::~CacheStorageService()
{
  sSelf = nullptr;
}

// nsICacheStorageService

NS_IMETHODIMP CacheStorageService::MemoryCacheStorage(nsILoadContextInfo *aLoadContextInfo,
                                                      nsICacheStorage * *_retval)
{
  NS_ENSURE_ARG(aLoadContextInfo);
  NS_ENSURE_ARG(_retval);

  nsRefPtr<CacheStorage> storage =
    new CacheStorage(aLoadContextInfo, false, false);

  storage.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP CacheStorageService::DiskCacheStorage(nsILoadContextInfo *aLoadContextInfo,
                                                    bool aLookupAppCache,
                                                    nsICacheStorage * *_retval)
{
  NS_ENSURE_ARG(aLoadContextInfo);
  NS_ENSURE_ARG(_retval);

  // TODO save some heap granularity - cache commonly used storages.

  nsRefPtr<CacheStorage> storage =
    new CacheStorage(aLoadContextInfo, true, aLookupAppCache);

  storage.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP CacheStorageService::AppCacheStorage(nsILoadContextInfo *aLoadContextInfo,
                                                   nsIApplicationCache *aApplicationCache,
                                                   nsICacheStorage * *_retval)
{
  NS_ENSURE_ARG(aLoadContextInfo);
  NS_ENSURE_ARG(_retval);

  nsRefPtr<CacheStorage> storage =
    new CacheStorage(aLoadContextInfo, aApplicationCache);

  storage.forget(_retval);
  return NS_OK;
}

// Methods exposed to and used by CacheStorage.

nsresult
CacheStorageService::AsyncOpenCacheEntry(CacheStorage* aStorage,
                                         nsIURI* aURI,
                                         const nsACString & aIdExtension,
                                         uint32_t aFlags,
                                         nsICacheEntryOpenCallback* aCallback)
{
  nsresult rv;

  bool truncate = !!(aFlags & nsICacheStorage::OPEN_TRUNCATE);

  nsCOMPtr<nsIURI> noRefURI;
  rv = aURI->CloneIgnoringRef(getter_AddRefs(noRefURI));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIApplicationCache> appCache;
  if (aStorage->LookupAppCache()) {
    MOZ_ASSERT((aFlags & nsICacheStorage::OPEN_TRUNCATE) == 0);

    rv = ChooseApplicationCache(aStorage, noRefURI, getter_AddRefs(appCache));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!appCache)
    appCache = aStorage->AppCache();

  if (appCache) {
    nsRefPtr<_OldApplicationCacheLoad> appCacheLoad =
      new _OldApplicationCacheLoad(noRefURI, aCallback, appCache, aStorage, truncate);
    rv = appCacheLoad->Start();
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    nsRefPtr<_OldGenericCacheLoad> cacheLoad =
      new _OldGenericCacheLoad(noRefURI, aCallback, aStorage, truncate);
    rv = cacheLoad->Start();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
CacheStorageService::ChooseApplicationCache(CacheStorage const* aStorage,
                                            nsIURI* aURI,
                                            nsIApplicationCache** aCache)
{
  nsresult rv;

  nsCOMPtr<nsIApplicationCacheService> appCacheService =
    do_GetService(NS_APPLICATIONCACHESERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString cacheKey;
  rv = aURI->GetAsciiSpec(cacheKey);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = appCacheService->ChooseApplicationCache(cacheKey, aStorage->LoadInfo(), aCache);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

} // net
} // mozilla
