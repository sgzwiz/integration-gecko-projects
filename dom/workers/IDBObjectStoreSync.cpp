/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBObjectStoreSync.h"

#include "mozilla/dom/DOMStringList.h"
#include "mozilla/dom/IDBObjectStoreSyncBinding.h"
#include "mozilla/dom/indexedDB/IDBKeyRange.h"
#include "mozilla/dom/indexedDB/IndexedDatabase.h"
#include "mozilla/dom/indexedDB/IndexedDatabaseInlines.h"

#include "IDBCursorSync.h"
#include "IDBIndexSync.h"
#include "IDBTransactionSync.h"
#include "IPCThreadUtils.h"
#include "WorkerPrivate.h"

#include "ipc/IndexedDBWorkerChild.h"

USING_WORKERS_NAMESPACE
using namespace mozilla::dom::indexedDB;
using namespace mozilla::dom::indexedDB::ipc;
using mozilla::dom::DOMStringList;
using mozilla::dom::IDBCursorDirection;
using mozilla::dom::indexedDB::IDBKeyRange;
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
    mObjectStore->Transaction()->Proxy()->Actor()->SendPIndexedDBObjectStoreConstructor(
                                                                        actor,
                                                                        params);
    actor->SetObjectStoreProxy(mObjectStore->Proxy());

    return NS_OK;
  }

private:
  nsRefPtr<IDBObjectStoreSync> mObjectStore;
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
    IndexedDBObjectStoreWorkerChild* dbActor = mObjectStore->Proxy()->Actor();
    dbActor->SendDeleteIndex(mIndexName);

    return NS_OK;
  }

private:
  nsRefPtr<IDBObjectStoreSync> mObjectStore;
  nsString mIndexName;
};

class AddHelper : public ObjectStoreHelper
{
public:
  AddHelper(WorkerPrivate* aWorkerPrivate, IDBObjectStoreSync* aObjectStore,
            StructuredCloneWriteInfo& aCloneWriteInfo, const Key& aKey,
            bool aOverwrite, nsTArray<IndexUpdateInfo>& aIndexUpdateInfo)
  : ObjectStoreHelper(aWorkerPrivate, aObjectStore), mKey(aKey),
    mOverwrite(aOverwrite)
  {
    mCloneWriteInfo.Swap(aCloneWriteInfo);
    mIndexUpdateInfo.SwapElements(aIndexUpdateInfo);
  }

  virtual nsresult
  PackArguments(ObjectStoreRequestParams& aParams) MOZ_OVERRIDE;

  virtual nsresult
  UnpackResponse(const ResponseValue& aResponseValue) MOZ_OVERRIDE;

  nsresult
  GetResult(JSContext* aCx, JS::MutableHandle<JS::Value> aResult)
  {
    MOZ_ASSERT(!mKey.IsUnset(), "Badness!");

    mCloneWriteInfo.mCloneBuffer.clear();

    return mKey.ToJSVal(aCx, aResult);
  }

private:
  // In-params.
  StructuredCloneWriteInfo mCloneWriteInfo;
  Key mKey;
  nsTArray<IndexUpdateInfo> mIndexUpdateInfo;
  const bool mOverwrite;
};

class DeleteHelper : public ObjectStoreHelper
{
public:
  DeleteHelper(WorkerPrivate* aWorkerPrivate, IDBObjectStoreSync* aObjectStore,
               IDBKeyRange* aKeyRange)
  : ObjectStoreHelper(aWorkerPrivate, aObjectStore), mKeyRange(aKeyRange)
  { }

  virtual nsresult
  PackArguments(ObjectStoreRequestParams& aParams) MOZ_OVERRIDE;

  virtual nsresult
  UnpackResponse(const ResponseValue& aResponseValue) MOZ_OVERRIDE;

private:
  // In-params.
  nsRefPtr<IDBKeyRange> mKeyRange;
};

class GetHelper : public ObjectStoreHelper
{
public:
  GetHelper(WorkerPrivate* aWorkerPrivate, IDBObjectStoreSync* aObjectStore,
            IDBKeyRange* aKeyRange)
  : ObjectStoreHelper(aWorkerPrivate, aObjectStore), mKeyRange(aKeyRange)
  { }

  virtual nsresult
  PackArguments(ObjectStoreRequestParams& aParams) MOZ_OVERRIDE;

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

class ClearHelper : public ObjectStoreHelper
{
public:
  ClearHelper(WorkerPrivate* aWorkerPrivate, IDBObjectStoreSync* aObjectStore)
  : ObjectStoreHelper(aWorkerPrivate, aObjectStore)
  { }

  virtual nsresult
  PackArguments(ObjectStoreRequestParams& aParams) MOZ_OVERRIDE;

  virtual nsresult
  UnpackResponse(const ResponseValue& aResponseValue) MOZ_OVERRIDE;
};

class CountHelper : public ObjectStoreHelper
{
public:
  CountHelper(WorkerPrivate* aWorkerPrivate, IDBObjectStoreSync* aObjectStore,
              IDBKeyRange* aKeyRange)
  : ObjectStoreHelper(aWorkerPrivate, aObjectStore), mKeyRange(aKeyRange)
  { }

  virtual nsresult
  PackArguments(ObjectStoreRequestParams& aParams) MOZ_OVERRIDE;

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

class GetAllHelper : public ObjectStoreHelper
{
  typedef mozilla::dom::indexedDB::StructuredCloneReadInfo
                                                        StructuredCloneReadInfo;

public:
  GetAllHelper(WorkerPrivate* aWorkerPrivate, IDBObjectStoreSync* aObjectStore,
               IDBKeyRange* aKeyRange, const uint32_t aLimit)
  : ObjectStoreHelper(aWorkerPrivate, aObjectStore), mKeyRange(aKeyRange),
    mLimit(aLimit)
  { }

  virtual nsresult
  PackArguments(ObjectStoreRequestParams& aParams) MOZ_OVERRIDE;

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

class GetAllKeysHelper : public ObjectStoreHelper
{
public:
  GetAllKeysHelper(WorkerPrivate* aWorkerPrivate,
                   IDBObjectStoreSync* aObjectStore, IDBKeyRange* aKeyRange,
                   const uint32_t aLimit)
  : ObjectStoreHelper(aWorkerPrivate, aObjectStore), mKeyRange(aKeyRange),
    mLimit(aLimit)
  { }

  virtual nsresult
  PackArguments(ObjectStoreRequestParams& aParams) MOZ_OVERRIDE;

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

IDBObjectStoreSyncProxy::IDBObjectStoreSyncProxy(
                                               IDBObjectStoreSync* aObjectStore)
: IDBObjectSyncProxy<IndexedDBObjectStoreWorkerChild>(aObjectStore)
{
}

IDBObjectStoreSync*
IDBObjectStoreSyncProxy::ObjectStore()
{
  return static_cast<IDBObjectStoreSync*>(mObject);
}

NS_IMPL_ADDREF_INHERITED(IDBObjectStoreSync, IDBObjectSync)
NS_IMPL_RELEASE_INHERITED(IDBObjectStoreSync, IDBObjectSync)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(IDBObjectStoreSync)
NS_INTERFACE_MAP_END_INHERITING(IDBObjectSync)

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBObjectStoreSync)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IDBObjectStoreSync,
                                                IDBObjectSync)
  tmp->ReleaseProxy(ObjectIsGoingAway);
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTransaction)

  tmp->mCreatedIndexes.Clear();

  tmp->mCachedKeyPath = JSVAL_VOID;

  if (tmp->mHoldingJSVal) {
    mozilla::DropJSObjects(tmp);
    tmp->mHoldingJSVal = false;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IDBObjectStoreSync,
                                                  IDBObjectSync)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTransaction)

  for (uint32_t i = 0; i < tmp->mCreatedIndexes.Length(); i++) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mCreatedIndexes[i]");
    cb.NoteXPCOMChild(static_cast<nsISupports*>(tmp->mCreatedIndexes[i].get()));
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(IDBObjectStoreSync,
                                               IDBObjectSync)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JSVAL_MEMBER_CALLBACK(mCachedKeyPath)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

// static
already_AddRefed<IDBObjectStoreSync>
IDBObjectStoreSync::Create(JSContext* aCx, IDBTransactionSync* aTransaction,
                           ObjectStoreInfo* aStoreInfo)
{
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);

  nsRefPtr<IDBObjectStoreSync> objectStore =
    new IDBObjectStoreSync(workerPrivate);

  objectStore->mTransaction = aTransaction;
  objectStore->mName = aStoreInfo->name;
  objectStore->mId = aStoreInfo->id;
  objectStore->mKeyPath = aStoreInfo->keyPath;
  objectStore->mAutoIncrement = aStoreInfo->autoIncrement;
  objectStore->mInfo = aStoreInfo;

  return objectStore.forget();
}

IDBObjectStoreSync::IDBObjectStoreSync(WorkerPrivate* aWorkerPrivate)
: IDBObjectSync(aWorkerPrivate), mHoldingJSVal(false)
{
  SetIsDOMBinding();
}

IDBObjectStoreSync::~IDBObjectStoreSync()
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  ReleaseProxy(ObjectIsGoingAway);

  MOZ_ASSERT(!mRooted);

  if (mHoldingJSVal) {
    mCachedKeyPath = JSVAL_VOID;
    mozilla::DropJSObjects(this);
  }
}

IDBObjectStoreSyncProxy*
IDBObjectStoreSync::Proxy()
{
  return static_cast<IDBObjectStoreSyncProxy*>(mProxy.get());
}

// static
bool
IDBObjectStoreSync::DeserializeValue(JSContext* aCx,
                                     JSAutoStructuredCloneBuffer& aBuffer,
                                     JS::MutableHandle<JS::Value> aValue)
{
  MOZ_ASSERT(aCx, "A JSContext is required!");

  if (!aBuffer.nbytes()) {
    aValue.setUndefined();
    return true;
  }

  if (!aBuffer.data()) {
    aValue.setUndefined();
    return true;
  }

  JSAutoRequest ar(aCx);

  return aBuffer.read(aCx, aValue, nullptr, nullptr);
}

// static
bool
IDBObjectStoreSync::SerializeValue(JSContext* aCx,
                                   StructuredCloneWriteInfo& aCloneWriteInfo,
                                   JS::Handle<JS::Value> aValue)
{
  MOZ_ASSERT(aCx, "A JSContext is required!");

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
    MOZ_ASSERT(cloneWriteInfo->mOffsetToKeyProp == 0,
               "We should not have been here before!");
    cloneWriteInfo->mOffsetToKeyProp = js_GetSCOffset(aWriter);

    uint64_t value = 0;
    return JS_WriteBytes(aWriter, &value, sizeof(value));
  }

  return false;
}

JSObject*
IDBObjectStoreSync::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return IDBObjectStoreSyncBinding_workers::Wrap(aCx, aScope, this);
}

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

  if (JSVAL_IS_GCTHING(mCachedKeyPath)) {
    mozilla::HoldJSObjects(this);
    mHoldingJSVal = true;
  }

  return mCachedKeyPath;
}

already_AddRefed<DOMStringList>
IDBObjectStoreSync::IndexNames(JSContext* aCx)
{
  nsRefPtr<DOMStringList> list(new DOMStringList());

  nsTArray<nsString>& names = list->Names();
  uint32_t count = mInfo->indexes.Length();
  names.SetCapacity(count);

  for (uint32_t index = 0; index < count; index++) {
    names.InsertElementSorted(mInfo->indexes[index].name);
  }

  return list.forget();
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

  nsRefPtr<DeleteHelper> helper =
    new DeleteHelper(mWorkerPrivate, this, keyRange);

  if (!helper->Run(aCx)) {
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

  nsRefPtr<GetHelper> helper = new GetHelper(mWorkerPrivate, this, keyRange);

  if (!helper->Run(aCx)) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return JSVAL_NULL;
  }

  JS::Rooted<JS::Value> value(aCx);
  if (!helper->GetResult(aCx, &value)) {
    NS_WARNING("Read failed!");
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

  nsRefPtr<ClearHelper> helper =
    new ClearHelper(mWorkerPrivate, this);

  if (!helper->Run(aCx)) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return;
  }
}

already_AddRefed<IDBIndexSync>
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

already_AddRefed<IDBIndexSync>
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

already_AddRefed<IDBIndexSync>
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

  DatabaseInfoMT* databaseInfo = mTransaction->DBInfo();

  IndexInfo* info = mInfo->indexes.AppendElement();

  info->name = aName;
  info->id = databaseInfo->nextIndexId++;
  info->keyPath = aKeyPath;
  info->unique = aOptionalParameters.mUnique;
  info->multiEntry = aOptionalParameters.mMultiEntry;

  // Don't leave this in the list if we fail below!
  AutoRemoveIndex autoRemove(mInfo, info->name);

  nsRefPtr<IDBIndexSync> retval = IDBIndexSync::Create(aCx, this, info);
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

  return retval.forget();
}

already_AddRefed<IDBIndexSync>
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

  nsRefPtr<IDBIndexSync> retval;
  for (uint32_t i = 0; i < mCreatedIndexes.Length(); i++) {
    nsRefPtr<IDBIndexSync>& index = mCreatedIndexes[i];
    if (index->Name() == aName) {
      retval = index;
      return retval.forget();
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

  return retval.forget();
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

already_AddRefed<IDBCursorSync>
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

  IDBCursorSync::Direction direction =
    IDBCursorSync::ConvertDirection(aDirection);

  nsRefPtr<IDBCursorSync> cursor =
    IDBCursorSync::CreateWithValue(aCx, this, direction);
  if (!cursor) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return nullptr;
  }

  cursor->mProxy = new IDBCursorSyncProxy(cursor);

  if (!cursor->Open2(aCx, keyRange)) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return nullptr;
  }

  return cursor->mHaveValue ? cursor.forget() : nullptr;
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

  nsRefPtr<CountHelper> helper =
    new CountHelper(mWorkerPrivate, this, keyRange);

  if (!helper->Run(aCx)) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return 0;
  }

  return helper->GetResult();
}

JS::Value
IDBObjectStoreSync::GetAll(JSContext* aCx,
                           const Optional<JS::Handle<JS::Value> >& aKey,
                           const Optional<uint32_t>& aLimit, ErrorResult& aRv)
{
  if (mTransaction->IsInvalid()) {
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
IDBObjectStoreSync::GetAllKeys(JSContext* aCx,
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

already_AddRefed<IDBCursorSync>
IDBObjectStoreSync::OpenKeyCursor(JSContext* aCx,
                                  const Optional<JS::Handle<JS::Value> >& aRange,
                                  IDBCursorDirection aDirection,
                                  ErrorResult& aRv)
{
  if (mTransaction->IsInvalid()) {
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

bool
IDBObjectStoreSync::Init(JSContext* aCx, bool aCreating)
{
  mProxy = new IDBObjectStoreSyncProxy(this);

  nsRefPtr<IDBObjectStoreSync> kungFuDeathGrip = this;

  nsRefPtr<InitRunnable> runnable =
    new InitRunnable(mWorkerPrivate, this, aCreating);

  if (!runnable->Dispatch(aCx)) {
    ReleaseProxy();
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

  nsRefPtr<AddHelper> helper =
    new AddHelper(mWorkerPrivate, this, cloneWriteInfo, key, aOverwrite,
                  updateInfo);

  if (!helper->Run(aCx)) {
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
    MOZ_ASSERT(aKey.IsUnset(), "Shouldn't have gotten the key yet!");

    rv = GetKeyPath().ExtractOrCreateKey(aCx, aValue, aKey,
                                         &GetAddInfoCallback, &data);
  }
  else {
    rv = GetAddInfoCallback(aCx, &data);
  }

  return rv;
}

nsresult
ObjectStoreHelper::SendConstructor(IndexedDBRequestWorkerChildBase** aActor)
{
  ObjectStoreRequestParams params;
  PackArguments(params);

  IndexedDBObjectStoreRequestWorkerChild* actor =
    new IndexedDBObjectStoreRequestWorkerChild(params.type());
  ObjectStore()->Proxy()->Actor()->SendPIndexedDBRequestConstructor(actor,
                                                                    params);

  *aActor = actor;
  return NS_OK;
}

nsresult
AddHelper::PackArguments(ObjectStoreRequestParams& aParams)
{
  AssertIsOnIPCThread();

  ipc::AddPutParams commonParams;
  commonParams.cloneInfo() = mCloneWriteInfo;
  commonParams.key() = mKey;
  commonParams.indexUpdateInfos().AppendElements(mIndexUpdateInfo);

  if (mOverwrite) {
    PutParams putParams;
    putParams.commonParams() = commonParams;
    aParams = putParams;
  }
  else {
    AddParams addParams;
    addParams.commonParams() = commonParams;
    aParams = addParams;
  }

  return NS_OK;
}

nsresult
AddHelper::UnpackResponse(const ResponseValue& aResponseValue)
{
  AssertIsOnIPCThread();

  MOZ_ASSERT(aResponseValue.type() == ResponseValue::TAddResponse ||
             aResponseValue.type() == ResponseValue::TPutResponse,
             "Bad response type!");

  mKey = mOverwrite ?
         aResponseValue.get_PutResponse().key() :
         aResponseValue.get_AddResponse().key();

  return NS_OK;
}

nsresult
DeleteHelper::PackArguments(ObjectStoreRequestParams& aParams)
{
  AssertIsOnIPCThread();

  ipc::DeleteParams deleteParams;

  mKeyRange->ToSerializedKeyRange(deleteParams.keyRange());

  aParams = deleteParams;

  return NS_OK;
}

nsresult
DeleteHelper::UnpackResponse(const ResponseValue& aResponseValue)
{
  AssertIsOnIPCThread();

  MOZ_ASSERT(aResponseValue.type() == ResponseValue::TDeleteResponse,
             "Bad response type!");

  return NS_OK;
}

nsresult
GetHelper::PackArguments(ObjectStoreRequestParams& aParams)
{
  AssertIsOnIPCThread();

  ObjectStoreRequestParams params;

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

  if (!mCloneBuffer.copy(cloneInfo.data, cloneInfo.dataLength)) {
    NS_WARNING("Failed to copy clone buffer!");
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  return NS_OK;
}

bool
GetHelper::GetResult(JSContext* aCx, JS::MutableHandle<JS::Value> aValue)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  bool result = IDBObjectStoreSync::DeserializeValue(aCx, mCloneBuffer, aValue);
  mCloneBuffer.clear();

  return result;
}

nsresult
ClearHelper::PackArguments(ObjectStoreRequestParams& aParams)
{
  AssertIsOnIPCThread();

  aParams = ClearParams();

  return NS_OK;
}

nsresult
ClearHelper::UnpackResponse(const ResponseValue& aResponseValue)
{
  AssertIsOnIPCThread();

  MOZ_ASSERT(aResponseValue.type() == ResponseValue::TClearResponse,
             "Bad response type!");

  return NS_OK;
}

nsresult
CountHelper::PackArguments(ObjectStoreRequestParams& aParams)
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

nsresult
GetAllHelper::PackArguments(ObjectStoreRequestParams& aParams)
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
GetAllKeysHelper::PackArguments(ObjectStoreRequestParams& aParams)
{
  AssertIsOnIPCThread();

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
