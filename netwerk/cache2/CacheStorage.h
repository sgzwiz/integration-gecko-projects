/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheStorage__h__
#define CacheStorage__h__

#include "nsICacheStorage.h"

#include "nsCOMPtr.h"
#include "nsILoadContextInfo.h"
#include "nsIApplicationCache.h"

namespace mozilla {
namespace net {

class CacheStorage : public nsICacheStorage
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSICACHESTORAGE

public:
  CacheStorage(nsILoadContextInfo* aInfo,
               bool aAllowDisk,
               bool aLookupAppCache);
  CacheStorage(nsILoadContextInfo* aInfo,
               nsIApplicationCache* aAppCache);

private:
  virtual ~CacheStorage();

  nsCOMPtr<nsIApplicationCache> mAppCache;
  nsCOMPtr<nsILoadContextInfo> mLoadContextInfo;
  bool mWriteToDisk : 1;
  bool mLookupAppCache : 1;

public:
  nsIApplicationCache* AppCache() const { return mAppCache; }
  nsILoadContextInfo* LoadInfo() const { return mLoadContextInfo; }
  bool WriteToDisk() const { return mWriteToDisk; }
  bool LookupAppCache() const { return mLookupAppCache; }
};

} // net
} // mozilla

#endif
