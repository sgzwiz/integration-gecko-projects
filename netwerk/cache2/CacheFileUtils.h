/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheFileUtils__h__
#define CacheFileUtils__h__

#include "CacheFileChunk.h"
#include "nsIRunnable.h"
#include "nsThreadUtils.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"

namespace mozilla {
namespace net {

class CacheFile;

class GapFiller : public nsRunnable
                , public CacheFileChunkListener
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIRUNNABLE

  GapFiller(uint32_t aStartChunk, uint32_t aEndChunk,
            CacheFileChunkListener *aCB, CacheFile *aFile);
  ~GapFiller();

  NS_IMETHOD OnChunkRead(nsresult aResult, CacheFileChunk *aChunk);
  NS_IMETHOD OnChunkWritten(nsresult aResult, CacheFileChunk *aChunk);
  NS_IMETHOD OnChunkAvailable(nsresult aResult, uint32_t aChunkIdx,
                              CacheFileChunk *aChunk);
  NS_IMETHOD OnChunkUpdated(CacheFileChunk *aChunk);

protected:
  nsresult NotifyListener(nsresult aResult, uint32_t aChunkIdx,
                          CacheFileChunk *aChunk);

  uint32_t                         mStartChunk;
  uint32_t                         mEndChunk;
  nsCOMPtr<CacheFileChunkListener> mListener;
  nsRefPtr<CacheFile>              mFile;
};

} // net
} // mozilla

#endif
