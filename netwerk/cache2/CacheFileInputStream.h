/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheFileInputStream__h__
#define CacheFileInputStream__h__

#include "nsIAsyncInputStream.h"
#include "nsISeekableStream.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "CacheFileChunk.h"


namespace mozilla {
namespace net {

class CacheFile;

class CacheFileInputStream : public nsIAsyncInputStream
                           , public nsISeekableStream
                           , public CacheFileChunkListener
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSIASYNCINPUTSTREAM
  NS_DECL_NSISEEKABLESTREAM

public:
  CacheFileInputStream(CacheFile *aFile);

  nsresult OnChunkRead(nsresult aResult, CacheFileChunk *aChunk);
  nsresult OnChunkWritten(nsresult aResult, CacheFileChunk *aChunk);
  nsresult OnChunkAvailable(nsresult aResult, CacheFileChunk *aChunk);
  nsresult OnChunkUpdated(CacheFileChunk *aChunk);

private:
  virtual ~CacheFileInputStream();

  void ReleaseChunk();
  void EnsureCorrectChunk(bool aReleaseOnly);
  void CanRead(int64_t *aCanRead, const char **aBuf);
  void NotifyListener();

  nsRefPtr<CacheFile>      mFile;
  nsRefPtr<CacheFileChunk> mChunk;
  int64_t                  mPos;
  bool                     mClosed;
  nsresult                 mStatus;
  bool                     mWaitingForUpdate;

  nsCOMPtr<nsIInputStreamCallback> mCallback;
  uint32_t                         mCallbackFlags;
  nsCOMPtr<nsIEventTarget>         mCallbackTarget;
};


} // net
} // mozilla

#endif
