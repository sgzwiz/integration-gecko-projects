/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheFileOutputStream.h"

#include "CacheFile.h"

namespace mozilla {
namespace net {

NS_IMPL_THREADSAFE_ADDREF(CacheFileOutputStream)
NS_IMETHODIMP_(nsrefcnt)
CacheFileOutputStream::Release()
{
  nsrefcnt count;
  NS_PRECONDITION(0 != mRefCnt, "dup release");
  count = NS_AtomicDecrementRefcnt(mRefCnt);
  NS_LOG_RELEASE(this, count, "CacheFileOutputStream");

  if (0 == count) {
    mRefCnt = 1;
    delete (this);
    return 0;
  }

  if (count == 1) {
    mFile->RemoveOutput(this);
    mFile = nullptr;
  }

  return count;
}

NS_INTERFACE_MAP_BEGIN(CacheFileOutputStream)
  NS_INTERFACE_MAP_ENTRY(nsIOutputStream)
  NS_INTERFACE_MAP_ENTRY(nsIAsyncOutputStream)
  NS_INTERFACE_MAP_ENTRY(nsISeekableStream)
  NS_INTERFACE_MAP_ENTRY(mozilla::net::CacheFileChunkListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIOutputStream)
NS_INTERFACE_MAP_END_THREADSAFE

CacheFileOutputStream::CacheFileOutputStream(CacheFile *aFile)
  : mFile(aFile)
  , mPos(0)
{
  MOZ_COUNT_CTOR(CacheFileOutputStream);
}

CacheFileOutputStream::~CacheFileOutputStream()
{
  MOZ_COUNT_DTOR(CacheFileOutputStream);
}

// nsIOutputStream
NS_IMETHODIMP
CacheFileOutputStream::Close()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CacheFileOutputStream::Flush()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CacheFileOutputStream::Write(const char * aBuf, uint32_t aCount, uint32_t *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CacheFileOutputStream::WriteFrom(nsIInputStream *aFromStream, uint32_t aCount, uint32_t *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CacheFileOutputStream::WriteSegments(nsReadSegmentFun aReader, void *aClosure, uint32_t aCount, uint32_t *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CacheFileOutputStream::IsNonBlocking(bool *_retval)
{
  *_retval = true;
  return NS_OK;
}

// nsIAsyncOutputStream
NS_IMETHODIMP
CacheFileOutputStream::CloseWithStatus(nsresult aStatus)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CacheFileOutputStream::AsyncWait(nsIOutputStreamCallback *aCallback,
                                 uint32_t aFlags,
                                 uint32_t aRequestedCount,
                                 nsIEventTarget *aEventTarget)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

// nsISeekableStream
NS_IMETHODIMP
CacheFileOutputStream::Seek(int32_t whence, int64_t offset)
{
  int64_t newPos = offset;
  switch (whence) {
  case NS_SEEK_SET:
    break;
  case NS_SEEK_CUR:
    newPos += mPos;
    break;
  case NS_SEEK_END:
    newPos += mFile->DataSize();
    break;
  default:
    NS_ERROR("invalid whence");
    return NS_ERROR_INVALID_ARG;
  }

  if (mChunk && (mPos / kChunkSize != newPos / kChunkSize)) {
    // new position points to a different chunk
    mChunk = nullptr;
  }

  mPos = newPos;
  return NS_OK;
}

NS_IMETHODIMP
CacheFileOutputStream::Tell(int64_t *_retval)
{
  *_retval = mPos;
  return NS_OK;
}

NS_IMETHODIMP
CacheFileOutputStream::SetEOF()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

// CacheFileChunkListener
nsresult
CacheFileOutputStream::OnChunkRead(nsresult aResult, CacheFileChunk *aChunk)
{
  MOZ_NOT_REACHED("CacheFileOutputStream::OnChunkRead should not be called!");
  return NS_ERROR_UNEXPECTED;
}

nsresult
CacheFileOutputStream::OnChunkWritten(nsresult aResult, CacheFileChunk *aChunk)
{
  MOZ_NOT_REACHED(
    "CacheFileOutputStream::OnChunkWritten should not be called!");
  return NS_ERROR_UNEXPECTED;
}

nsresult
CacheFileOutputStream::OnChunkAvailable(nsresult aResult, CacheFileChunk *aChunk)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
CacheFileOutputStream::OnChunkUpdated(CacheFileChunk *aChunk)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

} // net
} // mozilla
