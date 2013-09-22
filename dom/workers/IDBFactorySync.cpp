/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBFactorySync.h"

#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/IDBFactoryBinding.h"

#include "DOMBindingInlines.h"
#include "IDBDatabaseSync.h"
#include "IPCThreadUtils.h"
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
    NS_ASSERTION(mFactory->PrimarySyncQueueKey() == UINT32_MAX,
                 "Primary sync queue key should be unset!");
    mFactory->PrimarySyncQueueKey() = mSyncQueueKey;

    IndexedDBWorkerChild* actor = new IndexedDBWorkerChild();
    mWorkerPrivate->GetWorkerChild()->SendPIndexedDBConstructor(
                                                    actor,
                                                    mFactory->GetGroup(),
                                                    mFactory->GetASCIIOrigin());
    actor->SetFactory(mFactory);

    return NS_OK;
  }

private:
  uint32_t mSyncQueueKey;
  IDBFactorySync* mFactory;
};

} // anonymous namespace

// static
IDBFactorySync*
IDBFactorySync::Create(JSContext* aCx, JSObject* aGlobal)
{
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);

  nsRefPtr<IDBFactorySync> factory = new IDBFactorySync(aCx, workerPrivate);

  factory->mGroup = workerPrivate->Group();
  factory->mASCIIOrigin = workerPrivate->Origin();
  factory->mDefaultPersistenceType = workerPrivate->DefaultPersistenceType();

  if (!Wrap(aCx, aGlobal, factory)) {
    return nullptr;
  }

  AutoSyncLoopHolder syncLoop(workerPrivate);

  nsRefPtr<ConstructRunnable> runnable =
    new ConstructRunnable(workerPrivate, syncLoop.SyncQueueKey(),
                          factory);

  if (!runnable->Dispatch(aCx) || !syncLoop.RunAndForget(aCx)) {
    return nullptr;
  }

  return factory;
}

IDBFactorySync::IDBFactorySync(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
: IDBObjectSync(aCx, aWorkerPrivate), mDeleteDatabaseSyncQueueKey(UINT32_MAX),
  mActorChild(nullptr)
{ }

IDBFactorySync::~IDBFactorySync()
{
  NS_ASSERTION(!mActorChild, "Still have an actor object attached!");
}

void
IDBFactorySync::_trace(JSTracer* aTrc)
{
  IDBObjectSync::_trace(aTrc);
}

void
IDBFactorySync::_finalize(JSFreeOp* aFop)
{
  IDBObjectSync::_finalize(aFop);
}

void
IDBFactorySync::ReleaseIPCThreadObjects()
{
  AssertIsOnIPCThread();

  if (mActorChild) {
    mActorChild->Send__delete__(mActorChild);
    NS_ASSERTION(!mActorChild, "Should have cleared in Send__delete__!");
  }
}

NS_IMPL_ISUPPORTS_INHERITED0(IDBFactorySync, IDBObjectSync)

IDBDatabaseSync*
IDBFactorySync::Open(JSContext* aCx, const nsAString& aName, uint64_t aVersion,
                     const Optional<OwningNonNull<IDBVersionChangeCallbackWorkers> >& aUpgradeCallback,
                     const Optional<uint32_t>& aTimeout, ErrorResult& aRv)
{
  IDBOpenDBOptions options;
  options.mVersion.Construct();
  options.mVersion.Value() = aVersion;

  return Open(aCx, aName, options, aUpgradeCallback, aTimeout, aRv);
}

IDBDatabaseSync*
IDBFactorySync::Open(JSContext* aCx, const nsAString& aName,
                     const IDBOpenDBOptions& aOptions,
                     const Optional<OwningNonNull<IDBVersionChangeCallbackWorkers> >& aUpgradeCallback,
                     const Optional<uint32_t>& aTimeout, ErrorResult& aRv)
{
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

  IDBDatabaseSync* database =
    IDBDatabaseSync::Create(aCx, this, aName, version, persistenceType);
  if (!database) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  IDBVersionChangeCallbackWorkers* upgradeCallback =
    aUpgradeCallback.WasPassed() ? &aUpgradeCallback.Value() : nullptr;

  if (!database->Open(aCx, upgradeCallback)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return database;
}

void
IDBFactorySync::DeleteDatabase(JSContext* aCx, const nsAString& aName,
                               const IDBOpenDBOptions& aOptions,
                               ErrorResult& aRv)
{
  DOMBindingAnchor<IDBFactorySync> selfAnchor(this);

  AutoSyncLoopHolder syncLoop(mWorkerPrivate);

  quota::PersistenceType persistenceType =
    quota::PersistenceTypeFromStorage(aOptions.mStorage,
                                      mDefaultPersistenceType);

  nsRefPtr<DeleteDatabaseHelper> helper =
    new DeleteDatabaseHelper(mWorkerPrivate, syncLoop.SyncQueueKey(), this,
                             aName, persistenceType);

  if (!helper->Dispatch(aCx) || !syncLoop.RunAndForget(aCx)) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return;
  }
}

void
DeleteDatabaseHelper::OnRequestComplete(nsresult aRv)
{
  nsRefPtr<UnblockWorkerThreadRunnable> runnable =
    new UnblockWorkerThreadRunnable(mWorkerPrivate,
                                    mFactory->DeleteDatabaseSyncQueueKey(),
                                    aRv);

  if (!runnable->Dispatch()) {
    NS_WARNING("Failed to dispatch runnable!");
  }

  mFactory->DeleteDatabaseSyncQueueKey() = UINT32_MAX;
}

nsresult
DeleteDatabaseHelper::IPCThreadRun()
{
  NS_ASSERTION(mFactory->DeleteDatabaseSyncQueueKey() == UINT32_MAX,
               "Delete database sync queue key should be unset!");
  mFactory->DeleteDatabaseSyncQueueKey() = mSyncQueueKey;

  nsCString databaseId;
  QuotaManager::GetStorageId(mPersistenceType, mFactory->GetASCIIOrigin(),
                             mName, databaseId);

  IndexedDBDeleteDatabaseRequestWorkerChild* actor =
    new IndexedDBDeleteDatabaseRequestWorkerChild(databaseId);

  mFactory->GetActor()->SendPIndexedDBDeleteDatabaseRequestConstructor(
                                                              actor,
                                                              nsString(mName),
                                                              mPersistenceType);

  actor->SetHelper(this);

  return NS_OK;
}
