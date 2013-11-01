/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBCursorSync.h"

#include "mozilla/dom/indexedDB/IDBKeyRange.h"
#include "mozilla/dom/indexedDB/IndexedDatabaseInlines.h"
#include "mozilla/dom/indexedDB/Key.h"

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
using mozilla::dom::Optional;
using mozilla::ErrorResult;

BEGIN_WORKERS_NAMESPACE

class OpenHelper : public BlockingHelperBase
{
public:
  OpenHelper(WorkerPrivate* aWorkerPrivate, uint32_t aSyncQueueKey,
             IDBCursorSync* aCursor, IDBKeyRange* aKeyRange)
  : BlockingHelperBase(aWorkerPrivate, aCursor), mSyncQueueKey(aSyncQueueKey),
    mCursor(aCursor), mKeyRange(aKeyRange)
  { }

  virtual nsresult
  HandleResponse(const ResponseValue& aResponseValue);

protected:
  uint32_t mSyncQueueKey;
  IDBCursorSync* mCursor;

  // In-params.
  nsRefPtr<IDBKeyRange> mKeyRange;
};

class OpenIndexKeyCursorHelper : public OpenHelper
{
public:
  OpenIndexKeyCursorHelper(WorkerPrivate* aWorkerPrivate, uint32_t aSyncQueueKey,
                           IDBCursorSync* aCursor, IDBKeyRange* aKeyRange)
  : OpenHelper(aWorkerPrivate, aSyncQueueKey, aCursor, aKeyRange)
  { }

protected:
  nsresult
  IPCThreadRun()
  {
    MOZ_ASSERT(mPrimarySyncQueueKey == UINT32_MAX, "Should be unset!");
    mPrimarySyncQueueKey = mSyncQueueKey;

    IndexRequestParams params;

    OpenKeyCursorParams openKeyCursorParams;
    if (mKeyRange) {
      KeyRange keyRange;
      mKeyRange->ToSerializedKeyRange(keyRange);
      openKeyCursorParams.optionalKeyRange() = keyRange;
    }
    else {
      openKeyCursorParams.optionalKeyRange() = mozilla::void_t();
    }
    openKeyCursorParams.direction() = mCursor->GetDirection();

    params = openKeyCursorParams;

    IndexedDBIndexRequestWorkerChild* actor =
      new IndexedDBIndexRequestWorkerChild(params.type());
    mCursor->Index()->GetActor()->SendPIndexedDBRequestConstructor(actor,
                                                                   params);
    actor->SetHelper(this);

    return NS_OK;
  }
};

class OpenObjectStoreKeyCursorHelper : public OpenHelper
{
public:
  OpenObjectStoreKeyCursorHelper(WorkerPrivate* aWorkerPrivate,
                                 uint32_t aSyncQueueKey, IDBCursorSync* aCursor,
                                 IDBKeyRange* aKeyRange)
  : OpenHelper(aWorkerPrivate, aSyncQueueKey, aCursor, aKeyRange)
  { }

protected:
  nsresult
  IPCThreadRun()
  {
    MOZ_ASSERT(mPrimarySyncQueueKey == UINT32_MAX, "Should be unset!");
    mPrimarySyncQueueKey = mSyncQueueKey;

    ObjectStoreRequestParams params;

    OpenKeyCursorParams openKeyCursorParams;
    if (mKeyRange) {
      KeyRange keyRange;
      mKeyRange->ToSerializedKeyRange(keyRange);
      openKeyCursorParams.optionalKeyRange() = keyRange;
    }
    else {
      openKeyCursorParams.optionalKeyRange() = mozilla::void_t();
    }
    openKeyCursorParams.direction() = mCursor->GetDirection();

    params = openKeyCursorParams;

    IndexedDBObjectStoreRequestWorkerChild* actor =
      new IndexedDBObjectStoreRequestWorkerChild(params.type());
    mCursor->ObjectStore()->GetActor()->SendPIndexedDBRequestConstructor(
                                                                        actor,
                                                                        params);
    actor->SetHelper(this);

    return NS_OK;
  }
};

class OpenIndexCursorHelper : public OpenHelper
{
public:
  OpenIndexCursorHelper(WorkerPrivate* aWorkerPrivate, uint32_t aSyncQueueKey,
                        IDBCursorSync* aCursor, IDBKeyRange* aKeyRange)
  : OpenHelper(aWorkerPrivate, aSyncQueueKey, aCursor, aKeyRange)
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
    mCursor->Index()->GetActor()->SendPIndexedDBRequestConstructor(
                                                                        actor,
                                                                        params);
    actor->SetHelper(this);

    return NS_OK;
  }
};

class OpenObjectStoreCursorHelper : public OpenHelper
{
public:
  OpenObjectStoreCursorHelper(
                        WorkerPrivate* aWorkerPrivate, uint32_t aSyncQueueKey,
                        IDBCursorSync* aCursor, IDBKeyRange* aKeyRange)
  : OpenHelper(aWorkerPrivate, aSyncQueueKey, aCursor, aKeyRange)
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

class ContinueHelper : public BlockingHelperBase
{
public:
  ContinueHelper(WorkerPrivate* aWorkerPrivate, uint32_t aSyncQueueKey,
                 IDBCursorSync* aCursor, int32_t aCount)
  : BlockingHelperBase(aWorkerPrivate, aCursor), mSyncQueueKey(aSyncQueueKey),
    mCursor(aCursor), mCount(aCount)
  {
    MOZ_ASSERT(aCount > 0, "Must have a count!");
  }

  virtual nsresult
  HandleResponse(const ResponseValue& aResponseValue);

protected:
  nsresult
  IPCThreadRun()
  {
    MOZ_ASSERT(mPrimarySyncQueueKey == UINT32_MAX, "Should be unset!");
    mPrimarySyncQueueKey = mSyncQueueKey;

    CursorRequestParams params;

    ContinueParams continueParams;
    continueParams.key() = mCursor->mContinueToKey;
    continueParams.count() = uint32_t(mCount);

    params = continueParams;

    IndexedDBCursorRequestWorkerChild* actor =
      new IndexedDBCursorRequestWorkerChild(params.type());
    mCursor->GetActor()->SendPIndexedDBRequestConstructor(actor, params);
    actor->SetHelper(this);

    return NS_OK;
  }

private:
  uint32_t mSyncQueueKey;
  IDBCursorSync* mCursor;

  // In-params.
  int32_t mCount;
};

END_WORKERS_NAMESPACE

// For INDEXKEY cursors.
// static
IDBCursorSync*
IDBCursorSync::Create(JSContext* aCx, IDBIndexSync* aIndex,
                      Direction aDirection)
{
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);

  nsRefPtr<IDBCursorSync> cursor = new IDBCursorSync(aCx, workerPrivate);

  cursor->mIndex = aIndex;
  cursor->mTransaction = aIndex->ObjectStore()->Transaction();
  cursor->mType = INDEXKEY;
  cursor->mDirection = aDirection;

  if (!Wrap(aCx, nullptr, cursor)) {
    return nullptr;
  }

  return cursor;
}

// For OBJECTSTOREKEY cursors.
// static
IDBCursorSync*
IDBCursorSync::Create(JSContext* aCx, IDBObjectStoreSync* aObjectStore,
                      Direction aDirection)
{
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);

  nsRefPtr<IDBCursorSync> cursor = new IDBCursorSync(aCx, workerPrivate);

  cursor->mObjectStore = aObjectStore;
  cursor->mTransaction = aObjectStore->Transaction();
  cursor->mType = OBJECTSTOREKEY;
  cursor->mDirection = aDirection;

  if (!Wrap(aCx, nullptr, cursor)) {
    return nullptr;
  }

  return cursor;
}

inline JSObject*
MyWrap(JSContext* aCx, JSObject* aGlobal, nsRefPtr<IDBCursorSync>& aConcreteObject)
{
  MOZ_ASSERT(aCx);

  if (!aGlobal) {
    aGlobal = JS::CurrentGlobalOrNull(aCx);
    if (!aGlobal) {
      return nullptr;
    }
  }

  JS::Rooted<JSObject*> global(aCx, aGlobal);
  JSObject* proto = mozilla::dom::IDBCursorWithValueSyncBinding_workers::GetProtoObject(aCx, global);
  if (!proto) {
    return nullptr;
  }

  JSObject* wrapper =
    JS_NewObject(aCx, mozilla::dom::IDBCursorWithValueSyncBinding_workers::GetJSClass(), proto, global);
  if (!wrapper) {
    return nullptr;
  }

  js::SetReservedSlot(wrapper, DOM_OBJECT_SLOT,
                      PRIVATE_TO_JSVAL(aConcreteObject));

  aConcreteObject->SetIsDOMBinding();
  aConcreteObject->SetWrapper(wrapper);

  NS_ADDREF(aConcreteObject.get());
  return wrapper;
}

// For INDEXOBJECT cursors.
// static
IDBCursorSync*
IDBCursorSync::CreateWithValue(JSContext* aCx, IDBIndexSync* aIndex,
                               Direction aDirection)
{
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);

  nsRefPtr<IDBCursorSync> cursor =
    new IDBCursorSync(aCx, workerPrivate);

  cursor->mObjectStore = aIndex->ObjectStore();
  cursor->mIndex = aIndex;
  cursor->mTransaction = aIndex->ObjectStore()->Transaction();
  cursor->mType = INDEXOBJECT;
  cursor->mDirection = aDirection;

  if (!MyWrap(aCx, nullptr, cursor)) {
    return nullptr;
  }

  return cursor;
}

// For OBJECTSTORE cursors.
//static
IDBCursorSync*
IDBCursorSync::CreateWithValue(JSContext* aCx, IDBObjectStoreSync* aObjectStore,
                               Direction aDirection)
{
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);

  nsRefPtr<IDBCursorSync> cursor =
    new IDBCursorSync(aCx, workerPrivate);

  cursor->mObjectStore = aObjectStore;
  cursor->mTransaction = aObjectStore->Transaction();
  cursor->mType = OBJECTSTORE;
  cursor->mDirection = aDirection;

  if (!MyWrap(aCx, nullptr, cursor)) {
    return nullptr;
  }

  return cursor;
}

IDBCursorSync::IDBCursorSync(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
: IDBObjectSync(aCx, aWorkerPrivate), mObjectStore(nullptr), mIndex(nullptr),
  mHaveValue(true), mActorChild(nullptr)
{
}

IDBCursorSync::~IDBCursorSync()
{
  // TODO: Clear mCloneReadInfo
}

void
IDBCursorSync::_trace(JSTracer* aTrc)
{
  JS_CallHeapValueTracer(aTrc, &mCachedKey, "mCachedKey");
  JS_CallHeapValueTracer(aTrc, &mCachedPrimaryKey, "mCachedPrimaryKey");
  JS_CallHeapValueTracer(aTrc, &mCachedValue, "mCachedValue");

  IDBObjectSync::_trace(aTrc);
}

void
IDBCursorSync::_finalize(JSFreeOp* aFop)
{
  IDBObjectSync::_finalize(aFop);
}

void
IDBCursorSync::ReleaseIPCThreadObjects()
{
  AssertIsOnIPCThread();

  if (mActorChild) {
    mActorChild->Send__delete__(mActorChild);
    MOZ_ASSERT(!mActorChild, "Should have cleared in Send__delete__!");
  }
}

NS_IMPL_ISUPPORTS_INHERITED0(IDBCursorSync, IDBObjectSync)

JSObject*
IDBCursorSync::Source(JSContext* aCx)
{
  switch (mType) {
    case OBJECTSTORE:
    case OBJECTSTOREKEY:
      MOZ_ASSERT(mObjectStore);
      return mObjectStore->GetJSObject();

    case INDEXKEY:
    case INDEXOBJECT:
      MOZ_ASSERT(mIndex);
      return mIndex->GetJSObject();

    default:
      MOZ_ASSUME_UNREACHABLE("Bad type!");
  }
}

JS::Value
IDBCursorSync::Key(JSContext* aCx)
{
  MOZ_ASSERT(!mKey.IsUnset() || !mHaveValue, "Bad key!");

  if (!mHaveValue) {
    return JSVAL_VOID;
  }

  if (!mHaveCachedKey) {
    nsresult rv = mKey.ToJSVal(aCx, mCachedKey);
    NS_ENSURE_SUCCESS(rv, JSVAL_NULL);

    mHaveCachedKey = true;
  }

  return mCachedKey;
}

JS::Value
IDBCursorSync::PrimaryKey(JSContext* aCx)
{
  if (!mHaveValue) {
    return JSVAL_VOID;
  }

  if (!mHaveCachedPrimaryKey) {

    const indexedDB::Key& key =
      (mType == OBJECTSTORE || mType == OBJECTSTOREKEY) ? mKey : mObjectKey;
    MOZ_ASSERT(!key.IsUnset());

    nsresult rv = key.ToJSVal(aCx, mCachedPrimaryKey);
    NS_ENSURE_SUCCESS(rv, JSVAL_NULL);

    mHaveCachedPrimaryKey = true;
  }

  return mCachedPrimaryKey;
}

void
IDBCursorSync::Update(JSContext* aCx, JS::Value aValue, ErrorResult& aRv)
{
  if (mTransaction->IsInvalid()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return;
  }

  if (!mTransaction->IsWriteAllowed()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_READ_ONLY_ERR);
    return;
  }

  if (!mHaveValue || mType == OBJECTSTOREKEY || mType == INDEXKEY) {
    NS_WARNING("Dont have value or bad type!");
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return;
  }

  MOZ_ASSERT(mObjectStore);
  MOZ_ASSERT(!mKey.IsUnset());
  MOZ_ASSERT(mType == OBJECTSTORE || mType == INDEXOBJECT);
  MOZ_ASSERT_IF(mType == INDEXOBJECT, !mObjectKey.IsUnset());

  nsresult rv;
  const indexedDB::Key& objectKey = (mType == OBJECTSTORE) ? mKey : mObjectKey;

  if (mObjectStore->HasValidKeyPath()) {
    // Make sure the object given has the correct keyPath value set on it.
    const indexedDB::KeyPath& keyPath = mObjectStore->GetKeyPath();
    indexedDB::Key key;

    rv = keyPath.ExtractKey(aCx, aValue, key);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return;
    }

    if (key != objectKey) {
      NS_WARNING("key != objectKey");
      aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
      return;
    }

    Optional<JS::Handle<JS::Value> > emptyKey;
    mObjectStore->Put(aCx, aValue, emptyKey, aRv);

    return;
  }

  JS::Rooted<JS::Value> key(aCx);
  rv = objectKey.ToJSVal(aCx, &key);
  if (NS_FAILED(rv)) {
    NS_WARNING("ObjectKey.ToJSVal failed!");
    aRv.Throw(rv);
    return;
  }

  Optional<JS::Handle<JS::Value> > optionalKey;
  optionalKey.Construct(aCx, key);

  mObjectStore->Put(aCx, aValue, optionalKey, aRv);
}

bool
IDBCursorSync::Advance(JSContext* aCx, uint32_t aCount, ErrorResult& aRv)
{
  if (aCount < 1 || aCount > UINT32_MAX) {
    aRv.Throw(NS_ERROR_TYPE_ERR);
    return false;
  }

  indexedDB::Key key;
  return ContinueInternal(aCx, key, aCount, aRv);
}

bool
IDBCursorSync::Continue(JSContext* aCx,
                        const Optional<JS::Handle<JS::Value> >& aKey,
                        ErrorResult& aRv)
{
  indexedDB::Key key;
  if (aKey.WasPassed()) {
    nsresult rv = key.SetFromJSVal(aCx, aKey.Value());
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return false;
    }
  }

  if (!key.IsUnset()) {
    switch (mDirection) {
      case IDBCursorBase::NEXT:
      case IDBCursorBase::NEXT_UNIQUE:
        if (key <= mKey) {
          aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
          return false;
        }
        break;

      case IDBCursorBase::PREV:
      case IDBCursorBase::PREV_UNIQUE:
        if (key >= mKey) {
          aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
          return false;
        }
        break;

      default:
        MOZ_ASSUME_UNREACHABLE("Unknown direction type!");
    }
  }

  return ContinueInternal(aCx, key, 1, aRv);
}

bool
IDBCursorSync::Delete(JSContext* aCx, ErrorResult& aRv)
{
  if (mTransaction->IsInvalid()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return false;
  }

  if (!mTransaction->IsWriteAllowed()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_READ_ONLY_ERR);
    return false;
  }

  if (!mHaveValue || mType == OBJECTSTOREKEY || mType == INDEXKEY) {
    NS_WARNING("Dont have value or bad type!");
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return false;
  }

  MOZ_ASSERT(mObjectStore);
  MOZ_ASSERT(mType == OBJECTSTORE || mType == INDEXOBJECT);
  MOZ_ASSERT(!mKey.IsUnset());

  const indexedDB::Key& objectKey = (mType == OBJECTSTORE) ? mKey : mObjectKey;

  JS::Rooted<JS::Value> key(aCx);
  nsresult rv = objectKey.ToJSVal(aCx, &key);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return false;
  }

  mObjectStore->Delete(aCx, key, aRv);
  return !aRv.Failed();
}

JS::Value
IDBCursorSync::GetValue(JSContext* aCx, ErrorResult& aRv)
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
IDBCursorSync::Open(JSContext* aCx, IDBKeyRange* aKeyRange)
{
  DOMBindingAnchor<IDBCursorSync> selfAnchor(this);

  AutoSyncLoopHolder syncLoop(mWorkerPrivate);

  nsRefPtr<OpenHelper> helper;
  switch (mType) {
    case INDEXKEY:
      helper = new OpenIndexKeyCursorHelper(mWorkerPrivate,
                                            syncLoop.SyncQueueKey(), this,
                                            aKeyRange);
      break;
    case OBJECTSTOREKEY:
      helper = new OpenObjectStoreKeyCursorHelper(mWorkerPrivate,
                                                  syncLoop.SyncQueueKey(), this,
                                                  aKeyRange);
      break;
    case INDEXOBJECT:
      helper = new OpenIndexCursorHelper(mWorkerPrivate,
                                         syncLoop.SyncQueueKey(), this,
                                         aKeyRange);
      break;
    case OBJECTSTORE:
      helper = new OpenObjectStoreCursorHelper(mWorkerPrivate,
                                               syncLoop.SyncQueueKey(), this,
                                               aKeyRange);
      break;

    default:
      MOZ_ASSUME_UNREACHABLE("Unknown cursor type!");
  }

  if (!helper->Dispatch(aCx) || !syncLoop.RunAndForget(aCx)) {
    return false;
  }

  return true;
}

bool
IDBCursorSync::ContinueInternal(JSContext* aCx,
                                const mozilla::dom::indexedDB::Key& aKey,
                                int32_t aCount,
                                ErrorResult& aRv)
{
  MOZ_ASSERT(aCount > 0);

  if (mTransaction->IsInvalid()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return false;
  }

  if (!mHaveValue) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return false;
  }

  mContinueToKey = aKey;

  DOMBindingAnchor<IDBCursorSync> selfAnchor(this);

  AutoSyncLoopHolder syncLoop(mWorkerPrivate);

  nsRefPtr<ContinueHelper> helper =
    new ContinueHelper(mWorkerPrivate, syncLoop.SyncQueueKey(), this, aCount);

  if (!helper->Dispatch(aCx) || !syncLoop.RunAndForget(aCx)) {
    NS_WARNING("Failed to distpatch or RunAndForget!");
    return false;
  }

  return mHaveValue;
}

void
IDBCursorSync::SetCurrentKeysAndValue(
                             const mozilla::dom::indexedDB::Key& aKey,
                             const mozilla::dom::indexedDB::Key& aObjectKey,
                             const SerializedStructuredCloneReadInfo aCloneInfo)
{
  mCachedKey = JSVAL_VOID;
  mCachedPrimaryKey = JSVAL_VOID;
  mCachedValue = JSVAL_VOID;
  mHaveCachedKey = false;
  mHaveCachedPrimaryKey = false;
  mHaveCachedValue = false;

  if (aKey.IsUnset()) {
    mHaveValue = false;
  }
  else {
    mKey = aKey;
    mObjectKey = aObjectKey;
    mContinueToKey.Unset();

    if (!mCloneReadInfo.SetFromSerialized(aCloneInfo)) {
      NS_WARNING("Failed to copy clone buffer!");
    }
  }
}

nsresult
OpenHelper::HandleResponse(const ResponseValue& aResponseValue)
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
      indexedDB::Key emptyKey;
      indexedDB::SerializedStructuredCloneReadInfo cloneInfo;
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

nsresult
ContinueHelper::HandleResponse(const ResponseValue& aResponseValue)
{
  MOZ_ASSERT(aResponseValue.type() == ResponseValue::TContinueResponse,
             "Bad response type!");

  const ContinueResponse& response = aResponseValue.get_ContinueResponse();

  mCursor->SetCurrentKeysAndValue(response.key(), response.objectKey(),
                                  response.cloneInfo());

  return NS_OK;
}
