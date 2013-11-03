/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_idbobjectstoresync_h__
#define mozilla_dom_workers_idbobjectstoresync_h__

#include "Workers.h"

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/indexedDB/IDBObjectStoreBase.h"

#include "BlockingHelperBase.h"
#include "DatabaseInfoMT.h"
#include "IDBObjectSync.h"
#include "IDBTransactionSync.h"

namespace mozilla {
namespace dom {
class DOMStringList;
struct IDBIndexParameters;
namespace indexedDB {
struct IndexUpdateInfo;
struct ObjectStoreInfo;
namespace ipc {
class ObjectStoreRequestParams;
} // namespace ipc
} // namespace indexedDB
} // namespace dom
} // namespace mozilla

BEGIN_WORKERS_NAMESPACE

class IDBCursorSync;
class IDBIndexSync;
class IDBObjectStoreSync;
class IndexedDBObjectStoreWorkerChild;

class IDBObjectStoreSyncProxy : public IDBObjectSyncProxy<IndexedDBObjectStoreWorkerChild>
{
public:
  IDBObjectStoreSyncProxy(IDBObjectStoreSync* aObjectStore);

  IDBObjectStoreSync*
  ObjectStore();
};

class IDBObjectStoreSync MOZ_FINAL : public IDBObjectSync,
                                     public indexedDB::IDBObjectStoreBase
{
  friend class IDBTransactionSync;

  typedef mozilla::dom::IDBIndexParameters IDBIndexParameters;
  typedef mozilla::dom::indexedDB::Key Key;
  typedef mozilla::dom::indexedDB::IndexUpdateInfo IndexUpdateInfo;
  typedef mozilla::dom::indexedDB::ObjectStoreInfo ObjectStoreInfo;
  typedef mozilla::dom::indexedDB::StructuredCloneWriteInfo
                                                       StructuredCloneWriteInfo;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(IDBObjectStoreSync,
                                                         IDBObjectSync);

  static already_AddRefed<IDBObjectStoreSync>
  Create(JSContext* aCx, IDBTransactionSync* aTransaction,
         ObjectStoreInfo* aStoreInfo);

  IDBObjectStoreSyncProxy*
  Proxy();

  static bool
  DeserializeValue(JSContext* aCx,
                   JSAutoStructuredCloneBuffer& aBuffer,
                   JS::MutableHandle<JS::Value> aValue);

  static bool
  SerializeValue(JSContext* aCx,
                 StructuredCloneWriteInfo& aCloneWriteInfo,
                 JS::Handle<JS::Value> aValue);

  static bool
  StructuredCloneWriteCallback(JSContext* aCx,
                               JSStructuredCloneWriter* aWriter,
                               JS::Handle<JSObject*> aObj,
                               void* aClosure);

  const indexedDB::KeyPath&
  GetKeyPath() const
  {
    return mKeyPath;
  }

  bool IsWriteAllowed() const
  {
    return mTransaction->IsWriteAllowed();
  }

  // WebIDL
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  nsISupports*
  GetParentObject() const
  {
    return nullptr;
  }

  void
  GetName(nsString& aName)
  {
    aName = mName;
  }

  JS::Value
  GetKeyPath(JSContext* aCx, ErrorResult& aRv);

  already_AddRefed<DOMStringList>
  IndexNames(JSContext* aCx);

  IDBTransactionSync*
  Transaction()
  {
    return mTransaction;
  }

  bool
  AutoIncremenent()
  {
    return mAutoIncrement;
  }

  JS::Value
  Put(JSContext* aCx, JS::Value aValue,
      const Optional<JS::Handle<JS::Value> >& aKey, ErrorResult& aRv);

  JS::Value
  Add(JSContext* aCx, JS::Value aValue,
      const Optional<JS::Handle<JS::Value> >& aKey, ErrorResult& aRv);

  void
  Delete(JSContext* aCx, JS::Value aKey,
         ErrorResult& aRv);

  JS::Value
  Get(JSContext* aCx, JS::Value aKey, ErrorResult& aRv);

  void
  Clear(JSContext* aCx, ErrorResult& aRv);

  already_AddRefed<IDBIndexSync>
  CreateIndex(JSContext* aCx, const nsAString& aName, const nsAString& aKeyPath,
              const IDBIndexParameters& aOptionalParameters,
              ErrorResult& aRv);

  already_AddRefed<IDBIndexSync>
  CreateIndex(JSContext* aCx, const nsAString& aName,
              const Sequence<nsString>& aKeyPath,
              const IDBIndexParameters& aOptionalParameters,
              ErrorResult& aRv);

  already_AddRefed<IDBIndexSync>
  Index(JSContext* aCx, const nsAString& aName, ErrorResult& aRv);

  void
  DeleteIndex(JSContext* aCx, const nsAString& aIndexName, ErrorResult& aRv);

  already_AddRefed<IDBCursorSync>
  OpenCursor(JSContext* aCx, const Optional<JS::Handle<JS::Value> >& aRange,
             IDBCursorDirection aDirection, ErrorResult& aRv);

  uint64_t
  Count(JSContext* aCx, const Optional<JS::Handle<JS::Value> >& aValue,
        ErrorResult& aRv);

  JS::Value
  GetAll(JSContext* aCx, const Optional<JS::Handle<JS::Value> >& aKey,
         const Optional<uint32_t>& aLimit, ErrorResult& aRv);

  JS::Value
  GetAllKeys(JSContext* aCx, const Optional<JS::Handle<JS::Value> >& aKey,
             const Optional<uint32_t>& aLimit, ErrorResult& aRv);

  already_AddRefed<IDBCursorSync>
  OpenKeyCursor(JSContext* aCx, const Optional<JS::Handle<JS::Value> >& aRange,
                IDBCursorDirection aDirection, ErrorResult& aRv);

private:
  IDBObjectStoreSync(WorkerPrivate* aWorkerPrivate);
  ~IDBObjectStoreSync();

  bool
  Init(JSContext* aCx, bool aCreating);

  JS::Value
  AddOrPut(JSContext* aCx, JS::Value aValue,
           const Optional<JS::Handle<JS::Value> >& aKey, bool aOverwrite,
           ErrorResult& aRv);

  nsresult
  GetAddInfo(JSContext* aCx,
             JS::Handle<JS::Value> aValue,
             JS::Handle<JS::Value> aKeyVal,
             StructuredCloneWriteInfo& aCloneWriteInfo,
             Key& aKey,
             nsTArray<IndexUpdateInfo>& aUpdateInfoArray);

  already_AddRefed<IDBIndexSync>
  CreateIndex(JSContext* aCx, const nsAString& aName,
              indexedDB::KeyPath& aKeyPath,
              const IDBIndexParameters& aOptionalParameters,
              ErrorResult& aRv);

  nsTArray<nsRefPtr<IDBIndexSync> > mCreatedIndexes;

  nsRefPtr<IDBTransactionSync> mTransaction;

  bool mHoldingJSVal;
};

class ObjectStoreHelper : public BlockingHelperBase
{
public:
  typedef mozilla::dom::indexedDB::ipc::ObjectStoreRequestParams
                                                       ObjectStoreRequestParams;

  ObjectStoreHelper(WorkerPrivate* aWorkerPrivate,
                    IDBObjectStoreSync* aObjectStore)
  : BlockingHelperBase(aWorkerPrivate, aObjectStore)
  { }

  IDBObjectStoreSync*
  ObjectStore()
  {
    return static_cast<IDBObjectStoreSync*>(mObject.get());
  }

  virtual nsresult
  SendConstructor(IndexedDBRequestWorkerChildBase** aActor) MOZ_OVERRIDE;

  virtual nsresult
  PackArguments(ObjectStoreRequestParams& aParams) = 0;

  virtual nsresult
  UnpackResponse(const ResponseValue& aResponseValue) = 0;
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_idbobjectstoresync_h__
