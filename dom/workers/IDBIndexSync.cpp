/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBIndexSync.h"

#include "mozilla/dom/IDBIndexSyncBinding.h"
#include "mozilla/dom/indexedDB/IDBKeyRange.h"
#include "mozilla/dom/indexedDB/IndexedDatabase.h"
#include "mozilla/dom/indexedDB/IndexedDatabaseInlines.h"
#include "mozilla/dom/indexedDB/KeyPath.h"

#include "IDBCursorSync.h"
#include "IDBObjectStoreSync.h"
#include "IPCThreadUtils.h"
#include "WorkerPrivate.h"

#include "ipc/IndexedDBWorkerChild.h"

USING_WORKERS_NAMESPACE
using namespace mozilla::dom::indexedDB;
using namespace mozilla::dom::indexedDB::ipc;
using mozilla::dom::IDBCursorDirection;
using mozilla::dom::indexedDB::IDBKeyRange;
using mozilla::dom::Optional;
using mozilla::dom::PBlobChild;
using mozilla::ErrorResult;

namespace {

class InitRunnable : public BlockWorkerThreadRunnable
{
public:
  InitRunnable(WorkerPrivate* aWorkerPrivate, IDBIndexSync* aIndex,
               IndexInfo* aIndexInfo, bool aCreating)
  : BlockWorkerThreadRunnable(aWorkerPrivate), mIndex(aIndex),
    mIndexInfo(aIndexInfo) ,mCreating(aCreating)
  { }

protected:
  nsresult
  IPCThreadRun()
  {
    IndexConstructorParams params;

    if (mCreating) {
      CreateIndexParams createParams;
      createParams.info() = *mIndexInfo;
      params = createParams;
    }
    else {
      GetIndexParams getParams;
      getParams.name() = mIndexInfo->name;
      params = getParams;
    }

    IndexedDBIndexWorkerChild* actor =
      new IndexedDBIndexWorkerChild();
    mIndex->ObjectStore()->Proxy()->Actor()->SendPIndexedDBIndexConstructor(
                                                                        actor,
                                                                        params);
    actor->SetIndexProxy(mIndex->Proxy());

    return NS_OK;
  }

  void virtual
  PostRun() MOZ_OVERRIDE
  {
    mIndex = nullptr;
  }

private:
  IDBIndexSync* mIndex;
  IndexInfo* mIndexInfo;
  bool mCreating;
};

class GetHelper : public IndexHelper
{
public:
  GetHelper(WorkerPrivate* aWorkerPrivate, IDBIndexSync* aIndex,
            IDBKeyRange* aKeyRange)
  : IndexHelper(aWorkerPrivate, aIndex), mKeyRange(aKeyRange)
  { }

  virtual nsresult
  PackArguments(IndexRequestParams& aParams) MOZ_OVERRIDE;

  virtual nsresult
  UnpackResponse(const ResponseValue& aResponseValue) MOZ_OVERRIDE;

  bool
  GetResult(JSContext* aCx, JS::MutableHandle<JS::Value> aValue);

private:
  // In-params.
  nsRefPtr<IDBKeyRange> mKeyRange;

  // Out-params.
  JSAutoStructuredCloneBuffer mCloneBuffer;
};

class GetKeyHelper : public IndexHelper
{
public:
  GetKeyHelper(WorkerPrivate* aWorkerPrivate, IDBIndexSync* aIndex,
               IDBKeyRange* aKeyRange)
  : IndexHelper(aWorkerPrivate, aIndex), mKeyRange(aKeyRange)
  { }

  virtual nsresult
  PackArguments(IndexRequestParams& aParams) MOZ_OVERRIDE;

  virtual nsresult
  UnpackResponse(const ResponseValue& aResponseValue) MOZ_OVERRIDE;

  const Key&
  GetResult() const
  {
    return mKey;
  }

private:
  // In-params.
  nsRefPtr<IDBKeyRange> mKeyRange;

  // Out-params.
  Key mKey;
};

class GetAllHelper : public IndexHelper
{
  typedef mozilla::dom::indexedDB::StructuredCloneReadInfo
                                                        StructuredCloneReadInfo;

public:
  GetAllHelper(WorkerPrivate* aWorkerPrivate, IDBIndexSync* aIndex,
               IDBKeyRange* aKeyRange, const uint32_t aLimit)
  : IndexHelper(aWorkerPrivate, aIndex), mKeyRange(aKeyRange), mLimit(aLimit)
  { }

  virtual nsresult
  PackArguments(IndexRequestParams& aParams) MOZ_OVERRIDE;

  virtual nsresult
  UnpackResponse(const ResponseValue& aResponseValue) MOZ_OVERRIDE;

  bool
  GetResult(JSContext* aCx, JS::MutableHandle<JS::Value> aValue);

private:
  // In-params.
  nsRefPtr<IDBKeyRange> mKeyRange;
  const uint32_t mLimit;

  // Out-params.
  nsTArray<StructuredCloneReadInfo> mCloneReadInfos;
};

class GetAllKeysHelper : public IndexHelper
{
public:
  GetAllKeysHelper(WorkerPrivate* aWorkerPrivate, IDBIndexSync* aIndex,
                   IDBKeyRange* aKeyRange, const uint32_t aLimit)
  : IndexHelper(aWorkerPrivate, aIndex), mKeyRange(aKeyRange), mLimit(aLimit)
  { }

  virtual nsresult
  PackArguments(IndexRequestParams& aParams) MOZ_OVERRIDE;

  virtual nsresult
  UnpackResponse(const ResponseValue& aResponseValue) MOZ_OVERRIDE;

  bool
  GetResult(JSContext* aCx, JS::MutableHandle<JS::Value> aValue);

private:
  // In-params.
  nsRefPtr<IDBKeyRange> mKeyRange;
  const uint32_t mLimit;

  // Out-params.
  nsTArray<Key> mKeys;
};

class CountHelper : public IndexHelper
{
public:
  CountHelper(WorkerPrivate* aWorkerPrivate, IDBIndexSync* aIndex,
              IDBKeyRange* aKeyRange)
  : IndexHelper(aWorkerPrivate, aIndex), mKeyRange(aKeyRange)
  { }

  virtual nsresult
  PackArguments(IndexRequestParams& aParams) MOZ_OVERRIDE;

  virtual nsresult
  UnpackResponse(const ResponseValue& aResponseValue) MOZ_OVERRIDE;

  uint64_t
  GetResult() const
  {
    return mCount;
  }

private:
  // In-params.
  nsRefPtr<IDBKeyRange> mKeyRange;

  // Out-params.
  uint64_t mCount;
};

} // anonymous namespace

IDBIndexSyncProxy::IDBIndexSyncProxy(IDBIndexSync* aIndex)
: IDBObjectSyncProxy<IndexedDBIndexWorkerChild>(aIndex)
{
}

IDBIndexSync*
IDBIndexSyncProxy::Index()
{
  return static_cast<IDBIndexSync*>(mObject);
}

NS_IMPL_ADDREF_INHERITED(IDBIndexSync, IDBObjectSync)
NS_IMPL_RELEASE_INHERITED(IDBIndexSync, IDBObjectSync)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(IDBIndexSync)
NS_INTERFACE_MAP_END_INHERITING(IDBObjectSync)

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBIndexSync)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IDBIndexSync,
                                                IDBObjectSync)
  tmp->ReleaseProxy(ObjectIsGoingAway);
//  NS_IMPL_CYCLE_COLLECTION_UNLINK(mObjectStore)

  tmp->mCachedKeyPath = JSVAL_VOID;

  if (tmp->mHoldingJSVal) {
    mozilla::DropJSObjects(tmp);
    tmp->mHoldingJSVal = false;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IDBIndexSync,
                                                  IDBObjectSync)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mObjectStore)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(IDBIndexSync,
                                               IDBObjectSync)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JSVAL_MEMBER_CALLBACK(mCachedKeyPath)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

// static
already_AddRefed<IDBIndexSync>
IDBIndexSync::Create(JSContext* aCx, IDBObjectStoreSync* aObjectStore,
                     IndexInfo* aIndexInfo)
{
  MOZ_ASSERT(aIndexInfo, "Null pointer!");

  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);

  nsRefPtr<IDBIndexSync> index = new IDBIndexSync(workerPrivate);

  index->mObjectStore = aObjectStore;
  index->mId = aIndexInfo->id;
  index->mName = aIndexInfo->name;
  index->mKeyPath = aIndexInfo->keyPath;
  index->mUnique = aIndexInfo->unique;
  index->mMultiEntry = aIndexInfo->multiEntry;

  return index.forget();
}

IDBIndexSync::IDBIndexSync(WorkerPrivate* aWorkerPrivate)
: IDBObjectSync(aWorkerPrivate), mHoldingJSVal(false)
{
  SetIsDOMBinding();
}

IDBIndexSync::~IDBIndexSync()
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  ReleaseProxy(ObjectIsGoingAway);

  MOZ_ASSERT(!mRooted);

  if (mHoldingJSVal) {
    mCachedKeyPath = JSVAL_VOID;
    mozilla::DropJSObjects(this);
  }
}

IDBIndexSyncProxy*
IDBIndexSync::Proxy()
{
  return static_cast<IDBIndexSyncProxy*>(mProxy.get());
}

JSObject*
IDBIndexSync::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return IDBIndexSyncBinding_workers::Wrap(aCx, aScope, this);
}

void
IDBIndexSync::GetStoreName(nsString& aStoreName)
{
  mObjectStore->GetName(aStoreName);
}

JS::Value
IDBIndexSync::GetKeyPath(JSContext* aCx, ErrorResult& aRv)
{
  if (!JSVAL_IS_VOID(mCachedKeyPath)) {
    return mCachedKeyPath;
  }

  nsresult rv = mKeyPath.ToJSVal(aCx, mCachedKeyPath);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return JSVAL_NULL;
  }

  if (JSVAL_IS_GCTHING(mCachedKeyPath)) {
    mozilla::HoldJSObjects(this);
    mHoldingJSVal = true;
  }

  return mCachedKeyPath;
}

already_AddRefed<IDBCursorSync>
IDBIndexSync::OpenCursor(JSContext* aCx,
                         const Optional<JS::Handle<JS::Value> >& aRange,
                         IDBCursorDirection aDirection, ErrorResult& aRv)
{
  if (mObjectStore->Transaction()->IsInvalid()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return nullptr;
  }

  nsRefPtr<IDBKeyRange> keyRange;
  if (aRange.WasPassed()) {
    if (NS_FAILED(IDBKeyRange::FromJSVal(aCx, aRange.Value(),
                                         getter_AddRefs(keyRange)))) {
      aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
      return nullptr;
    }
  }

  IDBCursorSync::Direction direction =
    IDBCursorSync::ConvertDirection(aDirection);

  nsRefPtr<IDBCursorSync> cursor =
    IDBCursorSync::CreateWithValue(aCx, this, direction);
  if (!cursor) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  cursor->mProxy = new IDBCursorSyncProxy(cursor);

  if (!cursor->Open2(aCx, keyRange)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return cursor->mHaveValue ? cursor.forget() : nullptr;
}

already_AddRefed<IDBCursorSync>
IDBIndexSync::OpenKeyCursor(JSContext* aCx,
                            const Optional<JS::Handle<JS::Value> >& aRange,
                            IDBCursorDirection aDirection, ErrorResult& aRv)
{
  if (mObjectStore->Transaction()->IsInvalid()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return nullptr;
  }

  nsRefPtr<IDBKeyRange> keyRange;
  if (aRange.WasPassed()) {
    if (NS_FAILED(IDBKeyRange::FromJSVal(aCx, aRange.Value(),
                                         getter_AddRefs(keyRange)))) {
      aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
      return nullptr;
    }
  }

  IDBCursorSync::Direction direction =
    IDBCursorSync::ConvertDirection(aDirection);

  nsRefPtr<IDBCursorSync> cursor = IDBCursorSync::Create(aCx, this, direction);
  if (!cursor) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  cursor->mProxy = new IDBCursorSyncProxy(cursor);

  if (!cursor->Open2(aCx, keyRange)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return cursor->mHaveValue ? cursor.forget() : nullptr;
}

JS::Value
IDBIndexSync::Get(JSContext* aCx, JS::Value aKey, ErrorResult& aRv)
{
  if (mObjectStore->Transaction()->IsInvalid()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return JSVAL_NULL;
  }

  nsRefPtr<IDBKeyRange> keyRange;
  nsresult rv = IDBKeyRange::FromJSVal(aCx, aKey, getter_AddRefs(keyRange));
  NS_ENSURE_SUCCESS(rv, JSVAL_NULL);

  if (!keyRange) {
    // Must specify a key or keyRange for get().
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
    return JSVAL_NULL;
  }

  nsRefPtr<GetHelper> helper =
    new GetHelper(mWorkerPrivate, this, keyRange);

  if (!helper->Run(aCx)) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return JSVAL_NULL;
  }

  JS::Rooted<JS::Value> value(aCx);
  if (!helper->GetResult(aCx, &value)) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return JSVAL_NULL;
  }

  return value;
}

JS::Value
IDBIndexSync::GetKey(JSContext* aCx, JS::Value aKey, ErrorResult& aRv)
{
  if (mObjectStore->Transaction()->IsInvalid()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return JSVAL_NULL;
  }

  nsRefPtr<IDBKeyRange> keyRange;
  nsresult rv = IDBKeyRange::FromJSVal(aCx, aKey, getter_AddRefs(keyRange));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return JSVAL_NULL;
  }

  if (!keyRange) {
    // Must specify a key or keyRange for get().
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
    return JSVAL_NULL;
  }

  nsRefPtr<GetKeyHelper> helper =
    new GetKeyHelper(mWorkerPrivate, this, keyRange);

  if (!helper->Run(aCx)) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return JSVAL_NULL;
  }

  JS::Rooted<JS::Value> newKey(aCx);
  rv = helper->GetResult().ToJSVal(aCx, &newKey);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return JSVAL_NULL;
  }

  return newKey;
}

JS::Value
IDBIndexSync::GetAll(JSContext* aCx,
                     const Optional<JS::Handle<JS::Value> >& aKey,
                     const Optional<uint32_t>& aLimit, ErrorResult& aRv)
{
  if (mObjectStore->Transaction()->IsInvalid()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return JSVAL_NULL;
  }

  nsRefPtr<IDBKeyRange> keyRange;
  if (aKey.WasPassed()) {
    if (NS_FAILED(IDBKeyRange::FromJSVal(aCx, aKey.Value(),
                                         getter_AddRefs(keyRange)))) {
      NS_WARNING("KeyRange parsing failed!");
      aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
      return JSVAL_NULL;
    }
  }

  uint32_t limit = UINT32_MAX;
  if (aLimit.WasPassed() && aLimit.Value() != 0) {
    limit = aLimit.Value();
  }

  nsRefPtr<GetAllHelper> helper =
    new GetAllHelper(mWorkerPrivate, this, keyRange, limit);

  if (!helper->Run(aCx)) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return JSVAL_NULL;
  }

  JS::Rooted<JS::Value> value(aCx);
  if (!helper->GetResult(aCx, &value)) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return JSVAL_NULL;
  }

  return value;
}

JS::Value
IDBIndexSync::GetAllKeys(JSContext* aCx,
                         const Optional<JS::Handle<JS::Value> >& aKey,
                         const Optional<uint32_t>& aLimit, ErrorResult& aRv)
{
  if (mObjectStore->Transaction()->IsInvalid()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return JSVAL_NULL;
  }

  nsRefPtr<IDBKeyRange> keyRange;
  if (aKey.WasPassed()) {
    if (NS_FAILED(IDBKeyRange::FromJSVal(aCx, aKey.Value(),
                                         getter_AddRefs(keyRange)))) {
      NS_WARNING("KeyRange parsing failed!");
      aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
      return JSVAL_NULL;
    }
  }

  uint32_t limit = UINT32_MAX;
  if (aLimit.WasPassed() && aLimit.Value() != 0) {
    limit = aLimit.Value();
  }

  nsRefPtr<GetAllKeysHelper> helper =
    new GetAllKeysHelper(mWorkerPrivate, this, keyRange, limit);

  if (!helper->Run(aCx)) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return JSVAL_NULL;
  }

  JS::Rooted<JS::Value> value(aCx);
  if (!helper->GetResult(aCx, &value)) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return JSVAL_NULL;
  }

  return value;
}

uint64_t
IDBIndexSync::Count(JSContext* aCx,
                    const Optional<JS::Handle<JS::Value> >& aValue,
                    ErrorResult& aRv)
{
  if (mObjectStore->Transaction()->IsInvalid()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return 0;
  }

  nsRefPtr<IDBKeyRange> keyRange;
  if (aValue.WasPassed()) {
    if (NS_FAILED(IDBKeyRange::FromJSVal(aCx, aValue.Value(),
                                         getter_AddRefs(keyRange)))) {
      aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
      return 0;
    }
  }

  nsRefPtr<CountHelper> helper =
    new CountHelper(mWorkerPrivate, this, keyRange);

  if (!helper->Run(aCx)) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return 0;
  }

  return helper->GetResult();
}

bool
IDBIndexSync::Init(JSContext* aCx, IndexInfo* aIndexInfo, bool aCreating)
{
  mProxy = new IDBIndexSyncProxy(this);

  nsRefPtr<IDBIndexSync> kungFuDeathGrip = this;

  nsRefPtr<InitRunnable> runnable = new InitRunnable(mWorkerPrivate, this,
                                                     aIndexInfo, aCreating);

  if (!runnable->Dispatch(aCx)) {
    ReleaseProxy();
    return false;
  }

  return true;
}

nsresult
IndexHelper::SendConstructor(IndexedDBRequestWorkerChildBase** aActor)
{
  IndexRequestParams params;
  PackArguments(params);

  IndexedDBIndexRequestWorkerChild* actor =
    new IndexedDBIndexRequestWorkerChild(params.type());
  Index()->Proxy()->Actor()->SendPIndexedDBRequestConstructor(actor, params);

  *aActor = actor;
  return NS_OK;
}

nsresult
GetHelper::PackArguments(IndexRequestParams& aParams)
{
  GetParams getParams;

  mKeyRange->ToSerializedKeyRange(getParams.keyRange());

  aParams = getParams;

  return NS_OK;
}

nsresult
GetHelper::UnpackResponse(const ResponseValue& aResponseValue)
{
  AssertIsOnIPCThread();

  MOZ_ASSERT(aResponseValue.type() == ResponseValue::TGetResponse,
             "Bad response type!");

  const GetResponse& getResponse = aResponseValue.get_GetResponse();
  const SerializedStructuredCloneReadInfo& cloneInfo = getResponse.cloneInfo();

  mCloneBuffer.copy(cloneInfo.data, cloneInfo.dataLength);

  return NS_OK;
}

bool
GetHelper::GetResult(JSContext* aCx, JS::MutableHandle<JS::Value> aValue)
{
  bool result = IDBObjectStoreSync::DeserializeValue(aCx, mCloneBuffer, aValue);
  mCloneBuffer.clear();

  return result;
}

nsresult
GetKeyHelper::PackArguments(IndexRequestParams& aParams)
{
  AssertIsOnIPCThread();

  GetKeyParams getKeyParams;

  mKeyRange->ToSerializedKeyRange(getKeyParams.keyRange());

  aParams = getKeyParams;

  return NS_OK;
}

nsresult
GetKeyHelper::UnpackResponse(const ResponseValue& aResponseValue)
{
  AssertIsOnIPCThread();

  MOZ_ASSERT(aResponseValue.type() == ResponseValue::TGetKeyResponse,
             "Bad response type!");

  mKey = aResponseValue.get_GetKeyResponse().key();

  return NS_OK;
}

nsresult
GetAllHelper::PackArguments(IndexRequestParams& aParams)
{
  AssertIsOnIPCThread();

  GetAllParams getAllparams;

  if (mKeyRange) {
    KeyRange keyRange;
    mKeyRange->ToSerializedKeyRange(keyRange);
    getAllparams.optionalKeyRange() = keyRange;
  }
  else {
    getAllparams.optionalKeyRange() = mozilla::void_t();
  }

  getAllparams.limit() = mLimit;

  aParams = getAllparams;

  return NS_OK;
}

nsresult
GetAllHelper::UnpackResponse(const ResponseValue& aResponseValue)
{
  AssertIsOnIPCThread();

  MOZ_ASSERT(aResponseValue.type() == ResponseValue::TGetAllResponse,
             "Bad response type!");

  const GetAllResponse& getAllResponse = aResponseValue.get_GetAllResponse();

  const nsTArray<SerializedStructuredCloneReadInfo>& cloneInfos =
    getAllResponse.cloneInfos();

  mCloneReadInfos.SetCapacity(cloneInfos.Length());

  for (uint32_t index = 0; index < cloneInfos.Length(); index++) {
    const SerializedStructuredCloneReadInfo srcInfo = cloneInfos[index];

    StructuredCloneReadInfo* destInfo = mCloneReadInfos.AppendElement();
    if (!destInfo->SetFromSerialized(srcInfo)) {
      NS_WARNING("Failed to copy clone buffer!");
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }
  }

  return NS_OK;
}

bool
GetAllHelper::GetResult(JSContext* aCx, JS::MutableHandle<JS::Value> aValue)
{
  MOZ_ASSERT(mCloneReadInfos.Length() <= mLimit, "Too many results!");

  nsresult rv = ConvertToArrayAndCleanup(aCx, mCloneReadInfos, aValue);

  MOZ_ASSERT(mCloneReadInfos.IsEmpty(),
             "Should have cleared in ConvertToArrayAndCleanup");
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

nsresult
GetAllKeysHelper::PackArguments(IndexRequestParams& aParams)
{
  GetAllKeysParams getAllKeyParams;

  if (mKeyRange) {
    KeyRange keyRange;
    mKeyRange->ToSerializedKeyRange(keyRange);
    getAllKeyParams.optionalKeyRange() = keyRange;
  }
  else {
    getAllKeyParams.optionalKeyRange() = mozilla::void_t();
  }

  getAllKeyParams.limit() = mLimit;

  aParams = getAllKeyParams;

  return NS_OK;
}

nsresult
GetAllKeysHelper::UnpackResponse(const ResponseValue& aResponseValue)
{
  AssertIsOnIPCThread();

  MOZ_ASSERT(aResponseValue.type() == ResponseValue::TGetAllKeysResponse,
             "Bad response type!");

  mKeys.AppendElements(aResponseValue.get_GetAllKeysResponse().keys());

  return NS_OK;
}

bool
GetAllKeysHelper::GetResult(JSContext* aCx, JS::MutableHandle<JS::Value> aValue)
{
  MOZ_ASSERT(mKeys.Length() <= mLimit, "Too many results!");

  nsTArray<Key> keys;
  mKeys.SwapElements(keys);

  JS::Rooted<JSObject*> array(aCx, JS_NewArrayObject(aCx, 0, NULL));
  if (!array) {
    NS_WARNING("Failed to make array!");
    return false;
  }

  if (!keys.IsEmpty()) {
    if (!JS_SetArrayLength(aCx, array, uint32_t(keys.Length()))) {
      NS_WARNING("Failed to set array length!");
      return false;
    }

    for (uint32_t index = 0, count = keys.Length(); index < count; index++) {
      const Key& key = keys[index];
      MOZ_ASSERT(!key.IsUnset(), "Bad key!");

      JS::Rooted<JS::Value> value(aCx);
      nsresult rv = key.ToJSVal(aCx, &value);
      if (NS_FAILED(rv)) {
        NS_WARNING("Failed to get jsval for key!");
        return false;
      }

      if (!JS_SetElement(aCx, array, index, &value)) {
        NS_WARNING("Failed to set array element!");
        return false;
      }
    }
  }

  aValue.setObject(*array);
  return true;
}

nsresult
CountHelper::PackArguments(IndexRequestParams& aParams)
{
  CountParams countParams;

  if(mKeyRange) {
    KeyRange keyRange;
    mKeyRange->ToSerializedKeyRange(keyRange);
    countParams.optionalKeyRange() = keyRange;
  }
  else {
    countParams.optionalKeyRange() = mozilla::void_t();
  }

  aParams = countParams;

  return NS_OK;
}

nsresult
CountHelper::UnpackResponse(const ResponseValue& aResponseValue)
{
  AssertIsOnIPCThread();

  MOZ_ASSERT(aResponseValue.type() == ResponseValue::TCountResponse,
             "Bad response type!");

  mCount = aResponseValue.get_CountResponse().count();

  return NS_OK;
}
