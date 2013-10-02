/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBDatabaseSync.h"

#include "mozilla/dom/DOMStringList.h"
#include "mozilla/dom/IDBDatabaseBinding.h"
#include "mozilla/dom/IDBTransactionCallbackBinding.h"
#include "mozilla/dom/IDBVersionChangeCallbackBinding.h"

#include "DOMBindingInlines.h"
#include "Events.h"
#include "IDBFactorySync.h"
#include "IDBTransactionSync.h"
#include "IPCThreadUtils.h"
#include "WorkerPrivate.h"

#include "ipc/IndexedDBWorkerChild.h"

USING_WORKERS_NAMESPACE
using mozilla::dom::DOMStringList;
using mozilla::dom::IDBTransactionCallback;
using mozilla::dom::IDBTransactionMode;
using mozilla::dom::IDBVersionChangeCallback;
using mozilla::dom::indexedDB::IDBTransactionBase;
using mozilla::dom::NonNull;
using mozilla::dom::Optional;
using mozilla::dom::Sequence;
using mozilla::ErrorResult;

namespace {

class OpenRunnable : public BlockWorkerThreadRunnable
{
public:
  OpenRunnable(WorkerPrivate* aWorkerPrivate, uint32_t aSyncQueueKey,
               IDBFactorySync* aFactory, IDBDatabaseSync* aDatabase)
  : BlockWorkerThreadRunnable(aWorkerPrivate), mSyncQueueKey(aSyncQueueKey),
    mFactory(aFactory), mDatabase(aDatabase)
  { }

protected:
  nsresult
  IPCThreadRun()
  {
    MOZ_ASSERT(mDatabase->PrimarySyncQueueKey() == UINT32_MAX,
               "Primary sync queue key should be unset!");
    mDatabase->PrimarySyncQueueKey() = mSyncQueueKey;

    IndexedDBDatabaseWorkerChild* dbActor =
      static_cast<IndexedDBDatabaseWorkerChild*>(
        mFactory->GetActor()->SendPIndexedDBDatabaseConstructor(
                                                         mDatabase->Name(),
                                                         mDatabase->RequestedVersion(),
                                                         mDatabase->Type()));
    dbActor->SetDatabase(mDatabase);

    return NS_OK;
  }

private:
  uint32_t mSyncQueueKey;
  IDBFactorySync* mFactory;
  IDBDatabaseSync* mDatabase;
};

class CloseRunnable : public BlockWorkerThreadRunnable
{
public:
  CloseRunnable(WorkerPrivate* aWorkerPrivate, IDBDatabaseSync* aDatabase)
  : BlockWorkerThreadRunnable(aWorkerPrivate), mDatabase(aDatabase)
  { }

protected:
  nsresult
  IPCThreadRun()
  {
    IndexedDBDatabaseWorkerChild* dbActor = mDatabase->GetActor();
    dbActor->SendClose(false);

    return NS_OK;
  }

private:
  IDBDatabaseSync* mDatabase;
};

class DeleteObjectStoreRunnable : public BlockWorkerThreadRunnable
{
public:
  DeleteObjectStoreRunnable(WorkerPrivate* aWorkerPrivate,
                            IDBTransactionSync* aTransaction,
                            nsString aObjectStoreName)
  : BlockWorkerThreadRunnable(aWorkerPrivate), mTransaction(aTransaction),
    mObjectStoreName(aObjectStoreName)
  { }

protected:
  nsresult
  IPCThreadRun()
  {
    IndexedDBTransactionWorkerChild* dbActor = mTransaction->GetActor();
    dbActor->SendDeleteObjectStore(mObjectStoreName);

    return NS_OK;
  }

private:
  IDBTransactionSync* mTransaction;
  nsString mObjectStoreName;
};

class VersionChangeRunnable : public WorkerSyncRunnable
{
public:
  VersionChangeRunnable(WorkerPrivate* aWorkerPrivate, uint32_t aSyncQueueKey,
                        IDBDatabaseSync* aDatabase, uint64_t aOldVersion,
                        uint64_t aNewVersion)
  : WorkerSyncRunnable(aWorkerPrivate, aSyncQueueKey, false, SkipWhenClearing),
    mDatabase(aDatabase), mOldVersion(aOldVersion), mNewVersion(aNewVersion)
  {
    AssertIsOnIPCThread();
  }

  bool
  PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    AssertIsOnIPCThread();
    return true;
  }

  void
  PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
               bool aDispatchResult)
  {
    AssertIsOnIPCThread();
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    JS::Rooted<JSObject*> event(aCx,
               events::CreateVersionChangeEvent(aCx, mOldVersion, mNewVersion));

    if (!event) {
      return false;
    }

    JS::Rooted<JSObject*> target(aCx, mDatabase->GetJSObject());
    MOZ_ASSERT(target);

    bool dummy;
    if (!events::DispatchEventToTarget(aCx, target, event, &dummy)) {
      JS_ReportPendingException(aCx);
    }

    return true;
  }

private:
  IDBDatabaseSync* mDatabase;
  uint64_t mOldVersion;
  uint64_t mNewVersion;
};

MOZ_STACK_CLASS
class AutoRemoveObjectStore
{
public:
  AutoRemoveObjectStore(DatabaseInfoMT* aInfo, const nsAString& aName)
  : mInfo(aInfo), mName(aName)
  { }

  ~AutoRemoveObjectStore()
  {
    if (mInfo) {
      mInfo->RemoveObjectStore(mName);
    }
  }

  void forget()
  {
    mInfo = nullptr;
  }

private:
  DatabaseInfoMT* mInfo;
  nsString mName;
};

} // anonymous namespace

// static
IDBDatabaseSync*
IDBDatabaseSync::Create(JSContext* aCx, IDBFactorySync* aFactory,
                        const nsAString& aName, uint64_t aVersion,
                        PersistenceType aPersistenceType)
{
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);

  nsRefPtr<IDBDatabaseSync> database = new IDBDatabaseSync(aCx, workerPrivate);

  database->mFactory = aFactory;
  database->mName = aName;
  database->mVersion = aVersion;
  database->mPersistenceType = aPersistenceType;

  if (!Wrap(aCx, nullptr, database)) {
    return nullptr;
  }

  return database;
}

IDBDatabaseSync::IDBDatabaseSync(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
: IDBObjectSyncEventTarget(aCx, aWorkerPrivate), mFactory(nullptr), mVersion(0),
  mTransaction(nullptr), mUpgradeNeeded(false), mActorChild(nullptr)
{ }

IDBDatabaseSync::~IDBDatabaseSync()
{
  MOZ_ASSERT(!mActorChild, "Still have an actor object attached!");
}

void
IDBDatabaseSync::EnterSetVersionTransaction()
{
  MOZ_ASSERT(!mRunningVersionChange, "How did that happen?");

  mPreviousDatabaseInfo = mDatabaseInfo->Clone();

  mRunningVersionChange = true;
}

void
IDBDatabaseSync::ExitSetVersionTransaction()
{
  MOZ_ASSERT(mRunningVersionChange, "How did that happen?");

  mPreviousDatabaseInfo = nullptr;

  mRunningVersionChange = false;
}

void
IDBDatabaseSync::RevertToPreviousState()
{
  mDatabaseInfo = mPreviousDatabaseInfo;
  mPreviousDatabaseInfo = nullptr;
}

void
IDBDatabaseSync::_trace(JSTracer* aTrc)
{
  IDBObjectSyncEventTarget::_trace(aTrc);
}

void
IDBDatabaseSync::_finalize(JSFreeOp* aFop)
{
  IDBObjectSyncEventTarget::_finalize(aFop);
}

void
IDBDatabaseSync::ReleaseIPCThreadObjects()
{
  AssertIsOnIPCThread();

  if (mActorChild) {
    mActorChild->Send__delete__(mActorChild);
    MOZ_ASSERT(!mActorChild, "Should have cleared in Send__delete__!");
  }
}

void
IDBDatabaseSync::OnVersionChange(uint64_t aOldVersion, uint64_t aNewVersion)
{
  AssertIsOnIPCThread();

  nsRefPtr<VersionChangeRunnable> runnable =
    new VersionChangeRunnable(mWorkerPrivate,
                              mFactory->DeleteDatabaseSyncQueueKey(), this,
                              aOldVersion, aNewVersion);

  runnable->Dispatch(nullptr);
}

NS_IMPL_ISUPPORTS_INHERITED0(IDBDatabaseSync, IDBObjectSyncEventTarget)

already_AddRefed<DOMStringList>
IDBDatabaseSync::GetObjectStoreNames(JSContext* aCx, ErrorResult& aRv)
{
  nsRefPtr<DOMStringList> list(new DOMStringList());
  if(mDatabaseInfo && !mDatabaseInfo->GetObjectStoreNames(list->Names())) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return nullptr;
  }

  return list.forget();
}

IDBObjectStoreSync*
IDBDatabaseSync::CreateObjectStore(
                     JSContext* aCx,
                     const nsAString& aName,
                     const IDBObjectStoreParameters& aOptionalParameters,
                     ErrorResult& aRv)
{
  if (!mTransaction ||
       mTransaction->GetMode() != IDBTransactionBase::VERSION_CHANGE) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return nullptr;
  }

  indexedDB::KeyPath keyPath(0);

  if (NS_FAILED(indexedDB::KeyPath::Parse(aCx, aOptionalParameters.mKeyPath, &keyPath))) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return nullptr;
  }

  MOZ_ASSERT(mDatabaseInfo, "Null databaseInfo!");

  if (mDatabaseInfo->ContainsStoreName(aName)) {
    NS_WARNING("StoreName already exist!");
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_CONSTRAINT_ERR);
    return nullptr;
  }

  if (!keyPath.IsAllowedForObjectStore(aOptionalParameters.mAutoIncrement)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return nullptr;
  }

  nsRefPtr<ObjectStoreInfo> newInfo = new ObjectStoreInfo();
  newInfo->name = aName;
  newInfo->id = mDatabaseInfo->nextObjectStoreId++;
  newInfo->keyPath = keyPath;
  newInfo->autoIncrement = aOptionalParameters.mAutoIncrement;
  newInfo->nextAutoIncrementId = newInfo->autoIncrement ? 1 : 0;
  newInfo->comittedAutoIncrementId = newInfo->nextAutoIncrementId;

  if (!mDatabaseInfo->PutObjectStore(newInfo)) {
    NS_WARNING("Put failed!");
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return nullptr;
  }

  // Don't leave this in the hash if we fail below!
  AutoRemoveObjectStore autoRemove(mDatabaseInfo, newInfo->name);

  IDBObjectStoreSync* objectStore =
    mTransaction->GetOrCreateObjectStore(aCx, newInfo, true);

  if (!objectStore) {
    NS_WARNING("objectStore is null!");
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return nullptr;
  }

  autoRemove.forget();
  return objectStore;
}

void
IDBDatabaseSync::DeleteObjectStore(JSContext* aCx,
                                   const nsAString& aName,
                                   ErrorResult& aRv)
{
  if (!mTransaction ||
       mTransaction->GetMode() != IDBTransactionBase::VERSION_CHANGE) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return;
  }

  DatabaseInfoMT* info = mTransaction->DBInfo();
  ObjectStoreInfo* objectStoreInfo = info->GetObjectStore(aName);
  if (!objectStoreInfo) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_FOUND_ERR);
    return;
  }

  nsRefPtr<DeleteObjectStoreRunnable> runnable =
    new DeleteObjectStoreRunnable(mTransaction->mWorkerPrivate, mTransaction,
                                  nsString(aName));

  if (!runnable->Dispatch(aCx)) {
    NS_WARNING("Runnable did not Dispatch!");
    return;
  }

  mTransaction->RemoveObjectStore(aName);

}

void
IDBDatabaseSync::Transaction(JSContext* aCx,
                             const Sequence<nsString>& aStoreNames,
                             IDBTransactionCallback& aCallback,
                             IDBTransactionMode aMode,
                             const Optional<uint32_t>& aTimeout,
                             ErrorResult& aRv)
{
  if (mClosed) {
    NS_WARNING("Database was closed!");
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return;
  }

  if (mRunningVersionChange) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return;
  }

  if (aStoreNames.IsEmpty()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return;
  }

  // XXX move to the base class ?
  IDBTransactionBase::Mode transactionMode = IDBTransactionBase::READ_ONLY;
  switch (aMode) {
    case IDBTransactionMode::Readonly:
      transactionMode = IDBTransactionBase::READ_ONLY;
      break;
    case IDBTransactionMode::Readwrite:
      transactionMode = IDBTransactionBase::READ_WRITE;
      break;
    case IDBTransactionMode::Versionchange:
      transactionMode = IDBTransactionBase::VERSION_CHANGE;
      break;
    default:
      MOZ_CRASH("Unknown mode!");
  }

  // TODO: Timeout

  IDBTransactionSync* trans =
    IDBTransactionSync::Create(aCx, this, aStoreNames, transactionMode);
  if (!trans) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return;
  }

  if (!trans->Init(aCx)) {;
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return;
  }

  aCallback.Call(this, *trans, aRv);
  if (aRv.Failed()) {
    ErrorResult rv;
    trans->Abort(aCx, rv);
    return;
  }

  if (!trans->Finish(aCx)) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return;
  }

  trans->mInvalid = true;
}

void
IDBDatabaseSync::Close(JSContext* aCx, ErrorResult& aRv)
{
  if (!mClosed) {
    mClosed = true;

    {
      nsRefPtr<DatabaseInfoMT> previousInfo;
      mDatabaseInfo.swap(previousInfo);

      nsRefPtr<DatabaseInfoMT> clonedInfo = previousInfo->Clone();
      clonedInfo.swap(mDatabaseInfo);
    }

    if (IsOnIPCThread()) {
      mActorChild->SendClose(false);
    }
    else {
      nsRefPtr<CloseRunnable> runnable =
        new CloseRunnable(mWorkerPrivate, this);

      if (!runnable->Dispatch(aCx)) {
        NS_WARNING("Runnable did not Dispatch!");
        aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
        return;
      }
    }
  }
}

bool
IDBDatabaseSync::Open(JSContext* aCx,
                      IDBVersionChangeCallback* aUpgradeCallback)
{
  Sequence<nsString> storeNames;

  mTransaction = IDBTransactionSync::Create(aCx, this, storeNames,
                                            IDBTransactionBase::VERSION_CHANGE);
  if (!mTransaction) {
    return false;
  }

  DOMBindingAnchor<IDBDatabaseSync> selfAnchor(this);
  DOMBindingAnchor<IDBFactorySync> factoryAnchor(mFactory);

  AutoSyncLoopHolder syncLoop(mWorkerPrivate);

  nsRefPtr<OpenRunnable> runnable =
    new OpenRunnable(mWorkerPrivate, syncLoop.SyncQueueKey(), mFactory, this);

  if (!runnable->Dispatch(aCx) || !syncLoop.RunAndForget(aCx)) {
    return false;
  }

  if (mUpgradeNeeded) {
    if (aUpgradeCallback) {
      ErrorResult rv;
      aUpgradeCallback->Call(mFactory, *mTransaction, 99, rv);
      if (rv.Failed()) {
        return false;
      }
    }

    if (!mTransaction->Finish(aCx)) {
      return false;
    }
  }

  mTransaction->mInvalid = true;
  mTransaction = nullptr;

  return true;
}
