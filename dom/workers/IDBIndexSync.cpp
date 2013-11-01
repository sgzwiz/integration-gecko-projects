/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBIndexSync.h"

#include "mozilla/dom/indexedDB/IDBKeyRange.h"
#include "mozilla/dom/indexedDB/IndexedDatabase.h"
#include "mozilla/dom/indexedDB/IndexedDatabaseInlines.h"
#include "mozilla/dom/indexedDB/KeyPath.h"

#include "BlockingHelperBase.h"
#include "DOMBindingInlines.h"
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
    mIndex->ObjectStore()->GetActor()->SendPIndexedDBIndexConstructor(actor,
                                                                      params);
    actor->SetIndex(mIndex);

    return NS_OK;
  }

private:
  IDBIndexSync* mIndex;
  IndexInfo* mIndexInfo;
  bool mCreating;
};

class GetHelper : public BlockingHelperBase
{
public:
  GetHelper(WorkerPrivate* aWorkerPrivate, uint32_t aSyncQueueKey,
            IDBIndexSync* aIndex, IDBKeyRange* aKeyRange)
  : BlockingHelperBase(aWorkerPrivate, aIndex), mSyncQueueKey(aSyncQueueKey),
    mIndex(aIndex), mKeyRange(aKeyRange)
  { }

  virtual nsresult
  HandleResponse(const ResponseValue& aResponseValue);

  bool
  Read(JSContext* aCx, JS::MutableHandle<JS::Value> aValue);

protected:
  nsresult
  IPCThreadRun()
  {
    MOZ_ASSERT(mPrimarySyncQueueKey == UINT32_MAX, "Should be unset!");
    mPrimarySyncQueueKey = mSyncQueueKey;

    IndexRequestParams params;

    GetParams getParams;

    mKeyRange->ToSerializedKeyRange(getParams.keyRange());

    params = getParams;

    IndexedDBIndexRequestWorkerChild* actor =
      new IndexedDBIndexRequestWorkerChild(params.type());
    mIndex->GetActor()->SendPIndexedDBRequestConstructor(actor, params);
    actor->SetHelper(this);

    return NS_OK;
  }

private:
  uint32_t mSyncQueueKey;
  IDBIndexSync* mIndex;

  // In-params.
  nsRefPtr<IDBKeyRange> mKeyRange;

  // Out-params.
  JSAutoStructuredCloneBuffer mCloneBuffer;
};

class GetKeyHelper : public BlockingHelperBase
{
public:
  GetKeyHelper(WorkerPrivate* aWorkerPrivate, uint32_t aSyncQueueKey,
            IDBIndexSync* aIndex, IDBKeyRange* aKeyRange)
  : BlockingHelperBase(aWorkerPrivate, aIndex), mSyncQueueKey(aSyncQueueKey),
    mIndex(aIndex), mKeyRange(aKeyRange)
  { }

  virtual nsresult
  HandleResponse(const ResponseValue& aResponseValue);

  const Key&
  GetKey() const
  {
    return mKey;
  }

protected:
  nsresult
  IPCThreadRun()
  {
    MOZ_ASSERT(mPrimarySyncQueueKey == UINT32_MAX, "Should be unset!");
    mPrimarySyncQueueKey = mSyncQueueKey;

    IndexRequestParams params;

    GetKeyParams getKeyParams;

    mKeyRange->ToSerializedKeyRange(getKeyParams.keyRange());

    params = getKeyParams;

    IndexedDBIndexRequestWorkerChild* actor =
      new IndexedDBIndexRequestWorkerChild(params.type());
    mIndex->GetActor()->SendPIndexedDBRequestConstructor(actor, params);
    actor->SetHelper(this);

    return NS_OK;
  }

private:
  uint32_t mSyncQueueKey;
  IDBIndexSync* mIndex;

  // In-params.
  nsRefPtr<IDBKeyRange> mKeyRange;

  // Out-params.
  Key mKey;
};

class GetAllHelper : public BlockingHelperBase
{
  typedef mozilla::dom::indexedDB::StructuredCloneReadInfo
                                                        StructuredCloneReadInfo;

public:
  GetAllHelper(WorkerPrivate* aWorkerPrivate, uint32_t aSyncQueueKey,
               IDBIndexSync* aIndex, IDBKeyRange* aKeyRange,
               const uint32_t aLimit)
  : BlockingHelperBase(aWorkerPrivate, aIndex), mSyncQueueKey(aSyncQueueKey),
    mIndex(aIndex), mKeyRange(aKeyRange), mLimit(aLimit)
  { }

  virtual nsresult
  HandleResponse(const ResponseValue& aResponseValue);

  bool
  Read(JSContext* aCx, JS::MutableHandle<JS::Value> aValue);

protected:
  nsresult
  IPCThreadRun()
  {
    MOZ_ASSERT(mPrimarySyncQueueKey == UINT32_MAX, "Should be unset!");
    mPrimarySyncQueueKey = mSyncQueueKey;

    IndexRequestParams params;

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

    params = getAllparams;

    IndexedDBIndexRequestWorkerChild* actor =
      new IndexedDBIndexRequestWorkerChild(params.type());
    mIndex->GetActor()->SendPIndexedDBRequestConstructor(actor, params);
    actor->SetHelper(this);

    return NS_OK;
  }

private:
  uint32_t mSyncQueueKey;
  IDBIndexSync* mIndex;

  // In-params.
  nsRefPtr<IDBKeyRange> mKeyRange;
  const uint32_t mLimit;

  // Out-params.
  nsTArray<StructuredCloneReadInfo> mCloneReadInfos;
};

class GetAllKeysHelper : public BlockingHelperBase
{
public:
  GetAllKeysHelper(WorkerPrivate* aWorkerPrivate, uint32_t aSyncQueueKey,
                   IDBIndexSync* aIndex, IDBKeyRange* aKeyRange,
                   const uint32_t aLimit)
  : BlockingHelperBase(aWorkerPrivate, aIndex), mSyncQueueKey(aSyncQueueKey),
    mIndex(aIndex), mKeyRange(aKeyRange), mLimit(aLimit)
  { }

  virtual nsresult
  HandleResponse(const ResponseValue& aResponseValue);

  bool
  Read(JSContext* aCx, JS::MutableHandle<JS::Value> aValue);

protected:
  nsresult
  IPCThreadRun()
  {
    MOZ_ASSERT(mPrimarySyncQueueKey == UINT32_MAX, "Should be unset!");
    mPrimarySyncQueueKey = mSyncQueueKey;

    IndexRequestParams params;

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

    params = getAllKeyParams;

    IndexedDBIndexRequestWorkerChild* actor =
      new IndexedDBIndexRequestWorkerChild(params.type());
    mIndex->GetActor()->SendPIndexedDBRequestConstructor(actor, params);
    actor->SetHelper(this);

    return NS_OK;
  }

private:
  uint32_t mSyncQueueKey;
  IDBIndexSync* mIndex;

  // In-params.
  nsRefPtr<IDBKeyRange> mKeyRange;
  const uint32_t mLimit;

  // Out-params.
  nsTArray<Key> mKeys;
};

class CountHelper : public BlockingHelperBase
{
public:
  CountHelper(WorkerPrivate* aWorkerPrivate, uint32_t aSyncQueueKey,
            IDBIndexSync* aIndex, IDBKeyRange* aKeyRange)
  : BlockingHelperBase(aWorkerPrivate, aIndex), mSyncQueueKey(aSyncQueueKey),
    mIndex(aIndex), mKeyRange(aKeyRange)
  { }

  virtual nsresult
  HandleResponse(const ResponseValue& aResponseValue);

  uint64_t
  Count() const
  {
    return mCount;
  }

protected:
  nsresult
  IPCThreadRun()
  {
    MOZ_ASSERT(mPrimarySyncQueueKey == UINT32_MAX, "Should be unset!");
    mPrimarySyncQueueKey = mSyncQueueKey;

    IndexRequestParams params;
    CountParams countParams;

    if(mKeyRange) {
      KeyRange keyRange;
      mKeyRange->ToSerializedKeyRange(keyRange);
      countParams.optionalKeyRange() = keyRange;
    }
    else {
      countParams.optionalKeyRange() = mozilla::void_t();
    }

    params = countParams;

    IndexedDBIndexRequestWorkerChild* actor =
      new IndexedDBIndexRequestWorkerChild(params.type());
    mIndex->GetActor()->SendPIndexedDBRequestConstructor(actor, params);
    actor->SetHelper(this);

    return NS_OK;
  }

private:
  uint32_t mSyncQueueKey;
  IDBIndexSync* mIndex;

  // In-params.
  nsRefPtr<IDBKeyRange> mKeyRange;

  // Out-params.
  uint64_t mCount;
};

} // anonymous namespace

// static
IDBIndexSync*
IDBIndexSync::Create(JSContext* aCx, IDBObjectStoreSync* aObjectStore,
                     IndexInfo* aIndexInfo)
{
  MOZ_ASSERT(aIndexInfo, "Null pointer!");

  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);

  nsRefPtr<IDBIndexSync> index = new IDBIndexSync(aCx, workerPrivate);

  index->mObjectStore = aObjectStore;
  index->mId = aIndexInfo->id;
  index->mName = aIndexInfo->name;
  index->mKeyPath = aIndexInfo->keyPath;
  index->mUnique = aIndexInfo->unique;
  index->mMultiEntry = aIndexInfo->multiEntry;

  if (!Wrap(aCx, nullptr, index)) {
    return nullptr;
  }

  return index;
}

IDBIndexSync::IDBIndexSync(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
: IDBObjectSync(aCx, aWorkerPrivate),
  mObjectStore(nullptr),
  mActorChild(nullptr)
{ }

IDBIndexSync::~IDBIndexSync()
{
  MOZ_ASSERT(!mActorChild, "Still have an actor object attached!");
}

void
IDBIndexSync::_trace(JSTracer* aTrc)
{
  JS_CallHeapValueTracer(aTrc, &mCachedKeyPath, "mCachedKeyPath");

  IDBObjectSync::_trace(aTrc);
}

void
IDBIndexSync::_finalize(JSFreeOp* aFop)
{
  IDBObjectSync::_finalize(aFop);
}

void
IDBIndexSync::ReleaseIPCThreadObjects()
{
  AssertIsOnIPCThread();

  if (mActorChild) {
    mActorChild->Send__delete__(mActorChild);
    MOZ_ASSERT(!mActorChild, "Should have cleared in Send__delete__!");
  }
}

NS_IMPL_ISUPPORTS_INHERITED0(IDBIndexSync, IDBObjectSync)

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

  return mCachedKeyPath;
}

IDBCursorSync*
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

  IDBCursorSync* cursor = IDBCursorSync::CreateWithValue(aCx, this, direction);
  if (!cursor) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  if (!cursor->Open(aCx, keyRange)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return cursor->mHaveValue ? cursor : nullptr;
}

IDBCursorSync*
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

  IDBCursorSync* cursor = IDBCursorSync::Create(aCx, this, direction);
  if (!cursor) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  if (!cursor->Open(aCx, keyRange)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return cursor->mHaveValue ? cursor : nullptr;
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

  DOMBindingAnchor<IDBIndexSync> selfAnchor(this);

  AutoSyncLoopHolder syncLoop(mWorkerPrivate);

  nsRefPtr<GetHelper> helper =
    new GetHelper(mWorkerPrivate, syncLoop.SyncQueueKey(), this, keyRange);

  if (!helper->Dispatch(aCx) || !syncLoop.RunAndForget(aCx)) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return JSVAL_NULL;
  }

  JS::Rooted<JS::Value> value(aCx);
  if (!helper->Read(aCx, &value)) {
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

  DOMBindingAnchor<IDBIndexSync> selfAnchor(this);

  AutoSyncLoopHolder syncLoop(mWorkerPrivate);

  nsRefPtr<GetKeyHelper> helper =
    new GetKeyHelper(mWorkerPrivate, syncLoop.SyncQueueKey(), this, keyRange);

  if (!helper->Dispatch(aCx) || !syncLoop.RunAndForget(aCx)) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return JSVAL_NULL;
  }

  JS::Rooted<JS::Value> newKey(aCx);
  rv = helper->GetKey().ToJSVal(aCx, &newKey);
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

  DOMBindingAnchor<IDBIndexSync> selfAnchor(this);

  AutoSyncLoopHolder syncLoop(mWorkerPrivate);

  nsRefPtr<GetAllHelper> helper =
    new GetAllHelper(mWorkerPrivate, syncLoop.SyncQueueKey(), this,
                     keyRange, limit);

  if (!helper->Dispatch(aCx) || !syncLoop.RunAndForget(aCx)) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return JSVAL_NULL;
  }

  JS::Rooted<JS::Value> value(aCx);
  if (!helper->Read(aCx, &value)) {
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

  DOMBindingAnchor<IDBIndexSync> selfAnchor(this);

  AutoSyncLoopHolder syncLoop(mWorkerPrivate);

  nsRefPtr<GetAllKeysHelper> helper =
    new GetAllKeysHelper(mWorkerPrivate, syncLoop.SyncQueueKey(), this,
                         keyRange, limit);

  if (!helper->Dispatch(aCx) || !syncLoop.RunAndForget(aCx)) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return JSVAL_NULL;
  }

  JS::Rooted<JS::Value> value(aCx);
  if (!helper->Read(aCx, &value)) {
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

  DOMBindingAnchor<IDBIndexSync> selfAnchor(this);

  AutoSyncLoopHolder syncLoop(mWorkerPrivate);

  nsRefPtr<CountHelper> helper =
    new CountHelper(mWorkerPrivate, syncLoop.SyncQueueKey(), this, keyRange);

  if (!helper->Dispatch(aCx) || !syncLoop.RunAndForget(aCx)) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return 0;
  }

  return helper->Count();
}

bool
IDBIndexSync::Init(JSContext* aCx, IndexInfo* aIndexInfo, bool aCreating)
{
  nsRefPtr<InitRunnable> runnable = new InitRunnable(mWorkerPrivate, this,
                                                     aIndexInfo, aCreating);

  if (!runnable->Dispatch(aCx)) {
    NS_WARNING("Failed to dispatch!");
    return false;
  }

  return true;
}

nsresult
GetHelper::HandleResponse(const ResponseValue& aResponseValue)
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
GetHelper::Read(JSContext* aCx, JS::MutableHandle<JS::Value> aValue)
{
  bool result = IDBObjectStoreSync::DeserializeValue(aCx, mCloneBuffer, aValue);
  mCloneBuffer.clear();

  return result;
}

nsresult
GetKeyHelper::HandleResponse(const ResponseValue& aResponseValue)
{
  AssertIsOnIPCThread();

  MOZ_ASSERT(aResponseValue.type() == ResponseValue::TGetKeyResponse,
             "Bad response type!");

  mKey = aResponseValue.get_GetKeyResponse().key();

  return NS_OK;
}

nsresult
GetAllHelper::HandleResponse(const ResponseValue& aResponseValue)
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
GetAllHelper::Read(JSContext* aCx, JS::MutableHandle<JS::Value> aValue)
{
  MOZ_ASSERT(mCloneReadInfos.Length() <= mLimit, "Too many results!");

  nsresult rv = ConvertToArrayAndCleanup(aCx, mCloneReadInfos, aValue);

  MOZ_ASSERT(mCloneReadInfos.IsEmpty(),
             "Should have cleared in ConvertToArrayAndCleanup");
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

nsresult
GetAllKeysHelper::HandleResponse(const ResponseValue& aResponseValue)
{
  AssertIsOnIPCThread();

  MOZ_ASSERT(aResponseValue.type() == ResponseValue::TGetAllKeysResponse,
             "Bad response type!");

  mKeys.AppendElements(aResponseValue.get_GetAllKeysResponse().keys());

  return NS_OK;
}

bool
GetAllKeysHelper::Read(JSContext* aCx, JS::MutableHandle<JS::Value> aValue)
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
CountHelper::HandleResponse(const ResponseValue& aResponseValue)
{
  AssertIsOnIPCThread();

  MOZ_ASSERT(aResponseValue.type() == ResponseValue::TCountResponse,
             "Bad response type!");

  mCount = aResponseValue.get_CountResponse().count();

  return NS_OK;
}
