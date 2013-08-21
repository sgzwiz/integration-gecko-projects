/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_blockinghelperbase_h__
#define mozilla_dom_workers_blockinghelperbase_h__

#include "IPCThreadUtils.h"

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

class IDBObjectSync;
class IndexedDBRequestWorkerChildBase;

class BlockingHelperBase : public BlockWorkerThreadRunnable
{
public:
  typedef mozilla::dom::indexedDB::ipc::ResponseValue ResponseValue;
  typedef mozilla::dom::indexedDB::StructuredCloneReadInfo
                                                        StructuredCloneReadInfo;

  BlockingHelperBase(WorkerPrivate* aWorkerPrivate, IDBObjectSync* aObject)
  : BlockWorkerThreadRunnable(aWorkerPrivate), mPrimarySyncQueueKey(UINT32_MAX),
    mObject(aObject), mActorChild(nullptr)
  { }

  virtual
  ~BlockingHelperBase()
  {
    NS_ASSERTION(!mActorChild, "Still have an actor object attached!");
  }

  IDBObjectSync*
  Object() const
  {
    return mObject;
  }

  void
  SetActor(IndexedDBRequestWorkerChildBase* aActorChild)
  {
    NS_ASSERTION(!aActorChild || !mActorChild, "Shouldn't have more than one!");
    mActorChild = aActorChild;
  }

  void
  OnRequestComplete(const ResponseValue& aResponseValue);

  virtual nsresult
  HandleResponse(const ResponseValue& aResponseValue) = 0;

protected:
  /**
   * Helper to make a JS array object out of an array of clone buffers.
   */
  static nsresult
  ConvertToArrayAndCleanup(JSContext* aCx,
                           nsTArray<StructuredCloneReadInfo>& aReadInfos,
                           JS::MutableHandle<JS::Value> aResult);

  uint32_t mPrimarySyncQueueKey;
  IDBObjectSync* mObject;

  // Only touched on the IPC thread.
  IndexedDBRequestWorkerChildBase* mActorChild;
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_blockinghelperbase_h__
