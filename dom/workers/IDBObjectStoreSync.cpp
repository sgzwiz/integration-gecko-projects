/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBObjectStoreSync.h"

#include "mozilla/dom/indexedDB/IndexedDatabase.h"
#include "mozilla/dom/indexedDB/IndexedDatabaseInlines.h"

#include "BlockingHelperBase.h"
#include "DOMBindingInlines.h"
#include "DOMStringList.h"
#include "IDBCursorWithValueSync.h"
#include "IDBIndexSync.h"
#include "IDBKeyRange.h"
#include "IDBTransactionSync.h"
#include "IPCThreadUtils.h"
#include "WorkerPrivate.h"

#include "ipc/IndexedDBWorkerChild.h"

USING_WORKERS_NAMESPACE
using namespace mozilla::dom::indexedDB;
using namespace mozilla::dom::indexedDB::ipc;
using mozilla::dom::IDBCursorDirection;
using mozilla::dom::NonNull;
using mozilla::dom::Optional;
using mozilla::dom::Sequence;
using mozilla::ErrorResult;

namespace {

class InitRunnable : public BlockWorkerThreadRunnable
{
public:
  InitRunnable(WorkerPrivate* aWorkerPrivate, IDBObjectStoreSync* aObjectStore,
               bool aCreating)
  : BlockWorkerThreadRunnable(aWorkerPrivate), mObjectStore(aObjectStore),
    mCreating(aCreating)
  { }

protected:
  nsresult
  IPCThreadRun()
  {
    ObjectStoreConstructorParams params;

    if (mCreating) {
      CreateObjectStoreParams createParams;
      createParams.info() = *mObjectStore->Info();
      params = createParams;
    }
    else {
      GetObjectStoreParams getParams;
      getParams.name() = mObjectStore->Info()->name;
      params = getParams;
    }

    IndexedDBObjectStoreWorkerChild* actor =
      new IndexedDBObjectStoreWorkerChild();
    mObjectStore->Transaction()->GetActor()->SendPIndexedDBObjectStoreConstructor(
                                                                        actor,
                                                                        params);
    actor->SetObjectStore(mObjectStore);

    return NS_OK;
  }

private:
  IDBObjectStoreSync* mObjectStore;
  bool mCreating;
};

class DeleteIndexRunnable : public BlockWorkerThreadRunnable
{
public:
  DeleteIndexRunnable(WorkerPrivate* aWorkerPrivate,
                      IDBObjectStoreSync* aObjectStore, nsString aIndexName)
  : BlockWorkerThreadRunnable(aWorkerPrivate), mObjectStore(aObjectStore),
    mIndexName(aIndexName)
  { }

protected:
  nsresult
  IPCThreadRun()
  {
    IndexedDBObjectStoreWorkerChild* dbActor = mObjectStore->GetActor();
    dbActor->SendDeleteIndex(mIndexName);

    return NS_OK;
  }

private:
  IDBObjectStoreSync* mObjectStore;
  nsString mIndexName;
};

class AddHelper : public BlockingHelperBase
{
public:
  AddHelper(WorkerPrivate* aWorkerPrivate, uint32_t aSyncQueueKey,
            IDBObjectStoreSync* aObjectStore, const Key& aKey,
            bool aOverwrite, StructuredCloneWriteInfo& aCloneWriteInfo,
            nsTArray<IndexUpdateInfo>& aIndexUpdateInfo)
  : BlockingHelperBase(aWorkerPrivate, aObjectStore),
    mSyncQueueKey(aSyncQueueKey), mObjectStore(aObjectStore),
    mOverwrite(aOverwrite), mKey(aKey)
  {
    mCloneWriteInfo.Swap(aCloneWriteInfo);
    mIndexUpdateInfo.SwapElements(aIndexUpdateInfo);
  }

  virtual nsresult
  HandleResponse(const ResponseValue& aResponseValue);

  nsresult
  GetResult(JSContext* aCx, JS::MutableHandle<JS::Value> aResult)
  {
    NS_ASSERTION(!mKey.IsUnset(), "Badness!");

    mCloneWriteInfo.mCloneBuffer.clear();

    return mKey.ToJSVal(aCx, aResult);
  }

protected:
  nsresult
  IPCThreadRun()
  {
    NS_ASSERTION(mPrimarySyncQueueKey == UINT32_MAX, "Should be unset!");
    mPrimarySyncQueueKey = mSyncQueueKey;

    ipc::AddPutParams commonParams;
    commonParams.cloneInfo() = mCloneWriteInfo;
    commonParams.key() = mKey;
    commonParams.indexUpdateInfos().AppendElements(mIndexUpdateInfo);

    ObjectStoreRequestParams params;

    if (mOverwrite) {
      PutParams putParams;
      putParams.commonParams() = commonParams;
      params = putParams;
    }
    else {
      AddParams addParams;
      addParams.commonParams() = commonParams;
      params = addParams;
    }

    IndexedDBObjectStoreRequestWorkerChild* actor =
      new IndexedDBObjectStoreRequestWorkerChild(params.type());
    mObjectStore->GetActor()->SendPIndexedDBRequestConstructor(actor, params);
    actor->SetHelper(this);

    return NS_OK;
  }


private:
  uint32_t mSyncQueueKey;
  IDBObjectStoreSync* mObjectStore;

  // In-params.
  StructuredCloneWriteInfo mCloneWriteInfo;
  nsTArray<IndexUpdateInfo> mIndexUpdateInfo;
  const bool mOverwrite;

  // In/Out-params.
  Key mKey;
};

class DeleteHelper : public BlockingHelperBase
{
  typedef mozilla::dom::workers::IDBKeyRange IDBKeyRange;

public:
  DeleteHelper(WorkerPrivate* aWorkerPrivate, uint32_t aSyncQueueKey,
              IDBObjectStoreSync* aObjectStore, IDBKeyRange* aKeyRange)
  : BlockingHelperBase(aWorkerPrivate, aObjectStore),
    mSyncQueueKey(aSyncQueueKey), mObjectStore(aObjectStore),
    mKeyRange(aKeyRange)
  { }

  virtual nsresult
  HandleResponse(const ResponseValue& aResponseValue);

protected:
  nsresult
  IPCThreadRun()
  {
    NS_ASSERTION(mPrimarySyncQueueKey == UINT32_MAX, "Should be unset!");
    mPrimarySyncQueueKey = mSyncQueueKey;

    ObjectStoreRequestParams params;

    ipc::DeleteParams deleteParams;

    mKeyRange->ToSerializedKeyRange(deleteParams.keyRange());

    params = deleteParams;

    IndexedDBObjectStoreRequestWorkerChild* actor =
      new IndexedDBObjectStoreRequestWorkerChild(params.type());
    mObjectStore->GetActor()->SendPIndexedDBRequestConstructor(actor, params);
    actor->SetHelper(this);

    return NS_OK;
  }

private:
  uint32_t mSyncQueueKey;
  IDBObjectStoreSync* mObjectStore;

  // In-params.
  nsRefPtr<IDBKeyRange> mKeyRange;
};

class GetHelper : public BlockingHelperBase
{
  typedef mozilla::dom::workers::IDBKeyRange IDBKeyRange;

public:
  GetHelper(WorkerPrivate* aWorkerPrivate, uint32_t aSyncQueueKey,
            IDBObjectStoreSync* aObjectStore, IDBKeyRange* aKeyRange)
  : BlockingHelperBase(aWorkerPrivate, aObjectStore),
    mSyncQueueKey(aSyncQueueKey), mObjectStore(aObjectStore),
    mKeyRange(aKeyRange)
  { }

  virtual nsresult
  HandleResponse(const ResponseValue& aResponseValue);

  bool
  Read(JSContext* aCx, JS::MutableHandle<JS::Value> aValue);

protected:
  nsresult
  IPCThreadRun()
  {
    NS_ASSERTION(mPrimarySyncQueueKey == UINT32_MAX, "Should be unset!");
    mPrimarySyncQueueKey = mSyncQueueKey;

    ObjectStoreRequestParams params;

    GetParams getParams;

    mKeyRange->ToSerializedKeyRange(getParams.keyRange());

    params = getParams;

    IndexedDBObjectStoreRequestWorkerChild* actor =
      new IndexedDBObjectStoreRequestWorkerChild(params.type());
    mObjectStore->GetActor()->SendPIndexedDBRequestConstructor(actor, params);
    actor->SetHelper(this);

    return NS_OK;
  }

private:
  uint32_t mSyncQueueKey;
  IDBObjectStoreSync* mObjectStore;

  // In-params.
  nsRefPtr<IDBKeyRange> mKeyRange;

  // Out-params.
  JSAutoStructuredCloneBuffer mCloneBuffer;
};

class GetAllHelper : public BlockingHelperBase
{
  typedef mozilla::dom::indexedDB::StructuredCloneReadInfo
                                                        StructuredCloneReadInfo;
  typedef mozilla::dom::workers::IDBKeyRange IDBKeyRange;

public:
  GetAllHelper(WorkerPrivate* aWorkerPrivate, uint32_t aSyncQueueKey,
               IDBObjectStoreSync* aObjectStore, IDBKeyRange* aKeyRange,
               const uint32_t aLimit)
  : BlockingHelperBase(aWorkerPrivate, aObjectStore),
    mSyncQueueKey(aSyncQueueKey), mObjectStore(aObjectStore),
    mKeyRange(aKeyRange), mLimit(aLimit)
  { }

  virtual nsresult
  HandleResponse(const ResponseValue& aResponseValue);

  bool
  Read(JSContext* aCx, JS::MutableHandle<JS::Value> aValue);

protected:
  nsresult
  IPCThreadRun()
  {
    NS_ASSERTION(mPrimarySyncQueueKey == UINT32_MAX, "Should be unset!");
    mPrimarySyncQueueKey = mSyncQueueKey;

    ObjectStoreRequestParams params;

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

    IndexedDBObjectStoreRequestWorkerChild* actor =
      new IndexedDBObjectStoreRequestWorkerChild(params.type());
    mObjectStore->GetActor()->SendPIndexedDBRequestConstructor(actor, params);
    actor->SetHelper(this);

    return NS_OK;
  }

private:
  uint32_t mSyncQueueKey;
  IDBObjectStoreSync* mObjectStore;

  // In-params.
  nsRefPtr<IDBKeyRange> mKeyRange;
  const uint32_t mLimit;

  // Out-params.
  nsTArray<StructuredCloneReadInfo> mCloneReadInfos;
};

class ClearHelper : public BlockingHelperBase
{
public:
  ClearHelper(WorkerPrivate* aWorkerPrivate, uint32_t aSyncQueueKey,
              IDBObjectStoreSync* aObjectStore)
  : BlockingHelperBase(aWorkerPrivate, aObjectStore),
    mSyncQueueKey(aSyncQueueKey), mObjectStore(aObjectStore)
  { }

  virtual nsresult
  HandleResponse(const ResponseValue& aResponseValue);

protected:
  nsresult
  IPCThreadRun()
  {
    NS_ASSERTION(mPrimarySyncQueueKey == UINT32_MAX, "Should be unset!");
    mPrimarySyncQueueKey = mSyncQueueKey;

    ObjectStoreRequestParams params;
    params = ClearParams();

    IndexedDBObjectStoreRequestWorkerChild* actor =
      new IndexedDBObjectStoreRequestWorkerChild(params.type());
    mObjectStore->GetActor()->SendPIndexedDBRequestConstructor(actor, params);
    actor->SetHelper(this);

    return NS_OK;
  }

private:
  uint32_t mSyncQueueKey;
  IDBObjectStoreSync* mObjectStore;
};

class CountHelper : public BlockingHelperBase
{
  typedef mozilla::dom::workers::IDBKeyRange IDBKeyRange;

public:
  CountHelper(WorkerPrivate* aWorkerPrivate, uint32_t aSyncQueueKey,
              IDBObjectStoreSync* aObjectStore, IDBKeyRange* aKeyRange)
  : BlockingHelperBase(aWorkerPrivate, aObjectStore),
    mSyncQueueKey(aSyncQueueKey), mObjectStore(aObjectStore),
    mKeyRange(aKeyRange)
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
    NS_ASSERTION(mPrimarySyncQueueKey == UINT32_MAX, "Should be unset!");
    mPrimarySyncQueueKey = mSyncQueueKey;

    ObjectStoreRequestParams params;
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

    IndexedDBObjectStoreRequestWorkerChild* actor =
      new IndexedDBObjectStoreRequestWorkerChild(params.type());
    mObjectStore->GetActor()->SendPIndexedDBRequestConstructor(actor, params);
    actor->SetHelper(this);

    return NS_OK;
  }

private:
  uint32_t mSyncQueueKey;
  IDBObjectStoreSync* mObjectStore;

  // In-params.
  nsRefPtr<IDBKeyRange> mKeyRange;

  // Out-params.
  uint64_t mCount;
};

struct MOZ_STACK_CLASS GetAddInfoClosure
{
  IDBObjectStoreSync* mThis;
  StructuredCloneWriteInfo& mCloneWriteInfo;
  JS::Handle<JS::Value> mValue;
};

nsresult
GetAddInfoCallback(JSContext* aCx, void* aClosure)
{
  GetAddInfoClosure* data = static_cast<GetAddInfoClosure*>(aClosure);

  data->mCloneWriteInfo.mOffsetToKeyProp = 0;
  data->mCloneWriteInfo.mTransaction = nullptr;

  if (!IDBObjectStoreSync::SerializeValue(aCx, data->mCloneWriteInfo,
                                          data->mValue)) {
    NS_WARNING("IDBObjectStore::SerializeValue failed!");
    return NS_ERROR_DOM_DATA_CLONE_ERR;
  }

  return NS_OK;
}

} // anonymous namespace

// static
IDBObjectStoreSync*
IDBObjectStoreSync::Create(JSContext* aCx, IDBTransactionSync* aTransaction,
                           ObjectStoreInfo* aStoreInfo)
{
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);

  nsRefPtr<IDBObjectStoreSync> objectStore =
    new IDBObjectStoreSync(aCx, workerPrivate);

  objectStore->mTransaction = aTransaction;
  objectStore->mName = aStoreInfo->name;
  objectStore->mId = aStoreInfo->id;
  objectStore->mKeyPath = aStoreInfo->keyPath;
  objectStore->mAutoIncrement = aStoreInfo->autoIncrement;
  objectStore->mInfo = aStoreInfo;

  if (!Wrap(aCx, nullptr, objectStore)) {
    return nullptr;
  }

  return objectStore;
}

IDBObjectStoreSync::IDBObjectStoreSync(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
: IDBObjectSync(aCx, aWorkerPrivate), mTransaction(nullptr),
  mActorChild(nullptr)
{ }

IDBObjectStoreSync::~IDBObjectStoreSync()
{
  NS_ASSERTION(!mActorChild, "Still have an actor object attached!");
}

// static
bool
IDBObjectStoreSync::DeserializeValue(JSContext* aCx,
                                     JSAutoStructuredCloneBuffer& aBuffer,
                                     JS::MutableHandle<JS::Value> aValue)
{
  NS_ASSERTION(aCx, "A JSContext is required!");

  if (!aBuffer.nbytes()) {
    aValue.setUndefined();
    return true;
  }

  if (!aBuffer.data()) {
    aValue.setUndefined();
    return true;
  }

  JSAutoRequest ar(aCx);

  return aBuffer.read(aCx, aValue.address(), nullptr, nullptr);
}

// static 
bool
IDBObjectStoreSync::SerializeValue(JSContext* aCx,
                                   StructuredCloneWriteInfo& aCloneWriteInfo,
                                   JS::Handle<JS::Value> aValue)
{
  NS_ASSERTION(aCx, "A JSContext is required!");

  JSAutoRequest ar(aCx);

  JSStructuredCloneCallbacks callbacks = {
    nullptr,
    StructuredCloneWriteCallback,
    nullptr
  };

  JSAutoStructuredCloneBuffer& buffer = aCloneWriteInfo.mCloneBuffer;

  return buffer.write(aCx, aValue, &callbacks, &aCloneWriteInfo);
}

// static
bool
IDBObjectStoreSync::StructuredCloneWriteCallback(
                                               JSContext* aCx,
                                               JSStructuredCloneWriter* aWriter,
                                               JS::Handle<JSObject*> aObj,
                                               void* aClosure)
{
  StructuredCloneWriteInfo* cloneWriteInfo =
    reinterpret_cast<StructuredCloneWriteInfo*>(aClosure);

  if (JS_GetClass(aObj) == &IDBObjectStore::sDummyPropJSClass) {
    NS_ASSERTION(cloneWriteInfo->mOffsetToKeyProp == 0,
                 "We should not have been here before!");
    cloneWriteInfo->mOffsetToKeyProp = js_GetSCOffset(aWriter);

    uint64_t value = 0;
    return JS_WriteBytes(aWriter, &value, sizeof(value));
  }

  return false;
}

void
IDBObjectStoreSync::_trace(JSTracer* aTrc)
{
  JS_CallHeapValueTracer(aTrc, &mCachedKeyPath, "mCachedKeyPath");

  IDBObjectSync::_trace(aTrc);
}

void
IDBObjectStoreSync::_finalize(JSFreeOp* aFop)
{
  IDBObjectSync::_finalize(aFop);
}

void
IDBObjectStoreSync::ReleaseIPCThreadObjects()
{
  AssertIsOnIPCThread();

  if (mActorChild) {
    mActorChild->Send__delete__(mActorChild);
    NS_ASSERTION(!mActorChild, "Should have cleared in Send__delete__!");
  }
}

NS_IMPL_ISUPPORTS_INHERITED0(IDBObjectStoreSync, IDBObjectSync)

JS::Value
IDBObjectStoreSync::GetKeyPath(JSContext* aCx, ErrorResult& aRv)
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

DOMStringList*
IDBObjectStoreSync::IndexNames(JSContext* aCx)
{
  nsAutoTArray<nsString, 10> names;
  uint32_t count = mInfo->indexes.Length();
  names.SetCapacity(count);

  for (uint32_t index = 0; index < count; index++) {
    names.InsertElementSorted(mInfo->indexes[index].name);
  }

  return DOMStringList::Create(aCx, names);
}

JS::Value
IDBObjectStoreSync::Put(JSContext* aCx, JS::Value aValue,
                        const Optional<JS::Handle<JS::Value> >& aKey,
                        ErrorResult& aRv)
{
  return AddOrPut(aCx, aValue, aKey, true, aRv);
}

JS::Value
IDBObjectStoreSync::Add(JSContext* aCx, JS::Value aValue,
                        const Optional<JS::Handle<JS::Value> >& aKey,
                        ErrorResult& aRv)
{
  return AddOrPut(aCx, aValue, aKey, false, aRv);
}

void
IDBObjectStoreSync::Delete(JSContext* aCx, JS::Value aKey, ErrorResult& aRv)
{
  if (mTransaction->IsInvalid()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return;
  }

  if (!IsWriteAllowed()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_READ_ONLY_ERR);
    return;
  }

  nsRefPtr<IDBKeyRange> keyRange;
  nsresult rv = IDBKeyRange::FromJSVal(aCx, aKey, getter_AddRefs(keyRange));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  if (!keyRange) {
    // Key or KeyRange must be specified for delete().
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
    return;
  }

  DOMBindingAnchor<IDBObjectStoreSync> selfAnchor(this);

  AutoSyncLoopHolder syncLoop(mWorkerPrivate);

  nsRefPtr<DeleteHelper> helper =
    new DeleteHelper(mWorkerPrivate, syncLoop.SyncQueueKey(), this, keyRange);

  if (!helper->Dispatch(aCx) || !syncLoop.RunAndForget(aCx)) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return;
  }
}


JS::Value
IDBObjectStoreSync::Get(JSContext* aCx, JS::Value aKey, ErrorResult& aRv)
{
  if (mTransaction->IsInvalid()) {
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
    NS_WARNING("Must specify a key or keyRange for get()!");
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
    return JSVAL_NULL;
  }

  DOMBindingAnchor<IDBObjectStoreSync> selfAnchor(this);

  AutoSyncLoopHolder syncLoop(mWorkerPrivate);

  nsRefPtr<GetHelper> helper =
    new GetHelper(mWorkerPrivate, syncLoop.SyncQueueKey(), this, keyRange);

  if (!helper->Dispatch(aCx) || !syncLoop.RunAndForget(aCx)) {
    NS_WARNING("Failed to dispatch or RunAndForget!");
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return JSVAL_NULL;
  }

  JS::Rooted<JS::Value> value(aCx);
  if (!helper->Read(aCx, &value)) {
    NS_WARNING("Read failed!");
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return JSVAL_NULL;
  }

  return value;
}

JS::Value
IDBObjectStoreSync::GetAll(JSContext* aCx,
                              const Optional<JS::Handle<JS::Value> >& aKey,
                              const Optional<uint32_t>& aLimit,
                              ErrorResult& aRv)
{
  if (mTransaction->IsInvalid()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return JSVAL_NULL;
  }

  nsRefPtr<IDBKeyRange> keyRange;
  if (aKey.WasPassed()) {
    nsresult rv =
      IDBKeyRange::FromJSVal(aCx, aKey.Value(), getter_AddRefs(keyRange));
    if (NS_FAILED(rv)) {
      NS_WARNING("KeyRange parsing failed!");
      aRv.Throw(rv);
      return JSVAL_NULL;
    }
  }

  uint32_t limit = UINT32_MAX;
  if (aLimit.WasPassed() && aLimit.Value() != 0) {
    limit = aLimit.Value();
  }

  DOMBindingAnchor<IDBObjectStoreSync> selfAnchor(this);

  AutoSyncLoopHolder syncLoop(mWorkerPrivate);

  nsRefPtr<GetAllHelper> helper =
    new GetAllHelper(mWorkerPrivate, syncLoop.SyncQueueKey(), this, keyRange,
                     limit);

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

void
IDBObjectStoreSync::Clear(JSContext* aCx, ErrorResult& aRv)
{
  if (mTransaction->IsInvalid()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return;
  }

  if (!IsWriteAllowed()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_READ_ONLY_ERR);
    return;
  }

  DOMBindingAnchor<IDBObjectStoreSync> selfAnchor(this);

  AutoSyncLoopHolder syncLoop(mWorkerPrivate);

  nsRefPtr<ClearHelper> helper =
    new ClearHelper(mWorkerPrivate, syncLoop.SyncQueueKey(), this);

  if (!helper->Dispatch(aCx) || !syncLoop.RunAndForget(aCx)) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return;
  }
}

IDBIndexSync*
IDBObjectStoreSync::CreateIndex(
                           JSContext* aCx, const nsAString& aName,
                           const nsAString& aKeyPath,
                           const IDBIndexParameters& aOptionalParameters,
                           ErrorResult& aRv)
{
  indexedDB::KeyPath keyPath(0);
  if (NS_FAILED(KeyPath::Parse(aCx, aKeyPath, &keyPath)) ||
      !keyPath.IsValid()) {
    NS_WARNING("KeyPath parsing failed or keyPath is invalid!");
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return nullptr;
  }

  return CreateIndex(aCx, aName, keyPath, aOptionalParameters, aRv);
}

IDBIndexSync*
IDBObjectStoreSync::CreateIndex(
                           JSContext* aCx, const nsAString& aName,
                           const Sequence<nsString>& aKeyPath,
                           const IDBIndexParameters& aOptionalParameters,
                           ErrorResult& aRv)
{
  if (!aKeyPath.Length()) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return nullptr;
  }

  indexedDB::KeyPath keyPath(0);
  if (NS_FAILED(KeyPath::Parse(aCx, aKeyPath, &keyPath)) ||
      !keyPath.IsValid()) {
    NS_WARNING("KeyPath parsing failed or keyPath is invalid!");
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return nullptr;
  }

  return CreateIndex(aCx, aName, keyPath, aOptionalParameters, aRv);
}

IDBIndexSync*
IDBObjectStoreSync::CreateIndex(
                           JSContext* aCx, const nsAString& aName,
                           KeyPath& aKeyPath,
                           const IDBIndexParameters& aOptionalParameters,
                           ErrorResult& aRv)
{
  if (mTransaction->IsInvalid()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return nullptr;
  }

  if (mTransaction->GetMode() != IDBTransactionBase::VERSION_CHANGE)
  {
    NS_WARNING("Not in versionChange transaction!");
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return nullptr;
  }

  if (InfoContainsIndexName(aName)) {
    NS_WARNING("Index with same name already exists!");
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_CONSTRAINT_ERR);
    return nullptr;
  }

  if (aOptionalParameters.mMultiEntry && aKeyPath.IsArray()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return nullptr;
  }

  DatabaseInfoSync* databaseInfo = mTransaction->DBInfo();

  IndexInfo* info = mInfo->indexes.AppendElement();

  info->name = aName;
  info->id = databaseInfo->nextIndexId++;
  info->keyPath = aKeyPath;
  info->unique = aOptionalParameters.mUnique;
  info->multiEntry = aOptionalParameters.mMultiEntry;

  // Don't leave this in the list if we fail below!
  AutoRemoveIndex autoRemove(mInfo, info->name);

  IDBIndexSync* retval = IDBIndexSync::Create(aCx, this, info);
  if (!retval) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return nullptr;
  }

  if (!retval->Init(aCx, info, true)) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return nullptr;
  }

  mCreatedIndexes.AppendElement(retval);

  autoRemove.forget();

  return retval;
}

IDBIndexSync*
IDBObjectStoreSync::Index(JSContext* aCx, const nsAString& aName,
                          ErrorResult& aRv)
{
  if (mTransaction->IsInvalid()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return nullptr;
  }

  IndexInfo* indexInfo = nullptr;

  uint32_t indexCount = mInfo->indexes.Length();
  for (uint32_t index = 0; index < indexCount; index++) {
    if (mInfo->indexes[index].name == aName) {
      indexInfo = &(mInfo->indexes[index]);
      break;
    }
  }

  if (!indexInfo) {
    NS_WARNING("Index not found in ObjectStoreInfo!");
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_FOUND_ERR);
    return nullptr;
  }

  IDBIndexSync* retval = nullptr;
  for (uint32_t i = 0; i < mCreatedIndexes.Length(); i++) {
    nsRefPtr<IDBIndexSync>& index = mCreatedIndexes[i];
    if (index->Name() == aName) {
      retval = index;
      return retval;
    }
  }

  if (!retval) {
    retval = IDBIndexSync::Create(aCx, this, indexInfo);
    if (!retval) {
      aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
      return nullptr;
    }

    if (!retval->Init(aCx, indexInfo, false)) {
      aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
      return nullptr;
    }

    if (!mCreatedIndexes.AppendElement(retval)) {
      NS_WARNING("Out of memory!");
      aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
      return nullptr;
    }
  }

  return retval;
}

void
IDBObjectStoreSync::DeleteIndex(JSContext* aCx, const nsAString& aIndexName,
                                ErrorResult& aRv)
{
  if (mTransaction->IsInvalid()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return;
  }

  if (mTransaction->GetMode() != IDBTransactionBase::VERSION_CHANGE)
  {
    NS_WARNING("Not in versionChange transaction!");
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return;
  }

  uint32_t index = 0;
  for (; index < mInfo->indexes.Length(); index++) {
    if (mInfo->indexes[index].name == aIndexName) {
      break;
    }
  }

  if (index == mInfo->indexes.Length()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_FOUND_ERR);
    return;
  }

  nsRefPtr<DeleteIndexRunnable> runnable =
    new DeleteIndexRunnable(mWorkerPrivate, this, nsString(aIndexName));

  if (!runnable->Dispatch(aCx)) {
    NS_WARNING("Runnable did not Dispatch!");
    return;
  }

  mInfo->indexes.RemoveElementAt(index);

  for (uint32_t i = 0; i < mCreatedIndexes.Length(); i++) {
    if (mCreatedIndexes[i]->Name() == aIndexName) {
      mCreatedIndexes.RemoveElementAt(i);
      break;
    }
  }

}

IDBCursorWithValueSync*
IDBObjectStoreSync::OpenCursor(JSContext* aCx,
                               const Optional<JS::Handle<JS::Value> >& aRange,
                               IDBCursorDirection aDirection,
                               ErrorResult& aRv)
{
  if (mTransaction->IsInvalid()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return nullptr;
  }

  nsresult rv;

  nsRefPtr<IDBKeyRange> keyRange;
  if (aRange.WasPassed()) {
    rv = IDBKeyRange::FromJSVal(aCx, aRange.Value(), getter_AddRefs(keyRange));
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return nullptr;
    }
  }

/*
  IDBCursorSync::Direction direction = IDBCursorSync::NEXT;
  if (aDirection.WasPassed()) {
    nsresult rv = IDBCursorSync::ParseDirection(aDirection.Value(), &direction);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return nullptr;
    }
  }
*/
  IDBCursorSync::Direction direction =
    IDBCursorSync::ConvertDirection(aDirection);

  IDBCursorWithValueSync* cursor =
    IDBCursorWithValueSync::Create(aCx, this, direction);

  if (!cursor) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return nullptr;
  }

  if (!cursor->Open(aCx, keyRange)) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return nullptr;
  }

  return cursor->mHaveValue ? cursor : nullptr;
}

uint64_t
IDBObjectStoreSync::Count(JSContext* aCx,
                          const Optional<JS::Handle<JS::Value> >& aValue,
                          ErrorResult& aRv)
{
  if (mTransaction->IsInvalid()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return 0;
  }

  nsRefPtr<IDBKeyRange> keyRange;
  if (aValue.WasPassed()) {
    nsresult rv =
      IDBKeyRange::FromJSVal(aCx, aValue.Value(), getter_AddRefs(keyRange));
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return 0;
    }
  }

  DOMBindingAnchor<IDBObjectStoreSync> selfAnchor(this);

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
IDBObjectStoreSync::Init(JSContext* aCx, bool aCreating)
{
  nsRefPtr<InitRunnable> runnable =
    new InitRunnable(mWorkerPrivate, this, aCreating);

  if (!runnable->Dispatch(aCx)) {
    return false;
  }

  return true;
}

JS::Value
IDBObjectStoreSync::AddOrPut(JSContext* aCx, JS::Value aValue,
                             const Optional<JS::Handle<JS::Value> >& aKey,
                             bool aOverwrite, ErrorResult& aRv)
{
  if (mTransaction->IsInvalid()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return JSVAL_NULL;
  }

  if (!IsWriteAllowed()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_READ_ONLY_ERR);
    return JSVAL_NULL;
  }

  JS::Rooted<JS::Value> keyval(aCx,
    aKey.WasPassed() ? aKey.Value() : JSVAL_VOID);

  StructuredCloneWriteInfo cloneWriteInfo;
  Key key;
  nsTArray<IndexUpdateInfo> updateInfo;

  JS::Rooted<JS::Value> value(aCx, aValue);
  nsresult rv = GetAddInfo(aCx, value, keyval, cloneWriteInfo, key,
                           updateInfo);
  if (NS_FAILED(rv)) {
    NS_WARNING("GetAddInfo failed!");
    aRv.Throw(rv);
    return JSVAL_NULL;
  }

  DOMBindingAnchor<IDBObjectStoreSync> selfAnchor(this);

  AutoSyncLoopHolder syncLoop(mWorkerPrivate);

  nsRefPtr<AddHelper> helper =
    new AddHelper(mWorkerPrivate, syncLoop.SyncQueueKey(), this, key,
                  aOverwrite, cloneWriteInfo, updateInfo);

  if (!helper->Dispatch(aCx) || !syncLoop.RunAndForget(aCx)) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return JSVAL_NULL;
  }

  JS::Rooted<JS::Value> newKey(aCx);
  rv = helper->GetResult(aCx, &newKey);
  NS_ENSURE_SUCCESS(rv, JSVAL_NULL);

  return newKey;
}

nsresult
IDBObjectStoreSync::GetAddInfo(JSContext* aCx,
                               JS::Handle<JS::Value> aValue,
                               JS::Handle<JS::Value> aKeyVal,
                               StructuredCloneWriteInfo& aCloneWriteInfo,
                               Key& aKey,
                               nsTArray<IndexUpdateInfo>& aUpdateInfoArray)
{
  nsresult rv;

  // Return DATA_ERR if a key was passed in and this objectStore uses inline
  // keys.
  if (!JSVAL_IS_VOID(aKeyVal) && HasValidKeyPath()) {
    NS_WARNING("Key was passed in and this objectStore uses inline keys.");
    return NS_ERROR_DOM_INDEXEDDB_DATA_ERR;
  }

  JSAutoRequest ar(aCx);

  if (!HasValidKeyPath()) {
    // Out-of-line keys must be passed in.
    rv = aKey.SetFromJSVal(aCx, aKeyVal);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  else if (!mAutoIncrement) {
    rv = mKeyPath.ExtractKey(aCx, aValue, aKey);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  if (aKey.IsUnset() && !mAutoIncrement) {
    NS_WARNING("No key was specified, this is not an autoIncrement objectStore");
    return NS_ERROR_DOM_INDEXEDDB_DATA_ERR;
  }

  // Figure out indexes and the index values to update here.
  uint32_t count = mInfo->indexes.Length();
  aUpdateInfoArray.SetCapacity(count); // Pretty good estimate
  for (uint32_t indexesIndex = 0; indexesIndex < count; indexesIndex++) {
    const IndexInfo& indexInfo = mInfo->indexes[indexesIndex];

    rv = AppendIndexUpdateInfo(indexInfo.id, indexInfo.keyPath,
                               indexInfo.unique, indexInfo.multiEntry, aCx,
                               aValue, aUpdateInfoArray);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  GetAddInfoClosure data = {this, aCloneWriteInfo, aValue};

  if (mAutoIncrement && HasValidKeyPath()) {
    NS_ASSERTION(aKey.IsUnset(), "Shouldn't have gotten the key yet!");

    rv = GetKeyPath().ExtractOrCreateKey(aCx, aValue, aKey,
                                         &GetAddInfoCallback, &data);
  }
  else {
    rv = GetAddInfoCallback(aCx, &data);
  }

  return rv;
}

nsresult
AddHelper::HandleResponse(const ResponseValue& aResponseValue)
{
  AssertIsOnIPCThread();

  NS_ASSERTION(aResponseValue.type() == ResponseValue::TAddResponse ||
               aResponseValue.type() == ResponseValue::TPutResponse,
               "Bad response type!");

  mKey = mOverwrite ?
         aResponseValue.get_PutResponse().key() :
         aResponseValue.get_AddResponse().key();

  return NS_OK;
}

nsresult
DeleteHelper::HandleResponse(const ResponseValue& aResponseValue)
{
  AssertIsOnIPCThread();

  NS_ASSERTION(aResponseValue.type() == ResponseValue::TDeleteResponse,
               "Bad response type!");

  return NS_OK;
}

nsresult
GetHelper::HandleResponse(const ResponseValue& aResponseValue)
{
  AssertIsOnIPCThread();

  NS_ASSERTION(aResponseValue.type() == ResponseValue::TGetResponse,
               "Bad response type!");

  const GetResponse& getResponse = aResponseValue.get_GetResponse();
  const SerializedStructuredCloneReadInfo& cloneInfo = getResponse.cloneInfo();

  if (!mCloneBuffer.copy(cloneInfo.data, cloneInfo.dataLength)) {
    NS_WARNING("Failed to copy clone buffer!");
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

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
GetAllHelper::HandleResponse(const ResponseValue& aResponseValue)
{
  AssertIsOnIPCThread();

  NS_ASSERTION(aResponseValue.type() == ResponseValue::TGetAllResponse,
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
  NS_ASSERTION(mCloneReadInfos.Length() <= mLimit, "Too many results!");

  nsresult rv = ConvertToArrayAndCleanup(aCx, mCloneReadInfos, aValue);

  NS_ASSERTION(mCloneReadInfos.IsEmpty(),
               "Should have cleared in ConvertToArrayAndCleanup");
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

nsresult
ClearHelper::HandleResponse(const ResponseValue& aResponseValue)
{
  AssertIsOnIPCThread();

  NS_ASSERTION(aResponseValue.type() == ResponseValue::TClearResponse,
               "Bad response type!");

  return NS_OK;
}

nsresult
CountHelper::HandleResponse(const ResponseValue& aResponseValue)
{
  AssertIsOnIPCThread();

  NS_ASSERTION(aResponseValue.type() == ResponseValue::TCountResponse,
               "Bad response type!");

  mCount = aResponseValue.get_CountResponse().count();

  return NS_OK;
}
