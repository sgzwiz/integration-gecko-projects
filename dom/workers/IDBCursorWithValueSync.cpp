/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBCursorWithValueSync.h"

#include "mozilla/dom/indexedDB/IDBKeyRange.h"

#include "BlockingHelperBase.h"
#include "DatabaseInfoMT.h"
#include "DOMBindingInlines.h"
#include "IDBIndexSync.h"
#include "IDBObjectStoreSync.h"
#include "IPCThreadUtils.h"
#include "WorkerPrivate.h"

#include "ipc/IndexedDBWorkerChild.h"

USING_WORKERS_NAMESPACE
using namespace mozilla::dom::indexedDB::ipc;
using mozilla::dom::indexedDB::IDBKeyRange;
using mozilla::ErrorResult;

BEGIN_WORKERS_NAMESPACE

class OpenHelperWithValue : public BlockingHelperBase
{
public:
  OpenHelperWithValue(WorkerPrivate* aWorkerPrivate, uint32_t aSyncQueueKey,
                      IDBCursorWithValueSync* aCursor, IDBKeyRange* aKeyRange)
  : BlockingHelperBase(aWorkerPrivate, aCursor), mSyncQueueKey(aSyncQueueKey),
    mCursor(aCursor), mKeyRange(aKeyRange)
  { }

  virtual nsresult
  HandleResponse(const ResponseValue& aResponseValue);

protected:
  uint32_t mSyncQueueKey;
  IDBCursorWithValueSync* mCursor;

  // In-params.
  nsRefPtr<IDBKeyRange> mKeyRange;
};

class OpenIndexCursorHelper : public OpenHelperWithValue
{
public:
  OpenIndexCursorHelper(WorkerPrivate* aWorkerPrivate, uint32_t aSyncQueueKey,
                        IDBCursorWithValueSync* aCursor, IDBKeyRange* aKeyRange)
  : OpenHelperWithValue(aWorkerPrivate, aSyncQueueKey, aCursor, aKeyRange)
  { }

protected:
  nsresult
  IPCThreadRun()
  {
    MOZ_ASSERT(mPrimarySyncQueueKey == UINT32_MAX, "Should be unset!");
    mPrimarySyncQueueKey = mSyncQueueKey;

    IndexRequestParams params;

    OpenCursorParams openCursorParams;
    if (mKeyRange) {
      KeyRange keyRange;
      mKeyRange->ToSerializedKeyRange(keyRange);
      openCursorParams.optionalKeyRange() = keyRange;
    }
    else {
      openCursorParams.optionalKeyRange() = mozilla::void_t();
    }
    openCursorParams.direction() = mCursor->GetDirection();

    params = openCursorParams;

    IndexedDBIndexRequestWorkerChild* actor =
      new IndexedDBIndexRequestWorkerChild(params.type());
    mCursor->Index()->GetActor()->SendPIndexedDBRequestConstructor(actor,
                                                                   params);
    actor->SetHelper(this);

    return NS_OK;
  }
};

class OpenObjectStoreCursorHelper : public OpenHelperWithValue
{
public:
  OpenObjectStoreCursorHelper(
                        WorkerPrivate* aWorkerPrivate, uint32_t aSyncQueueKey,
                        IDBCursorWithValueSync* aCursor, IDBKeyRange* aKeyRange)
  : OpenHelperWithValue(aWorkerPrivate, aSyncQueueKey, aCursor, aKeyRange)
  { }

protected:
  nsresult
  IPCThreadRun()
  {
    MOZ_ASSERT(mPrimarySyncQueueKey == UINT32_MAX, "Should be unset!");
    mPrimarySyncQueueKey = mSyncQueueKey;

    ObjectStoreRequestParams params;

    OpenCursorParams openCursorParams;
    if (mKeyRange) {
      KeyRange keyRange;
      mKeyRange->ToSerializedKeyRange(keyRange);
      openCursorParams.optionalKeyRange() = keyRange;
    }
    else {
      openCursorParams.optionalKeyRange() = mozilla::void_t();
    }
    openCursorParams.direction() = mCursor->GetDirection();

    params = openCursorParams;

    IndexedDBObjectStoreRequestWorkerChild* actor =
      new IndexedDBObjectStoreRequestWorkerChild(params.type());
    mCursor->ObjectStore()->GetActor()->SendPIndexedDBRequestConstructor(
                                                                        actor,
                                                                        params);
    actor->SetHelper(this);

    return NS_OK;
  }
};

END_WORKERS_NAMESPACE

// For INDEXOBJECT cursors.
// static
IDBCursorWithValueSync*
IDBCursorWithValueSync::Create(JSContext* aCx, IDBIndexSync* aIndex,
                               Direction aDirection)
{
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);

  nsRefPtr<IDBCursorWithValueSync> cursor =
    new IDBCursorWithValueSync(aCx, workerPrivate);

  cursor->mObjectStore = aIndex->ObjectStore();
  cursor->mIndex = aIndex;
  cursor->mTransaction = aIndex->ObjectStore()->Transaction();
  cursor->mType = INDEXOBJECT;
  cursor->mDirection = aDirection;

  if (!Wrap(aCx, nullptr, cursor)) {
    return nullptr;
  }

  return cursor;
}

// For OBJECTSTORE cursors.
//static
IDBCursorWithValueSync*
IDBCursorWithValueSync::Create(JSContext* aCx, IDBObjectStoreSync* aObjectStore,
                               Direction aDirection)
{
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);

  nsRefPtr<IDBCursorWithValueSync> cursor =
    new IDBCursorWithValueSync(aCx, workerPrivate);

  cursor->mObjectStore = aObjectStore;
  cursor->mTransaction = aObjectStore->Transaction();
  cursor->mType = OBJECTSTORE;
  cursor->mDirection = aDirection;

  if (!Wrap(aCx, nullptr, cursor)) {
    return nullptr;
  }

  return cursor;
}

IDBCursorWithValueSync::IDBCursorWithValueSync(JSContext* aCx,
                                               WorkerPrivate* aWorkerPrivate)
: IDBCursorSync(aCx, aWorkerPrivate)
{ }

IDBCursorWithValueSync::~IDBCursorWithValueSync()
{ }

void
IDBCursorWithValueSync::_trace(JSTracer* aTrc)
{
  JS_CallHeapValueTracer(aTrc, &mCachedValue, "mCachedValue");

  IDBCursorSync::_trace(aTrc);
}

void
IDBCursorWithValueSync::_finalize(JSFreeOp* aFop)
{
  IDBCursorSync::_finalize(aFop);
}

NS_IMPL_ISUPPORTS_INHERITED0(IDBCursorWithValueSync, IDBCursorSync)

JS::Value
IDBCursorWithValueSync::GetValue(JSContext* aCx, ErrorResult& aRv)
{
  MOZ_ASSERT(mType == OBJECTSTORE || mType == INDEXOBJECT);

  if (!mHaveValue) {
    return JSVAL_VOID;
  }

  if (!mHaveCachedValue) {
    JS::Rooted<JS::Value> val(aCx);
    if (!IDBObjectStoreSync::DeserializeValue(aCx, mCloneReadInfo.mCloneBuffer,
                                              &val)) {
      NS_WARNING("DeserializeValue failed!");
      aRv.Throw(NS_ERROR_DOM_DATA_CLONE_ERR);
      return JSVAL_VOID;
    }

    mCloneReadInfo.mCloneBuffer.clear();

    mCachedValue = val;
    mHaveCachedValue = true;
  }

  return mCachedValue;
}

bool
IDBCursorWithValueSync::Open(JSContext* aCx, IDBKeyRange* aKeyRange)
{
  MOZ_ASSERT(mType != INDEXKEY,
             "Open Value cursor shouldn't exist on index keys");

  DOMBindingAnchor<IDBCursorWithValueSync> selfAnchor(this);

  AutoSyncLoopHolder syncLoop(mWorkerPrivate);

  nsRefPtr<OpenHelperWithValue> helper;
  if (mType == INDEXOBJECT) {
    helper = new OpenIndexCursorHelper(mWorkerPrivate, syncLoop.SyncQueueKey(),
                                       this, aKeyRange);
  }
  else {
    helper = new OpenObjectStoreCursorHelper(mWorkerPrivate,
                                             syncLoop.SyncQueueKey(), this,
                                             aKeyRange);
  }

  if (!helper->Dispatch(aCx) || !syncLoop.RunAndForget(aCx)) {
    NS_WARNING("Failed to Dispatch or RunAndForget!");
    return false;
  }

  return true;
}

nsresult
OpenHelperWithValue::HandleResponse(const ResponseValue& aResponseValue)
{
  AssertIsOnIPCThread();

  MOZ_ASSERT(aResponseValue.type() == ResponseValue::TOpenCursorResponse,
             "Bad response type!");
  MOZ_ASSERT(aResponseValue.get_OpenCursorResponse().type() ==
             OpenCursorResponse::Tvoid_t ||
             aResponseValue.get_OpenCursorResponse().type() ==
             OpenCursorResponse::TPIndexedDBCursorChild,
             "Bad response union type!");

  const OpenCursorResponse& response =
    aResponseValue.get_OpenCursorResponse();

  switch (response.type()) {
    case OpenCursorResponse::Tvoid_t: {
      mozilla::dom::indexedDB::Key emptyKey;
      mozilla::dom::indexedDB::SerializedStructuredCloneReadInfo cloneInfo;
      mCursor->SetCurrentKeysAndValue(emptyKey, emptyKey, cloneInfo);
    } break;

    case OpenCursorResponse::TPIndexedDBCursorChild:
      break;

    default:
      NS_NOTREACHED("Unknown response union type!");
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  return NS_OK;
}
