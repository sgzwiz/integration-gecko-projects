/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheFileChunk__h__
#define CacheFileChunk__h__

#include "CacheFileIOManager.h"
#include "CacheHashUtils.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"
#include "mozilla/Mutex.h"

namespace mozilla {
namespace net {

#define kChunkSize   4096

class CacheFileChunk;
class CacheFile;

class CacheFileChunkListener : public nsISupports
{
public:
  NS_IMETHOD OnChunkRead(nsresult aResult, CacheFileChunk *aChunk) = 0;
  NS_IMETHOD OnChunkWritten(nsresult aResult, CacheFileChunk *aChunk) = 0;
  NS_IMETHOD OnChunkAvailable(nsresult aResult, CacheFileChunk *aChunk) = 0;
  NS_IMETHOD OnChunkUpdated(CacheFileChunk *aChunk) = 0;
};

class ChunkListenerItem {
public:
  ChunkListenerItem()  { MOZ_COUNT_CTOR(ChunkListenerItem); }
  ~ChunkListenerItem() { MOZ_COUNT_DTOR(ChunkListenerItem); }

  nsCOMPtr<nsIEventTarget>         mTarget;
  nsCOMPtr<CacheFileChunkListener> mCallback;
};

class ChunkListeners {
public:
  ChunkListeners()  { MOZ_COUNT_CTOR(ChunkListeners); }
  ~ChunkListeners() { MOZ_COUNT_DTOR(ChunkListeners); }

  nsTArray<ChunkListenerItem *> mItems;
};

class CacheFileChunk : public CacheFileIOListener
{
public:
  NS_DECL_ISUPPORTS

  CacheFileChunk(CacheFile *aFile, uint32_t aIndex);

  void     InitNew(CacheFileChunkListener *aCallback);
  nsresult Read(CacheFileHandle *aHandle, uint32_t aLen,
                CacheFileChunkListener *aCallback);
  nsresult Write(CacheFileHandle *aHandle, CacheFileChunkListener *aCallback);
  void     WaitForUpdate(CacheFileChunkListener *aCallback);
  nsresult CancelWait(CacheFileChunkListener *aCallback);
  nsresult NotifyUpdateListeners();

  uint32_t                 Index();
  CacheHashUtils::Hash16_t Hash();
  uint32_t                 DataSize();

  nsresult OnFileOpened(CacheFileHandle *aHandle, nsresult aResult);
  nsresult OnDataWritten(CacheFileHandle *aHandle, nsresult aResult);
  nsresult OnDataRead(CacheFileHandle *aHandle, nsresult aResult);
  nsresult OnFileDoomed(CacheFileHandle *aHandle, nsresult aResult);

  bool   IsReady();
  void   SetReady(bool aReady);
  bool   IsDirty();
  char * Buf();

private:
  friend class GapFiller;
  friend class CacheFileInputStream;
  friend class CacheFileOutputStream;
  friend class CacheFile;

  virtual ~CacheFileChunk();

  uint32_t        mIndex;
  bool            mIsReady;
  bool            mIsDirty;
  bool            mRemovingChunk;
  uint32_t        mDataSize;
  char           *mBuf;

  nsRefPtr<CacheFile>              mFile;
  nsCOMPtr<CacheFileChunkListener> mListener;
  nsTArray<ChunkListenerItem *>    mUpdateListeners;
};


} // net
} // mozilla

#endif
