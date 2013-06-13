/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheFile__h__
#define CacheFile__h__

#include "CacheFileChunk.h"
#include "CacheFileIOManager.h"
#include "CacheFileMetadata.h"
#include "nsTArray.h"
#include "nsRefPtrHashtable.h"
#include "nsClassHashtable.h"
#include "mozilla/Mutex.h"

class nsIInputStream;
class nsIOutputStream;

namespace mozilla {
namespace net {

class CacheFileInputStream;
class CacheFileOutputStream;
class GapFiller;

class CacheFileListener : public nsISupports
{
public:
  NS_IMETHOD OnFileReady(nsresult aResult) = 0;
  NS_IMETHOD OnFileDoomed(nsresult aResult) = 0;
};

class CacheFile : public CacheFileChunkListener
                , public CacheFileIOListener
                , public CacheFileMetadataListener
{
public:
  NS_DECL_ISUPPORTS

  CacheFile();

  nsresult Init(const nsACString &aKey,
                bool aCreateNew,
                CacheFileListener *aCallback);

  NS_IMETHOD OnChunkRead(nsresult aResult, CacheFileChunk *aChunk);
  NS_IMETHOD OnChunkWritten(nsresult aResult, CacheFileChunk *aChunk);
  NS_IMETHOD OnChunkAvailable(nsresult aResult, CacheFileChunk *aChunk);
  NS_IMETHOD OnChunkUpdated(CacheFileChunk *aChunk);

  NS_IMETHOD OnFileOpened(CacheFileHandle *aHandle, nsresult aResult);
  NS_IMETHOD OnDataWritten(CacheFileHandle *aHandle, nsresult aResult);
  NS_IMETHOD OnDataRead(CacheFileHandle *aHandle, nsresult aResult);
  NS_IMETHOD OnFileDoomed(CacheFileHandle *aHandle, nsresult aResult);

  NS_IMETHOD OnMetadataRead(nsresult aResult);
  NS_IMETHOD OnMetadataWritten(nsresult aResult);

  NS_IMETHOD OpenInputStream(nsIInputStream **_retval);
  NS_IMETHOD OpenOutputStream(nsIOutputStream **_retval);
  NS_IMETHOD SetMemoryOnly(bool aMemoryOnly);
  NS_IMETHOD Doom(CacheFileListener *aCallback);

  mozilla::Mutex *GetLock();
  CacheFileMetadata* Metadata();
  int64_t DataSize();

private:
  friend class CacheFileChunk;
  friend class CacheFileInputStream;
  friend class CacheFileOutputStream;
  friend class GapFiller;

  virtual ~CacheFile();

  void     AssertOwnsLock();
  nsresult GetChunk(uint32_t aIndex, bool aWriter,
                    CacheFileChunkListener *aCallback);
  nsresult GetChunkLocked(uint32_t aIndex, bool aWriter,
                          CacheFileChunkListener *aCallback);
  nsresult RemoveChunk(CacheFileChunk *aChunk);

  nsresult RemoveInput(CacheFileInputStream *aInput);
  nsresult RemoveOutput(CacheFileOutputStream *aOutput);
  nsresult NotifyChunkListener(CacheFileChunkListener *aCallback,
                               nsIEventTarget *aTarget,
                               nsresult aResult,
                               CacheFileChunk *aChunk);
  nsresult QueueChunkListener(uint32_t aIndex,
                              CacheFileChunkListener *aCallback);
  nsresult NotifyChunkListeners(uint32_t aIndex, nsresult aResult,
                                CacheFileChunk *aChunk);


  mozilla::Mutex mLock;
  bool           mReady;
  bool           mMemoryOnly;
  bool           mDataAccessed;
  int64_t        mDataSize;
  nsCString      mKey;

  nsRefPtr<CacheFileHandle>   mHandle;
  nsRefPtr<CacheFileMetadata> mMetadata;
  nsCOMPtr<CacheFileListener> mListener;
  nsRefPtr<GapFiller>         mGapFiller;

  nsRefPtrHashtable<nsUint32HashKey, CacheFileChunk> mChunks;
  nsClassHashtable<nsUint32HashKey, ChunkListeners> mChunkListeners;

  nsTArray<nsRefPtr<CacheFileInputStream> > mInputs;
  nsRefPtr<CacheFileOutputStream> mOutput;
};

} // net
} // mozilla

#endif
