/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_idbfactorysync_h__
#define mozilla_dom_workers_idbfactorysync_h__

#include "Workers.h"

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/indexedDB/IDBFactoryBase.h"
#include "mozilla/dom/quota/PersistenceType.h"

#include "IDBObjectSync.h"
#include "IPCThreadUtils.h"

namespace mozilla {
namespace dom {
struct IDBOpenDBOptions;
class IDBVersionChangeCallback;
} // namespace dom
} // namespace mozilla

BEGIN_WORKERS_NAMESPACE

class DeleteDatabaseHelper;
class IDBDatabaseSync;
class IDBFactorySync;
class IndexedDBDeleteDatabaseRequestWorkerChild;
class IndexedDBWorkerChild;

class IDBFactorySyncProxy : public IDBObjectSyncProxy<IndexedDBWorkerChild>
{
public:
  IDBFactorySyncProxy(IDBFactorySync* aFactory);
};

class IDBFactorySync MOZ_FINAL : public IDBObjectSync,
                                 public indexedDB::IDBFactoryBase
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(IDBFactorySync, IDBObjectSync);

  static already_AddRefed<IDBFactorySync>
  Create(JSContext* aCx, JSObject* aGlobal);

  IDBFactorySyncProxy*
  Proxy();

  uint32_t
  DeleteDatabaseSyncQueueKey() const;

  // WebIDL
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  nsISupports*
  GetParentObject() const
  {
    return nullptr;
  }

  already_AddRefed<IDBDatabaseSync>
  Open(JSContext* aCx, const nsAString& aName, uint64_t aVersion,
       const Optional<OwningNonNull<IDBVersionChangeCallback> >& aUpgradeCallback,
       const Optional<uint32_t>& aTimeout,
       ErrorResult& aRv);

  already_AddRefed<IDBDatabaseSync>
  Open(JSContext* aCx, const nsAString& aName,
       const IDBOpenDBOptions& aOptions,
       const Optional<OwningNonNull<IDBVersionChangeCallback> >& aUpgradeCallback,
       const Optional<uint32_t>& aTimeout, ErrorResult& aRv);

  void
  DeleteDatabase(JSContext* aCx, const nsAString& aName,
                 const IDBOpenDBOptions& aOptions, ErrorResult& aRv);

private:
  IDBFactorySync(WorkerPrivate* aWorkerPrivate);
  ~IDBFactorySync();

  nsRefPtr<DeleteDatabaseHelper> mDeleteDatabaseHelper;
};

class DeleteDatabaseProxy : public IDBObjectSyncProxyWithActor<IndexedDBDeleteDatabaseRequestWorkerChild>
{
public:
  DeleteDatabaseProxy(DeleteDatabaseHelper* aHelper);

  virtual void
  Teardown() MOZ_OVERRIDE;
};

class DeleteDatabaseHelper : public IDBObjectSyncBase
{
public:
  // This needs to be fully qualified to not confuse trace refcnt assertions.
  NS_INLINE_DECL_REFCOUNTING(mozilla::dom::workers::DeleteDatabaseHelper)

  DeleteDatabaseHelper(WorkerPrivate* aWorkerPrivate, IDBFactorySync* aFactory,
                       const nsAString& aName,
                       quota::PersistenceType aPersistenceType)
  : IDBObjectSyncBase(aWorkerPrivate), mFactory(aFactory), mName(aName),
    mPersistenceType(aPersistenceType)
  { }

  DeleteDatabaseProxy*
  Proxy() const
  {
    return static_cast<DeleteDatabaseProxy*>(mProxy.get());
  }

  bool
  Run(JSContext* aCx);

  nsresult
  SendConstructor(IndexedDBDeleteDatabaseRequestWorkerChild** aActor);

private:
  nsRefPtr<IDBFactorySync> mFactory;

  // In-params.
  nsString mName;
  quota::PersistenceType mPersistenceType;
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_idbfactorysync_h__
