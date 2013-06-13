/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheFileMetadata__h__
#define CacheFileMetadata__h__

#include "CacheFileIOManager.h"
#include "CacheHashUtils.h"
#include "nsAutoPtr.h"
#include "nsString.h"

namespace mozilla {
namespace net {

typedef struct {
  uint32_t        mFetchCount;
  uint32_t        mLastFetched;
  uint32_t        mLastModified;
  uint32_t        mExpirationTime;
  uint32_t        mKeySize;
} CacheFileMetadataHeader;

class CacheFileMetadataListener : public nsISupports
{
public:
  NS_IMETHOD OnMetadataRead(nsresult aResult) = 0;
  NS_IMETHOD OnMetadataWritten(nsresult aResult) = 0;
};

class CacheFileMetadata : public CacheFileIOListener
{
public:
  NS_DECL_ISUPPORTS

  CacheFileMetadata(CacheFileHandle *aHandle, const nsACString &aKey);

  nsresult ReadMetadata(CacheFileMetadataListener *aListener);
  nsresult WriteMetadata(uint32_t aOffset,
                         CacheFileMetadataListener *aListener);

  const char * GetElement(const char *aKey);
  nsresult     SetElement(const char *aKey, const char *aValue);

  CacheHashUtils::Hash16_t GetHash(uint32_t aIndex);
  nsresult                 SetHash(uint32_t aIndex,
                                   CacheHashUtils::Hash16_t aHash);

  nsresult SetExpirationTime(uint32_t aExpirationTime);
  nsresult GetExpirationTime(uint32_t *_retval);
  nsresult SetLastModified(uint32_t aLastModified);
  nsresult GetLastModified(uint32_t *_retval);
  nsresult GetLastFetched(uint32_t *_retval);
  nsresult GetFetchCount(uint32_t *_retval);

  int64_t  Offset() { return mOffset; }
  uint32_t ElementsSize() { return mElementsSize; }

  NS_IMETHOD OnFileOpened(CacheFileHandle *aHandle, nsresult aResult);
  NS_IMETHOD OnDataWritten(CacheFileHandle *aHandle, nsresult aResult);
  NS_IMETHOD OnDataRead(CacheFileHandle *aHandle, nsresult aResult);
  NS_IMETHOD OnFileDoomed(CacheFileHandle *aHandle, nsresult aResult);

private:
  virtual ~CacheFileMetadata();

  nsresult ParseMetadata(uint32_t aMetaOffset, uint32_t aBufOffset);
  nsresult CheckElements(const char *aBuf, uint32_t aSize);
  void     EnsureBuffer(uint32_t aSize);

  nsRefPtr<CacheFileHandle>           mHandle;
  nsCString                           mKey;
  CacheHashUtils::Hash16_t           *mHashArray;
  uint32_t                            mHashArraySize;
  uint32_t                            mHashCount;
  int64_t                             mOffset;
  char                               *mBuf; // used for parsing, then points
                                            // to elements
  uint32_t                            mBufSize;
  char                               *mWriteBuf;
  CacheFileMetadataHeader             mMetaHdr;
  uint32_t                            mElementsSize;
  nsCOMPtr<CacheFileMetadataListener> mListener;
};


} // net
} // mozilla

#endif
