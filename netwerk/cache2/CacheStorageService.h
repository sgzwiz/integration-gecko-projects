/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheStorageService__h__
#define CacheStorageService__h__

#include "nsICacheStorageService.h"

class nsIURI;
class nsICacheEntryOpenCallback;

namespace mozilla {
namespace net {

class CacheStorage;

class CacheStorageService : public nsICacheStorageService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICACHESTORAGESERVICE

  CacheStorageService();

  static CacheStorageService* Self() { return sSelf; }

private:
  virtual ~CacheStorageService();

  friend class CacheStorage;
  /**
   * Method responsible for opening a cache entry for the given URI. It selects
   * the proper mechanism to locate and keep the entry based on how aStorage had
   * been set up.
   */
  nsresult AsyncOpenCacheEntry(CacheStorage* aStorage,
                               nsIURI* aURI,
                               const nsACString & aIdExtension,
                               uint32_t aFlags,
                               nsICacheEntryOpenCallback* aCallback);

  nsresult ChooseApplicationCache(CacheStorage const* aStorage,
                                  nsIURI* aURI,
                                  nsIApplicationCache** aCache);

  static CacheStorageService* sSelf;
};

} // net
} // mozilla

#define NS_CACHE_STORAGE_SERVICE_CID \
  { 0xea70b098, 0x5014, 0x4e21, \
  { 0xae, 0xe1, 0x75, 0xe6, 0xb2, 0xc4, 0xb8, 0xe0 } } \

#define NS_CACHE_STORAGE_SERVICE_CONTRACTID \
  "@mozilla.org/netwerk/cache-storage-service;1"

#endif
