/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheStorage.h"
#include "CacheStorageService.h"
#include "nsProxyRelease.h"

namespace mozilla {
namespace net {

NS_IMPL_THREADSAFE_ISUPPORTS1(CacheStorage, nsICacheStorage)

CacheStorage::CacheStorage(nsILoadContextInfo* aInfo,
                           bool aAllowDisk,
                           bool aLookupAppCache)
: mLoadContextInfo(aInfo)
, mWriteToDisk(aAllowDisk)
, mLookupAppCache(aLookupAppCache)
{
}

CacheStorage::CacheStorage(nsILoadContextInfo* aInfo,
                           nsIApplicationCache* aAppCache)
: mAppCache(aAppCache)
, mLoadContextInfo(aInfo)
, mWriteToDisk(true)
, mLookupAppCache(false)
{
}

CacheStorage::~CacheStorage()
{
  nsCOMPtr<nsIThread> mainThread;
  NS_GetMainThread(getter_AddRefs(mainThread));

  nsIApplicationCache* appcache;
  mAppCache.forget(&appcache);
  NS_ProxyRelease(mainThread, appcache);
}

NS_IMETHODIMP CacheStorage::AsyncOpenURI(nsIURI *aURI,
                                         const nsACString & aIdExtension,
                                         uint32_t aFlags,
                                         nsICacheEntryOpenCallback *aCallback)
{
  if (!CacheStorageService::Self())
    return NS_ERROR_NOT_INITIALIZED;

  CacheStorageService::Self()->AsyncOpenCacheEntry(this, aURI, aIdExtension, aFlags, aCallback);
  return NS_OK;
}

NS_IMETHODIMP CacheStorage::AsyncDoomURI(nsIURI *aURI, const nsACString & aIdExtension)
{
  if (!CacheStorageService::Self())
    return NS_ERROR_NOT_INITIALIZED;

  return NS_OK;
}

NS_IMETHODIMP CacheStorage::AsyncEvictStorage()
{
  if (!CacheStorageService::Self())
    return NS_ERROR_NOT_INITIALIZED;

  return NS_OK;
}

} // net
} // mozilla
