/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBFactorySync.h"

#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/IDBFactoryBinding.h"
#include "mozilla/dom/IDBFactorySyncBinding.h"

#include "IDBDatabaseSync.h"
#include "IndexedDBSyncProxies.h"
#include "IPCThreadUtils.h"
#include "RuntimeService.h"
#include "WorkerPrivate.h"

#include "ipc/IndexedDBWorkerChild.h"
#include "ipc/WorkerChild.h"

USING_WORKERS_NAMESPACE
using namespace mozilla::dom;
using mozilla::dom::NonNull;
using mozilla::dom::Optional;
using mozilla::dom::quota::QuotaManager;
using mozilla::ErrorResult;

namespace {

class ConstructRunnable : public BlockWorkerThreadRunnable
{
public:
  ConstructRunnable(WorkerPrivate* aWorkerPrivate, uint32_t aSyncQueueKey,
                    IDBFactorySync* aFactory)
  : BlockWorkerThreadRunnable(aWorkerPrivate), mSyncQueueKey(aSyncQueueKey),
    mFactory(aFactory)
  { }

protected:
  nsresult
  IPCThreadRun()
  {
    const nsRefPtr<IDBFactorySyncProxy>& proxy = mFactory->Proxy();

    MOZ_ASSERT(!proxy->mWorkerPrivate, "Should be null!");
    proxy->mWorkerPrivate = mWorkerPrivate;

    MOZ_ASSERT(proxy->mSyncQueueKey, "Should be unset!");
    proxy->mSyncQueueKey = mSyncQueueKey;

    WorkerChild* workerActor =
      mWorkerPrivate->GetTopLevelWorker()->GetActorChild();
    MOZ_ASSERT(workerActor);

    IndexedDBWorkerChild* actor = new IndexedDBWorkerChild();
    workerActor->SendPIndexedDBConstructor(actor, mFactory->GetGroup(),
                                           mFactory->GetASCIIOrigin());
    actor->SetFactoryProxy(proxy);

    proxy->mExpectingResponse = true;

    return NS_OK;
  }

private:
  uint32_t mSyncQueueKey;
  nsRefPtr<IDBFactorySync> mFactory;
};

class SendConstructorRunnable : public BlockWorkerThreadRunnable
{
public:
  SendConstructorRunnable(WorkerPrivate* aWorkerPrivate, uint32_t aSyncQueueKey,
                          DeleteDatabaseHelper* aHelper)
  : BlockWorkerThreadRunnable(aWorkerPrivate), mSyncQueueKey(aSyncQueueKey),
    mHelper(aHelper)
  { }

protected:
  virtual nsresult
  IPCThreadRun() MOZ_OVERRIDE
  {
    const nsRefPtr<DeleteDatabaseProxy>& proxy = mHelper->Proxy();

    MOZ_ASSERT(!proxy->mWorkerPrivate, "Should be null!");
    proxy->mWorkerPrivate = mWorkerPrivate;

    MOZ_ASSERT(proxy->mSyncQueueKey == UINT32_MAX, "Should be unset!");
    proxy->mSyncQueueKey = mSyncQueueKey;

    IndexedDBDeleteDatabaseRequestWorkerChild* actor;
    nsresult rv = mHelper->SendConstructor(&actor);
    NS_ENSURE_SUCCESS(rv, rv);

    actor->SetHelperProxy(proxy);

    proxy->mExpectingResponse = true;

    return NS_OK;
  }

  uint32_t mSyncQueueKey;
  nsRefPtr<DeleteDatabaseHelper> mHelper;
};

} // anonymous namespace

NS_IMPL_ADDREF_INHERITED(IDBFactorySync, IDBObjectSync)
NS_IMPL_RELEASE_INHERITED(IDBFactorySync, IDBObjectSync)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(IDBFactorySync)
NS_INTERFACE_MAP_END_INHERITING(IDBObjectSync)

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBFactorySync)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IDBFactorySync,
                                                IDBObjectSync)
  tmp->ReleaseProxy(ObjectIsGoingAway);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IDBFactorySync,
                                                  IDBObjectSync)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

// static
already_AddRefed<IDBFactorySync>
IDBFactorySync::Create(JSContext* aCx, JSObject* aGlobal)
{
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);

  nsRefPtr<IDBFactorySync> factory = new IDBFactorySync(workerPrivate);

  factory->mGroup = workerPrivate->Group();
  factory->mASCIIOrigin = workerPrivate->Origin();
  factory->mDefaultPersistenceType = workerPrivate->DefaultPersistenceType();

  workerPrivate->InitIPCFromWorker(aCx);

  factory->mProxy = new IDBFactorySyncProxy(factory);

  factory->Pin();

  AutoUnpin autoUnpin(factory);
  AutoSyncLoopHolder syncLoop(workerPrivate);

  nsRefPtr<ConstructRunnable> runnable =
    new ConstructRunnable(workerPrivate, syncLoop.SyncQueueKey(),
                          factory);

  if (!runnable->Dispatch(aCx)) {
    factory->ReleaseProxy();
    return nullptr;
  }

  autoUnpin.Forget();

  if (!syncLoop.RunAndForget(aCx)) {
    return nullptr;
  }

  return factory.forget();
}

IDBFactorySync::IDBFactorySync(WorkerPrivate* aWorkerPrivate)
: IDBObjectSync(aWorkerPrivate)
{
  SetIsDOMBinding();
}

IDBFactorySync::~IDBFactorySync()
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  ReleaseProxy(ObjectIsGoingAway);

  MOZ_ASSERT(!mRooted);
}

IDBFactorySyncProxy*
IDBFactorySync::Proxy()
{
  return static_cast<IDBFactorySyncProxy*>(mProxy.get());
}

uint32_t
IDBFactorySync::OpenOrDeleteDatabaseSyncQueueKey() const
{
  if (mDatabase) {
    return mDatabase->Proxy()->mSyncQueueKey;
  }
  if (mDeleteDatabaseHelper) {
    return mDeleteDatabaseHelper->Proxy()->mSyncQueueKey;
  }
  return UINT32_MAX;
}

JSObject*
IDBFactorySync::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return IDBFactorySyncBinding_workers::Wrap(aCx, aScope, this);
}

already_AddRefed<IDBDatabaseSync>
IDBFactorySync::Open(JSContext* aCx, const nsAString& aName, uint64_t aVersion,
                     const Optional<OwningNonNull<IDBVersionChangeCallback> >& aUpgradeCallback,
                     const Optional<OwningNonNull<IDBVersionChangeBlockedCallback> >& aUpgradeBlockedCallback,
                     const Optional<uint32_t>& aTimeout, ErrorResult& aRv)
{
  IDBOpenDBOptions options;
  options.mVersion.Construct();
  options.mVersion.Value() = aVersion;

  return Open(aCx, aName, options, aUpgradeCallback, aUpgradeBlockedCallback, aTimeout, aRv);
}

already_AddRefed<IDBDatabaseSync>
IDBFactorySync::Open(JSContext* aCx, const nsAString& aName,
                     const IDBOpenDBOptions& aOptions,
                     const Optional<OwningNonNull<IDBVersionChangeCallback> >& aUpgradeCallback,
                     const Optional<OwningNonNull<IDBVersionChangeBlockedCallback> >& aUpgradeBlockedCallback,
                     const Optional<uint32_t>& aTimeout, ErrorResult& aRv)
{
  RuntimeService* rts = RuntimeService::GetService();
  MOZ_ASSERT(rts);

  if (!rts->IsIndexedDBSyncEnabled()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return nullptr;
  }

  uint64_t version = 0;
  if (aOptions.mVersion.WasPassed()) {
    if (aOptions.mVersion.Value() < 1) {
      aRv.ThrowTypeError(MSG_INVALID_VERSION);
      return nullptr;
    }
    version = aOptions.mVersion.Value();
  }

  quota::PersistenceType persistenceType =
    quota::PersistenceTypeFromStorage(aOptions.mStorage,
                                      mDefaultPersistenceType);

  IDBVersionChangeCallback* upgradeCallback =
    aUpgradeCallback.WasPassed() ? &aUpgradeCallback.Value() : nullptr;

  IDBVersionChangeBlockedCallback* upgradeBlockedCallback =
    aUpgradeBlockedCallback.WasPassed() ? &aUpgradeBlockedCallback.Value()
                                        : nullptr;

  mDatabase = IDBDatabaseSync::Create(aCx, this, aName, version,
                                      persistenceType, upgradeCallback,
                                      upgradeBlockedCallback);
  if (!mDatabase) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  if (!mDatabase->Open(aCx)) {
    aRv.Throw(NS_ERROR_FAILURE);
    mDatabase = nullptr;
    return nullptr;
  }

  nsRefPtr<IDBDatabaseSync> database;
  mDatabase.swap(database);
  return database.forget();
}

void
IDBFactorySync::DeleteDatabase(JSContext* aCx, const nsAString& aName,
                               const IDBOpenDBOptions& aOptions,
                               const Optional<OwningNonNull<IDBVersionChangeBlockedCallback> >& aDeleteBlockedCallback,
                               ErrorResult& aRv)
{
  RuntimeService* rts = RuntimeService::GetService();
  MOZ_ASSERT(rts);

  if (!rts->IsIndexedDBSyncEnabled()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return;
  }

  quota::PersistenceType persistenceType =
    quota::PersistenceTypeFromStorage(aOptions.mStorage,
                                      mDefaultPersistenceType);

  mDeleteDatabaseHelper =
    new DeleteDatabaseHelper(mWorkerPrivate, this, aName, persistenceType);

  if (!mDeleteDatabaseHelper->Run(aCx)) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    mDeleteDatabaseHelper = nullptr;
    return;
  }

  mDeleteDatabaseHelper = nullptr;
}

bool
DeleteDatabaseHelper::Run(JSContext* aCx)
{
  mProxy = new DeleteDatabaseProxy(this);

  Pin();

  AutoUnpin autoUnpin(this);
  AutoSyncLoopHolder syncLoop(mWorkerPrivate);

  nsRefPtr<SendConstructorRunnable> runnable =
    new SendConstructorRunnable(mWorkerPrivate, syncLoop.SyncQueueKey(), this);

  if (!runnable->Dispatch(aCx)) {
    ReleaseProxy();
    return false;
  }

  autoUnpin.Forget();

  if (!syncLoop.RunAndForget(aCx)) {
    return false;
  }

  return true;
}

nsresult
DeleteDatabaseHelper::SendConstructor(
                             IndexedDBDeleteDatabaseRequestWorkerChild** aActor)
{
  nsCString databaseId;
  QuotaManager::GetStorageId(mPersistenceType, mFactory->GetASCIIOrigin(),
                             mName, databaseId);

  IndexedDBDeleteDatabaseRequestWorkerChild* actor =
    new IndexedDBDeleteDatabaseRequestWorkerChild(databaseId);
  mFactory->Proxy()->Actor()->SendPIndexedDBDeleteDatabaseRequestConstructor(actor, nsString(mName), mPersistenceType);

  *aActor = actor;
  return NS_OK;
}
