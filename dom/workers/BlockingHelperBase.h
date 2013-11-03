/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_blockinghelperbase_h__
#define mozilla_dom_workers_blockinghelperbase_h__

#include "IPCThreadUtils.h"
#include "IDBObjectSync.h"

namespace mozilla {
namespace dom {
namespace indexedDB {
struct StructuredCloneReadInfo;
namespace ipc {
class ResponseValue;
} // namespace ipc
} // namespace indexedDB
} // namespace dom
} // namespace mozilla

BEGIN_WORKERS_NAMESPACE

class BlockingHelperBase;
class IDBObjectSync;
class IndexedDBRequestWorkerChildBase;

class BlockingHelperProxy : public IDBObjectSyncProxyWithActor<IndexedDBRequestWorkerChildBase>
{
public:
  typedef mozilla::dom::indexedDB::ipc::ResponseValue ResponseValue;

  BlockingHelperProxy(BlockingHelperBase* aHelper);

  virtual void
  Teardown() MOZ_OVERRIDE;

  BlockingHelperBase*
  Helper();

  void
  OnRequestComplete(const ResponseValue& aResponseValue);
};

class BlockingHelperBase : public IDBObjectSyncBase
{
public:
  typedef mozilla::dom::indexedDB::ipc::ResponseValue ResponseValue;
  typedef mozilla::dom::indexedDB::StructuredCloneReadInfo
                                                        StructuredCloneReadInfo;

  NS_INLINE_DECL_REFCOUNTING(BlockingHelperBase)

  BlockingHelperProxy*
  Proxy() const
  {
    return static_cast<BlockingHelperProxy*>(mProxy.get());
  }

  virtual IDBObjectSync*
  Object() const
  {
    return mObject;
  }

  bool
  Run(JSContext* aCx);

  virtual nsresult
  SendConstructor(IndexedDBRequestWorkerChildBase** aActor) = 0;

  virtual nsresult
  UnpackResponse(const ResponseValue& aResponseValu) = 0;

protected:
  BlockingHelperBase(WorkerPrivate* aWorkerPrivate,
                     IDBObjectSync* aObject)
  : IDBObjectSyncBase(aWorkerPrivate), mObject(aObject)
  { }

  /**
   * Helper to make a JS array object out of an array of clone buffers.
   */
  static nsresult
  ConvertToArrayAndCleanup(JSContext* aCx,
                           nsTArray<StructuredCloneReadInfo>& aReadInfos,
                           JS::MutableHandle<JS::Value> aResult);

  nsRefPtr<IDBObjectSync> mObject;
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_blockinghelperbase_h__
