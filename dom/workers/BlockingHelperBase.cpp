/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BlockingHelperBase.h"

#include "nsIInputStream.h"

#include "mozilla/dom/indexedDB/FileInfo.h"
#include "mozilla/dom/indexedDB/IndexedDatabase.h"
#include "mozilla/dom/indexedDB/PIndexedDBRequest.h"

#include "WorkerPrivate.h"

#include "ipc/IndexedDBWorkerChild.h"

USING_WORKERS_NAMESPACE
using namespace mozilla::dom::indexedDB;

namespace {

class SendConstructorRunnable : public BlockWorkerThreadRunnable
{
public:
  SendConstructorRunnable(WorkerPrivate* aWorkerPrivate, uint32_t aSyncQueueKey,
                          BlockingHelperBase* aHelper)
  : BlockWorkerThreadRunnable(aWorkerPrivate), mSyncQueueKey(aSyncQueueKey),
    mHelper(aHelper)
  { }

protected:
  virtual nsresult
  IPCThreadRun() MOZ_OVERRIDE
  {
    const nsRefPtr<BlockingHelperProxy>& proxy = mHelper->Proxy();

    MOZ_ASSERT(!proxy->mWorkerPrivate, "Should be null!");
    proxy->mWorkerPrivate = mWorkerPrivate;

    MOZ_ASSERT(proxy->mSyncQueueKey == UINT32_MAX, "Should be unset!");
    proxy->mSyncQueueKey = mSyncQueueKey;

    IndexedDBRequestWorkerChildBase* actor;
    nsresult rv = mHelper->SendConstructor(&actor);
    NS_ENSURE_SUCCESS(rv, rv);

    actor->SetHelperProxy(proxy);

    proxy->mExpectingResponse = true;

    return NS_OK;
  }

  uint32_t mSyncQueueKey;
  nsRefPtr<BlockingHelperBase> mHelper;
};

// This inline is just so that we always clear aBuffers appropriately even if
// something fails.
inline
nsresult
ConvertCloneReadInfosToArrayInternal(
                                  JSContext* aCx,
                                  nsTArray<StructuredCloneReadInfo>& aReadInfos,
                                  JS::MutableHandle<JS::Value> aResult)
{
  JS::Rooted<JSObject*> array(aCx, JS_NewArrayObject(aCx, 0, nullptr));
  if (!array) {
    NS_WARNING("Failed to make array!");
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  if (!aReadInfos.IsEmpty()) {
    if (!JS_SetArrayLength(aCx, array, uint32_t(aReadInfos.Length()))) {
      NS_WARNING("Failed to set array length!");
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    for (uint32_t index = 0, count = aReadInfos.Length(); index < count;
         index++) {
      StructuredCloneReadInfo& readInfo = aReadInfos[index];

      JS::Rooted<JS::Value> val(aCx);
      JSAutoStructuredCloneBuffer& buffer = readInfo.mCloneBuffer;
      if (!buffer.data()) {
        val = JSVAL_VOID;
      }
      else {
        buffer.read(aCx, &val, nullptr, nullptr);
      }

      if (!JS_SetElement(aCx, array, index, &val)) {
        NS_WARNING("Failed to set array element!");
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }
    }
  }

  aResult.setObject(*array);
  return NS_OK;
}

} // anonymous namespace

BlockingHelperProxy::BlockingHelperProxy(BlockingHelperBase* aHelper)
: IDBObjectSyncProxyWithActor<IndexedDBRequestWorkerChildBase>(aHelper)
{
}

void
BlockingHelperProxy::Teardown()
{
  AssertIsOnIPCThread();
  if (mActor) {
    MaybeUnpinObject();

    mActor->Disconnect();
    MOZ_ASSERT(!mActor);
  }
}

BlockingHelperBase*
BlockingHelperProxy::Helper()
{
  return static_cast<BlockingHelperBase*>(mObject);
}

void
BlockingHelperProxy::OnRequestComplete(const ResponseValue& aResponseValue)
{
  nsresult rv;

  if (aResponseValue.type() == ResponseValue::Tnsresult) {
    MOZ_ASSERT(NS_FAILED(aResponseValue.get_nsresult()), "Huh?");
    rv = aResponseValue.get_nsresult();
  }
  else {
    rv = Helper()->UnpackResponse(aResponseValue);
  }

  UnblockWorkerThread(rv, false);
}

bool
BlockingHelperBase::Run(JSContext* aCx)
{
  mProxy = new BlockingHelperProxy(this);

  Pin();

  AutoUnpin autoUnpin(this);
  AutoSyncLoopHolder syncLoop(mWorkerPrivate);

  nsRefPtr<SendConstructorRunnable> runnable =
    new SendConstructorRunnable(mWorkerPrivate, syncLoop.SyncQueueKey(), this);

  if (!runnable->Dispatch(aCx)) {
    ReleaseProxy();
    return false;
  }

  autoUnpin.Forget();

  if (!syncLoop.RunAndForget(aCx)) {
    return false;
  }

  return true;
}

// static
nsresult
BlockingHelperBase::ConvertToArrayAndCleanup(
                                  JSContext* aCx,
                                  nsTArray<StructuredCloneReadInfo>& aReadInfos,
                                  JS::MutableHandle<JS::Value> aResult)
{
  MOZ_ASSERT(aCx, "Null context!");
  MOZ_ASSERT(aResult.address(), "Null pointer!");

  nsresult rv = ConvertCloneReadInfosToArrayInternal(aCx, aReadInfos, aResult);

  for (uint32_t index = 0; index < aReadInfos.Length(); index++) {
    aReadInfos[index].mCloneBuffer.clear();
  }
  aReadInfos.Clear();

  return rv;
}
