/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheFileOutputStream__h__
#define CacheFileOutputStream__h__

#include "nsIAsyncOutputStream.h"
#include "nsISeekableStream.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "CacheFileChunk.h"


namespace mozilla {
namespace net {

class CacheFile;

class CacheFileOutputStream : public nsIAsyncOutputStream
                            , public nsISeekableStream
                            , public CacheFileChunkListener
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOUTPUTSTREAM
  NS_DECL_NSIASYNCOUTPUTSTREAM
  NS_DECL_NSISEEKABLESTREAM

public:
  CacheFileOutputStream(CacheFile *aFile);

  nsresult OnChunkRead(nsresult aResult, CacheFileChunk *aChunk);
  nsresult OnChunkWritten(nsresult aResult, CacheFileChunk *aChunk);
  nsresult OnChunkAvailable(nsresult aResult, CacheFileChunk *aChunk);
  nsresult OnChunkUpdated(CacheFileChunk *aChunk);

private:
  virtual ~CacheFileOutputStream();

  nsRefPtr<CacheFile>      mFile;
  nsRefPtr<CacheFileChunk> mChunk;
  int64_t                  mPos;
};


} // net
} // mozilla

#endif
