/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_idbindexsync_h__
#define mozilla_dom_workers_idbindexsync_h__

#include "Workers.h"

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/indexedDB/IDBIndexBase.h"

#include "BlockingHelperBase.h"
#include "DatabaseInfoMT.h"
#include "IDBObjectSync.h"

namespace mozilla {
namespace dom {
namespace indexedDB {
struct IndexInfo;
namespace ipc {
class IndexRequestParams;
} // namespace ipc
} // namespace indexedDB
} // namespace dom
} // namespace mozilla

BEGIN_WORKERS_NAMESPACE

class IDBCursorSync;
class IDBIndexSync;
class IDBObjectStoreSync;
class IndexedDBIndexWorkerChild;

class IDBIndexSyncProxy : public IDBObjectSyncProxy<IndexedDBIndexWorkerChild>
{
public:
  IDBIndexSyncProxy(IDBIndexSync* aIndex);

  IDBIndexSync*
  Index();
};

class IDBIndexSync MOZ_FINAL : public IDBObjectSync,
                               public indexedDB::IDBIndexBase
{
  friend class IDBObjectStoreSync;

  typedef mozilla::dom::indexedDB::IndexInfo IndexInfo;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(IDBIndexSync,
                                                         IDBObjectSync);

  static already_AddRefed<IDBIndexSync>
  Create(JSContext* aCx, IDBObjectStoreSync* aObjectStore,
         IndexInfo* aIndexInfo);

  IDBIndexSyncProxy*
  Proxy();

  const indexedDB::KeyPath&
  GetKeyPath() const
  {
    return mKeyPath;
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

  void
  GetStoreName(nsString& aStoreName);

  IDBObjectStoreSync*
  ObjectStore()
  {
    return mObjectStore;
  }

  JS::Value
  GetKeyPath(JSContext* aCx, ErrorResult& aRv);

  bool
  MultiEntry()
  {
    return mMultiEntry;
  }

  bool
  Unique()
  {
    return mUnique;
  }

  already_AddRefed<IDBCursorSync>
  OpenCursor(JSContext* aCx, const Optional<JS::Handle<JS::Value> >& aRange,
             IDBCursorDirection aDirection, ErrorResult& aRv);

  already_AddRefed<IDBCursorSync>
  OpenKeyCursor(JSContext* aCx, const Optional<JS::Handle<JS::Value> >& aRange,
                IDBCursorDirection aDirection, ErrorResult& aRv);

  JS::Value
  Get(JSContext* aCx, JS::Value aKey, ErrorResult& aRv);

  JS::Value
  GetKey(JSContext* aCx, JS::Value aKey, ErrorResult& aRv);

  JS::Value
  GetAll(JSContext* aCx, const Optional<JS::Handle<JS::Value> >& aKey,
            const Optional<uint32_t>& aLimit, ErrorResult& aRv);

  JS::Value
  GetAllKeys(JSContext* aCx, const Optional<JS::Handle<JS::Value> >& aKey,
                const Optional<uint32_t>& aLimit, ErrorResult& aRv);

  uint64_t
  Count(JSContext* aCx, const Optional<JS::Handle<JS::Value> >& aValue,
        ErrorResult& aRv);

private:
  IDBIndexSync(WorkerPrivate* aWorkerPrivate);
  ~IDBIndexSync();

  bool
  Init(JSContext* aCx, IndexInfo* aIndexInfo, bool aCreating);

  nsRefPtr<IDBObjectStoreSync> mObjectStore;

  bool mHoldingJSVal;
};

class IndexHelper : public BlockingHelperBase
{
public:
  typedef mozilla::dom::indexedDB::ipc::IndexRequestParams IndexRequestParams;

  IndexHelper(WorkerPrivate* aWorkerPrivate, IDBIndexSync* aIndex)
  : BlockingHelperBase(aWorkerPrivate, aIndex)
  { }

  IDBIndexSync*
  Index()
  {
    return static_cast<IDBIndexSync*>(mObject.get());
  }

  virtual nsresult
  SendConstructor(IndexedDBRequestWorkerChildBase** aActor) MOZ_OVERRIDE;

  virtual nsresult
  PackArguments(IndexRequestParams& aParams) = 0;

  virtual nsresult
  UnpackResponse(const ResponseValue& aResponseValue) = 0;
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_idbindexsync_h__
