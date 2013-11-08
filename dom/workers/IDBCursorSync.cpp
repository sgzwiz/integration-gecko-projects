/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBCursorSync.h"

#include "mozilla/dom/IDBCursorSyncBinding.h"
#include "mozilla/dom/indexedDB/IDBKeyRange.h"
#include "mozilla/dom/indexedDB/IndexedDatabaseInlines.h"
#include "mozilla/dom/indexedDB/Key.h"
#include "mozilla/dom/UnionTypes.h"

#include "BlockingHelperBase.h"
#include "DatabaseInfoMT.h"
#include "IDBIndexSync.h"
#include "IDBObjectStoreSync.h"
#include "IndexedDBSyncProxies.h"
#include "IPCThreadUtils.h"
#include "WorkerPrivate.h"

#include "ipc/IndexedDBWorkerChild.h"

USING_WORKERS_NAMESPACE
using namespace mozilla::dom::indexedDB::ipc;
using mozilla::dom::indexedDB::IDBKeyRange;
using mozilla::dom::Optional;
using mozilla::dom::OwningIDBObjectStoreSyncOrIDBIndexSync;
using mozilla::ErrorResult;

BEGIN_WORKERS_NAMESPACE

class OpenHelper
{
public:
  OpenHelper(IDBCursorSync* aCursor, IDBKeyRange* aKeyRange)
  : mCursor(aCursor), mKeyRange(aKeyRange)
  { }

  virtual IDBObjectSync*
  Object() const
  {
    return mCursor;
  }

  virtual nsresult
  UnpackResponse(const ResponseValue& aResponseValue);

protected:
  nsRefPtr<IDBCursorSync> mCursor;

  // In-params.
  nsRefPtr<IDBKeyRange> mKeyRange;
};

class OpenObjectStoreKeyCursorHelper : public OpenHelper,
                                       public ObjectStoreHelper
{
public:
  OpenObjectStoreKeyCursorHelper(WorkerPrivate* aWorkerPrivate,
                                 IDBCursorSync* aCursor, IDBKeyRange* aKeyRange)
  : OpenHelper(aCursor, aKeyRange),
    ObjectStoreHelper(aWorkerPrivate, aCursor->ObjectStore())
  { }

  virtual IDBObjectSync*
  Object() const MOZ_OVERRIDE
  {
    return OpenHelper::Object();
  }

  virtual nsresult
  PackArguments(ObjectStoreRequestParams& aParams) MOZ_OVERRIDE;

  virtual nsresult
  UnpackResponse(const ResponseValue& aResponseValue) MOZ_OVERRIDE
  {
    return OpenHelper::UnpackResponse(aResponseValue);
  }
};

class OpenObjectStoreCursorHelper : public OpenObjectStoreKeyCursorHelper
{
public:
  OpenObjectStoreCursorHelper(WorkerPrivate* aWorkerPrivate,
                              IDBCursorSync* aCursor, IDBKeyRange* aKeyRange)
  : OpenObjectStoreKeyCursorHelper(aWorkerPrivate, aCursor, aKeyRange)
  { }

  virtual nsresult
  PackArguments(ObjectStoreRequestParams& aParams) MOZ_OVERRIDE;
};

class OpenIndexKeyCursorHelper : public OpenHelper,
                                 public IndexHelper
{
public:
  OpenIndexKeyCursorHelper(WorkerPrivate* aWorkerPrivate,
                           IDBCursorSync* aCursor, IDBKeyRange* aKeyRange)
  : OpenHelper(aCursor, aKeyRange),
    IndexHelper(aWorkerPrivate, aCursor->Index())
  { }

  virtual IDBObjectSync*
  Object() const MOZ_OVERRIDE
  {
    return OpenHelper::Object();
  }

  virtual nsresult
  PackArguments(IndexRequestParams& aParams) MOZ_OVERRIDE;

  virtual nsresult
  UnpackResponse(const ResponseValue& aResponseValue)
  {
    return OpenHelper::UnpackResponse(aResponseValue);
  }
};

class OpenIndexCursorHelper : public OpenIndexKeyCursorHelper
{
public:
  OpenIndexCursorHelper(WorkerPrivate* aWorkerPrivate, IDBCursorSync* aCursor,
                        IDBKeyRange* aKeyRange)
  : OpenIndexKeyCursorHelper(aWorkerPrivate, aCursor, aKeyRange)
  { }

  virtual nsresult
  PackArguments(IndexRequestParams& aParams) MOZ_OVERRIDE;
};

class ContinueHelper : public BlockingHelperBase
{
public:
  ContinueHelper(WorkerPrivate* aWorkerPrivate, IDBCursorSync* aCursor,
                 int32_t aCount)
  : BlockingHelperBase(aWorkerPrivate, aCursor), mCursor(aCursor),
    mCount(aCount)
  {
    MOZ_ASSERT(aCount > 0, "Must have a count!");
  }

  IDBCursorSync*
  Cursor()
  {
    return static_cast<IDBCursorSync*>(mObject.get());
  }

  virtual nsresult
  SendConstructor(IndexedDBRequestWorkerChildBase** aActor) MOZ_OVERRIDE;

  virtual nsresult
  UnpackResponse(const ResponseValue& aResponseValue) MOZ_OVERRIDE;

private:
  nsRefPtr<IDBCursorSync> mCursor;

  // In-params.
  int32_t mCount;
};

END_WORKERS_NAMESPACE

NS_IMPL_ADDREF_INHERITED(IDBCursorSync, IDBObjectSync)
NS_IMPL_RELEASE_INHERITED(IDBCursorSync, IDBObjectSync)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(IDBCursorSync)
NS_INTERFACE_MAP_END_INHERITING(IDBObjectSync)

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBCursorSync)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IDBCursorSync,
                                                IDBObjectSync)
  tmp->ReleaseProxy(ObjectIsGoingAway);
//  NS_IMPL_CYCLE_COLLECTION_UNLINK(mObjectStore)
//  NS_IMPL_CYCLE_COLLECTION_UNLINK(mIndex)
//  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTransaction)

  if (tmp->mHoldingJSVal) {
    tmp->mCachedKey = JSVAL_VOID;
    tmp->mCachedPrimaryKey = JSVAL_VOID;
    tmp->mCachedValue = JSVAL_VOID;
    tmp->mHaveCachedKey = false;
    tmp->mHaveCachedPrimaryKey = false;
    tmp->mHaveCachedValue = false;
    tmp->mHaveValue = false;
    mozilla::DropJSObjects(tmp);
    tmp->mHoldingJSVal = false;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IDBCursorSync,
                                                  IDBObjectSync)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mObjectStore)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIndex)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTransaction)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(IDBCursorSync,
                                               IDBObjectSync)
  MOZ_ASSERT(tmp->mHaveCachedKey || JSVAL_IS_VOID(tmp->mCachedKey),
             "Should have a cached key");
  MOZ_ASSERT(tmp->mHaveCachedPrimaryKey ||
             JSVAL_IS_VOID(tmp->mCachedPrimaryKey),
             "Should have a cached primary key");
  MOZ_ASSERT(tmp->mHaveCachedValue || JSVAL_IS_VOID(tmp->mCachedValue),
             "Should have a cached value");
  NS_IMPL_CYCLE_COLLECTION_TRACE_JSVAL_MEMBER_CALLBACK(mCachedKey)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JSVAL_MEMBER_CALLBACK(mCachedPrimaryKey)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JSVAL_MEMBER_CALLBACK(mCachedValue)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

// For INDEXKEY cursors.
// static
already_AddRefed<IDBCursorSync>
IDBCursorSync::Create(JSContext* aCx, IDBIndexSync* aIndex,
                      Direction aDirection)
{
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);

  nsRefPtr<IDBCursorSync> cursor = new IDBCursorSync(workerPrivate);

  cursor->mIndex = aIndex;
  cursor->mTransaction = aIndex->ObjectStore()->Transaction();
  cursor->mType = INDEXKEY;
  cursor->mDirection = aDirection;

  return cursor.forget();
}

// For OBJECTSTOREKEY cursors.
// static
already_AddRefed<IDBCursorSync>
IDBCursorSync::Create(JSContext* aCx, IDBObjectStoreSync* aObjectStore,
                      Direction aDirection)
{
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);

  nsRefPtr<IDBCursorSync> cursor = new IDBCursorSync(workerPrivate);

  cursor->mObjectStore = aObjectStore;
  cursor->mTransaction = aObjectStore->Transaction();
  cursor->mType = OBJECTSTOREKEY;
  cursor->mDirection = aDirection;

  return cursor.forget();
}

// For INDEXOBJECT cursors.
// static
already_AddRefed<IDBCursorSync>
IDBCursorSync::CreateWithValue(JSContext* aCx, IDBIndexSync* aIndex,
                               Direction aDirection)
{
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);

  nsRefPtr<IDBCursorSync> cursor = new IDBCursorSync(workerPrivate);

  cursor->mObjectStore = aIndex->ObjectStore();
  cursor->mIndex = aIndex;
  cursor->mTransaction = aIndex->ObjectStore()->Transaction();
  cursor->mType = INDEXOBJECT;
  cursor->mDirection = aDirection;

  return cursor.forget();
}

// For OBJECTSTORE cursors.
//static
already_AddRefed<IDBCursorSync>
IDBCursorSync::CreateWithValue(JSContext* aCx, IDBObjectStoreSync* aObjectStore,
                               Direction aDirection)
{
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);

  nsRefPtr<IDBCursorSync> cursor = new IDBCursorSync(workerPrivate);

  cursor->mObjectStore = aObjectStore;
  cursor->mTransaction = aObjectStore->Transaction();
  cursor->mType = OBJECTSTORE;
  cursor->mDirection = aDirection;

  return cursor.forget();
}

IDBCursorSync::IDBCursorSync(WorkerPrivate* aWorkerPrivate)
: IDBObjectSync(aWorkerPrivate), mHaveValue(true), mHoldingJSVal(false)
{
  SetIsDOMBinding();
}

IDBCursorSync::~IDBCursorSync()
{
  // TODO: Clear mCloneReadInfo
  mWorkerPrivate->AssertIsOnWorkerThread();

  ReleaseProxy(ObjectIsGoingAway);

  MOZ_ASSERT(!mRooted);

  if (mHoldingJSVal) {
    mCachedKey = JSVAL_VOID;
    mCachedPrimaryKey = JSVAL_VOID;
    mCachedValue = JSVAL_VOID;
    mozilla::DropJSObjects(this);
  }
}

IDBCursorSyncProxy*
IDBCursorSync::Proxy()
{
  return static_cast<IDBCursorSyncProxy*>(mProxy.get());
}

JSObject*
IDBCursorSync::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  switch (mType) {
    case OBJECTSTORE:
    case INDEXOBJECT:
      return IDBCursorWithValueSyncBinding_workers::Wrap(aCx, aScope, this);

    case OBJECTSTOREKEY:
    case INDEXKEY:
      return IDBCursorSyncBinding_workers::Wrap(aCx, aScope, this);

    default:
      MOZ_ASSUME_UNREACHABLE("Bad type!");
  }
}

void
IDBCursorSync::GetSource(OwningIDBObjectStoreSyncOrIDBIndexSync& aSource) const
{
  switch (mType) {
    case OBJECTSTORE:
    case OBJECTSTOREKEY:
      MOZ_ASSERT(mObjectStore);
      aSource.SetAsIDBObjectStoreSync() = mObjectStore;
      break;

    case INDEXKEY:
    case INDEXOBJECT:
      MOZ_ASSERT(mIndex);
      aSource.SetAsIDBIndexSync() = mIndex;

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
    if (!mHoldingJSVal) {
      mozilla::HoldJSObjects(this);
      mHoldingJSVal = true;
    }

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
    if (!mHoldingJSVal) {
      mozilla::HoldJSObjects(this);
      mHoldingJSVal = true;
    }

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
    if (!mHoldingJSVal) {
      mozilla::HoldJSObjects(this);
      mHoldingJSVal = true;
    }

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
IDBCursorSync::Open2(JSContext* aCx, IDBKeyRange* aKeyRange)
{
//  mProxy = new IDBCursorSyncProxy(this);

  nsRefPtr<BlockingHelperBase> helper;
  switch (mType) {
    case OBJECTSTOREKEY:
      helper = new OpenObjectStoreKeyCursorHelper(mWorkerPrivate, this,
                                                  aKeyRange);
      break;
    case OBJECTSTORE:
      helper = new OpenObjectStoreCursorHelper(mWorkerPrivate, this, aKeyRange);
      break;
    case INDEXKEY:
      helper = new OpenIndexKeyCursorHelper(mWorkerPrivate, this, aKeyRange);
      break;
    case INDEXOBJECT:
      helper = new OpenIndexCursorHelper(mWorkerPrivate, this, aKeyRange);
      break;

    default:
      MOZ_ASSUME_UNREACHABLE("Unknown cursor type!");
  }

  if (!helper->Run(aCx)) {
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

  nsRefPtr<ContinueHelper> helper =
    new ContinueHelper(mWorkerPrivate, this, aCount);

  if (!helper->Run(aCx)) {
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
OpenHelper::UnpackResponse(const ResponseValue& aResponseValue)
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
OpenObjectStoreKeyCursorHelper::PackArguments(ObjectStoreRequestParams& aParams)
{
  AssertIsOnIPCThread();

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

  aParams = openKeyCursorParams;

  return NS_OK;
}

nsresult
OpenObjectStoreCursorHelper::PackArguments(ObjectStoreRequestParams& aParams)
{
  AssertIsOnIPCThread();

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

  aParams = openCursorParams;

  return NS_OK;
}

nsresult
OpenIndexKeyCursorHelper::PackArguments(IndexRequestParams& aParams)
{
  AssertIsOnIPCThread();

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

  aParams = openKeyCursorParams;

  return NS_OK;
}

nsresult
OpenIndexCursorHelper::PackArguments(IndexRequestParams& aParams)
{
  AssertIsOnIPCThread();

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

  aParams = openCursorParams;

  return NS_OK;
}

nsresult
ContinueHelper::SendConstructor(IndexedDBRequestWorkerChildBase** aActor)
{
  CursorRequestParams params;

  ContinueParams continueParams;
  continueParams.key() = mCursor->mContinueToKey;
  continueParams.count() = uint32_t(mCount);

  params = continueParams;

  IndexedDBCursorRequestWorkerChild* actor =
    new IndexedDBCursorRequestWorkerChild(params.type());
  Cursor()->Proxy()->Actor()->SendPIndexedDBRequestConstructor(actor, params);

  *aActor = actor;
  return NS_OK;
}

nsresult
ContinueHelper::UnpackResponse(const ResponseValue& aResponseValue)
{
  MOZ_ASSERT(aResponseValue.type() == ResponseValue::TContinueResponse,
             "Bad response type!");

  const ContinueResponse& response = aResponseValue.get_ContinueResponse();

  mCursor->SetCurrentKeysAndValue(response.key(), response.objectKey(),
                                  response.cloneInfo());

  return NS_OK;
}
