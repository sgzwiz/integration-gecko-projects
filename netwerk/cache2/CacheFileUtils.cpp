/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheFileUtils.h"

#include "CacheFile.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS_INHERITED1(GapFiller, nsRunnable, CacheFileChunkListener)

GapFiller::GapFiller(uint32_t aStartChunk, uint32_t aEndChunk,
                     CacheFileChunkListener *aCB, CacheFile *aFile)
  : mStartChunk(aStartChunk)
  , mEndChunk(aEndChunk)
  , mListener(aCB)
  , mFile(aFile)
{
  MOZ_COUNT_CTOR(GapFiller);
}

GapFiller::~GapFiller()
{
  MOZ_COUNT_DTOR(GapFiller);
}

NS_IMETHODIMP
GapFiller::Run()
{
  nsresult rv;

  rv = mFile->GetChunk(mStartChunk, true, this);
  if (NS_FAILED(rv)) {
    return NotifyListener(rv, mStartChunk, nullptr);
  }

  return NS_OK;
}

nsresult
GapFiller::OnChunkRead(nsresult aResult, CacheFileChunk *aChunk)
{
  MOZ_NOT_REACHED("GapFiller::OnChunkRead should not be called!");
  return NS_ERROR_UNEXPECTED;
}

nsresult
GapFiller::OnChunkWritten(nsresult aResult, CacheFileChunk *aChunk)
{
  MOZ_NOT_REACHED("GapFiller::OnChunkWritten should not be called!");
  return NS_ERROR_UNEXPECTED;
}

nsresult
GapFiller::OnChunkAvailable(nsresult aResult, uint32_t aChunkIdx,
                            CacheFileChunk *aChunk)
{
  nsresult rv;

  if (NS_FAILED(aResult)) {
    return NotifyListener(aResult, aChunkIdx, nullptr);
  }

  if (aChunkIdx == mEndChunk) {
    return NotifyListener(NS_OK, aChunkIdx, aChunk);
  }

  {
    CacheFileAutoLock lock(mFile);
    memset(aChunk->Buf() + aChunk->DataSize(), 0,
           kChunkSize - aChunk->DataSize());
    aChunk->UpdateDataSize(kChunkSize, false);
  }

  rv = mFile->GetChunk(aChunkIdx + 1, true, this);
  if (NS_FAILED(rv)) {
    return NotifyListener(rv, aChunkIdx, nullptr);
  }

  return NS_OK;
}

nsresult
GapFiller::OnChunkUpdated(CacheFileChunk *aChunk)
{
  MOZ_NOT_REACHED("GapFiller::OnChunkUpdated should not be called!");
  return NS_ERROR_UNEXPECTED;
}

nsresult
GapFiller::NotifyListener(nsresult aResult, uint32_t aChunkIdx,
                          CacheFileChunk *aChunk)
{
  {
    CacheFileAutoLock lock(mFile);
    mFile->mGapFiller = nullptr;
  }
  nsCOMPtr<CacheFileChunkListener> listener;
  mListener.swap(listener);
  listener->OnChunkAvailable(aResult, aChunkIdx, aChunk);

  return NS_OK;
}


} // net
} // mozilla

