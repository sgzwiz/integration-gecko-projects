/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBDatabaseSync.h"

#include "mozilla/dom/DOMStringList.h"
#include "mozilla/dom/IDBDatabaseSyncBinding.h"
#include "mozilla/dom/IDBTransactionCallbackBinding.h"
#include "mozilla/dom/IDBVersionChangeCallbackBinding.h"
#include "mozilla/dom/IDBVersionChangeBlockedCallbackBinding.h"
#include "mozilla/dom/indexedDB/IDBEvents.h"

#include "IDBFactorySync.h"
#include "IDBObjectStoreSync.h"
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
using mozilla::dom::indexedDB::IDBVersionChangeEvent;
using mozilla::dom::NonNull;
using mozilla::dom::Optional;
using mozilla::dom::Sequence;
using mozilla::ErrorResult;

namespace {

class OpenRunnable : public BlockWorkerThreadRunnable
{
public:
  OpenRunnable(WorkerPrivate* aWorkerPrivate, uint32_t aSyncQueueKey,
               IDBDatabaseSync* aDatabase)
  : BlockWorkerThreadRunnable(aWorkerPrivate), mSyncQueueKey(aSyncQueueKey),
    mDatabase(aDatabase)
  { }

protected:
  nsresult
  IPCThreadRun()
  {
    const nsRefPtr<IDBDatabaseSyncProxy>& proxy = mDatabase->Proxy();

    MOZ_ASSERT(!proxy->mWorkerPrivate, "Should be null!");
    proxy->mWorkerPrivate = mWorkerPrivate;

    MOZ_ASSERT(proxy->mSyncQueueKey == UINT32_MAX, "Should be unset!");
    proxy->mSyncQueueKey = mSyncQueueKey;

    IndexedDBDatabaseWorkerChild* dbActor =
      static_cast<IndexedDBDatabaseWorkerChild*>(
        mDatabase->Factory()->Proxy()->Actor()->SendPIndexedDBDatabaseConstructor(
                                                         mDatabase->Name(),
                                                         mDatabase->RequestedVersion(),
                                                         mDatabase->Type()));
    dbActor->SetDatabaseProxy(proxy);

    proxy->mExpectingResponse = true;

    return NS_OK;
  }

private:
  uint32_t mSyncQueueKey;
  nsRefPtr<IDBDatabaseSync> mDatabase;
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
    IndexedDBDatabaseWorkerChild* dbActor = mDatabase->Proxy()->Actor();
    dbActor->SendClose(false);

    return NS_OK;
  }

private:
  nsRefPtr<IDBDatabaseSync> mDatabase;
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
    IndexedDBTransactionWorkerChild* dbActor = mTransaction->Proxy()->Actor();
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
                        uint64_t aDatabaseSerial, uint64_t aOldVersion,
                        uint64_t aNewVersion)
  : WorkerSyncRunnable(aWorkerPrivate, aSyncQueueKey, false, SkipWhenClearing),
    mDatabaseSerial(aDatabaseSerial), mOldVersion(aOldVersion),
    mNewVersion(aNewVersion)
  {
    AssertIsOnIPCThread();
  }

  VersionChangeRunnable(WorkerPrivate* aWorkerPrivate,
                        uint64_t aDatabaseSerial, uint64_t aOldVersion,
                        uint64_t aNewVersion)
  : WorkerSyncRunnable(aWorkerPrivate, UINT32_MAX, true, SkipWhenClearing),
    mDatabaseSerial(aDatabaseSerial), mOldVersion(aOldVersion),
    mNewVersion(aNewVersion)
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
    nsRefPtr<IDBDatabaseSync> database =
      aWorkerPrivate->GetDatabase(mDatabaseSerial);
    if (!database) {
      return true;
    }

    nsRefPtr<nsDOMEvent> event =
      IDBVersionChangeEvent::Create(database, mOldVersion, mNewVersion);
    NS_ENSURE_TRUE(event, false);

    bool dummy;
    database->DispatchEvent(event, &dummy);

    return true;
  }

private:
  // We can't addref the native object from the IPC thread and a weak ref can't
  // be used since the database may or may not have been pinned.
  uint64_t mDatabaseSerial;

  uint64_t mOldVersion;
  uint64_t mNewVersion;
};

class UpgradeNeededRunnable : public WorkerSyncRunnable
{
public:
  UpgradeNeededRunnable(WorkerPrivate* aWorkerPrivate, uint32_t aSyncQueueKey,
                        IDBDatabaseSync* aDatabase)
  : WorkerSyncRunnable(aWorkerPrivate, aSyncQueueKey, false, SkipWhenClearing),
    mDatabase(aDatabase)
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
    mDatabase->DoUpgrade(aCx);

    return true;
  }

private:
  // A weak ref (we can't addref the native object from the IPC thread).
  // This shouldn't be a problem because the database has been pinned.
  IDBDatabaseSync* mDatabase;
};

class UpgradeBlockedRunnable : public WorkerSyncRunnable
{
public:
  UpgradeBlockedRunnable(WorkerPrivate* aWorkerPrivate, uint32_t aSyncQueueKey,
                         IDBDatabaseSync* aDatabase, uint64_t aOldVersion)
  : WorkerSyncRunnable(aWorkerPrivate, aSyncQueueKey, false, SkipWhenClearing),
    mDatabase(aDatabase), mOldVersion(aOldVersion)
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
    mDatabase->DoUpgradeBlocked(aCx, mOldVersion);

    return true;
  }

private:
  // A weak ref (we can't addref the native object from the IPC thread).
  // This shouldn't be a problem because the database has been pinned.
  IDBDatabaseSync* mDatabase;

  uint64_t mOldVersion;
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

IDBDatabaseSyncProxy::IDBDatabaseSyncProxy(IDBDatabaseSync* aDatabase)
: IDBObjectSyncProxy<IndexedDBDatabaseWorkerChild>(aDatabase)
{
}

IDBDatabaseSync*
IDBDatabaseSyncProxy::Database()
{
  return static_cast<IDBDatabaseSync*>(mObject);
}

NS_IMPL_ADDREF_INHERITED(IDBDatabaseSync, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(IDBDatabaseSync, nsDOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(IDBDatabaseSync)
NS_INTERFACE_MAP_END_INHERITING(IDBObjectSyncEventTarget)

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBDatabaseSync)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IDBDatabaseSync,
                                                IDBObjectSyncEventTarget)
  tmp->ReleaseProxy(ObjectIsGoingAway);
//  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFactory)

  tmp->mWorkerPrivate->UnregisterDatabase(tmp);
  tmp->mRegistered = false;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IDBDatabaseSync,
                                                  IDBObjectSyncEventTarget)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFactory)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(IDBDatabaseSync,
                                               IDBObjectSyncEventTarget)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

// static
already_AddRefed<IDBDatabaseSync>
IDBDatabaseSync::Create(JSContext* aCx, IDBFactorySync* aFactory,
                        const nsAString& aName, uint64_t aVersion,
                        PersistenceType aPersistenceType,
                        IDBVersionChangeCallback* aUpgradeCallback,
                        IDBVersionChangeBlockedCallback* aUpgradeBlockedCallback)
{
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);

  nsRefPtr<IDBDatabaseSync> database = new IDBDatabaseSync(workerPrivate);

  database->mFactory = aFactory;
  database->mName = aName;
  database->mVersion = aVersion;
  database->mPersistenceType = aPersistenceType;
  database->mUpgradeCallback = aUpgradeCallback;
  database->mUpgradeBlockedCallback = aUpgradeBlockedCallback;

  workerPrivate->RegisterDatabase(database);
  database->mRegistered = true;

  return database.forget();
}

IDBDatabaseSync::IDBDatabaseSync(WorkerPrivate* aWorkerPrivate)
: IDBObjectSyncEventTarget(aWorkerPrivate), mSerial(0), mVersion(0),
  mUpgradeCallback(nullptr), mUpgradeBlockedCallback(nullptr),
  mUpgradeCode(NS_OK), mRegistered(false)
{
  SetIsDOMBinding();
}

IDBDatabaseSync::~IDBDatabaseSync()
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  ReleaseProxy(ObjectIsGoingAway);

  MOZ_ASSERT(!mRooted);

  if (mRegistered) {
    mWorkerPrivate->UnregisterDatabase(this);
  }
}

IDBDatabaseSyncProxy*
IDBDatabaseSync::Proxy()
{
  return static_cast<IDBDatabaseSyncProxy*>(mProxy.get());
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
IDBDatabaseSync::DoUpgrade(JSContext* aCx)
{
  MOZ_ASSERT(mPreviousDatabaseInfo);

  if (mUpgradeCallback) {
    ErrorResult rv;
    mUpgradeCallback->Call(mFactory, *mTransaction,
                           mPreviousDatabaseInfo->version, rv);
    if (rv.Failed()) {
      mTransaction->Abort(aCx, rv);
      mUpgradeCode = mTransaction->GetAbortCode();
    }
  }

  if (!mTransaction->Finish(aCx)) {
    mUpgradeCode = NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }
}

void
IDBDatabaseSync::DoUpgradeBlocked(JSContext* aCx, uint64_t aOldVersion)
{
  if (mUpgradeBlockedCallback) {
    ErrorResult rv;
    mUpgradeBlockedCallback->Call(mFactory, aOldVersion, rv);
  }
}

void
IDBDatabaseSync::OnVersionChange(uint64_t aOldVersion, uint64_t aNewVersion)
{
  AssertIsOnIPCThread();

  nsRefPtr<VersionChangeRunnable> runnable;

  uint32_t syncQueueKey = mFactory->OpenOrDeleteDatabaseSyncQueueKey();
  if (syncQueueKey == UINT32_MAX) {
    runnable = new VersionChangeRunnable(mWorkerPrivate, mSerial, aOldVersion,
                                         aNewVersion);
  }
  else {
    runnable = new VersionChangeRunnable(mWorkerPrivate, syncQueueKey, mSerial,
                                         aOldVersion, aNewVersion);
  }

  runnable->Dispatch(nullptr);
}

void
IDBDatabaseSync::OnBlocked(uint64_t aOldVersion)
{
  AssertIsOnIPCThread();

  nsRefPtr<UpgradeBlockedRunnable> runnable =
    new UpgradeBlockedRunnable(mWorkerPrivate, mProxy->mSyncQueueKey, this,
                               aOldVersion);

  runnable->Dispatch(nullptr);
}

void
IDBDatabaseSync::OnUpgradeNeeded()
{
  AssertIsOnIPCThread();

  nsRefPtr<UpgradeNeededRunnable> runnable =
    new UpgradeNeededRunnable(mWorkerPrivate, mProxy->mSyncQueueKey, this);

  runnable->Dispatch(nullptr);
}

JSObject*
IDBDatabaseSync::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return IDBDatabaseSyncBinding_workers::Wrap(aCx, aScope, this);
}

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

already_AddRefed<IDBObjectStoreSync>
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

  nsRefPtr<IDBObjectStoreSync> objectStore =
    mTransaction->GetOrCreateObjectStore(aCx, newInfo, true);

  if (!objectStore) {
    NS_WARNING("objectStore is null!");
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return nullptr;
  }

  autoRemove.forget();
  return objectStore.forget();
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

  nsRefPtr<IDBTransactionSync> trans =
    IDBTransactionSync::Create(aCx, this, aStoreNames, transactionMode);
  if (!trans) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return;
  }

  if (!trans->Init(aCx)) {;
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return;
  }

  ErrorResult rv;
  aCallback.Call(this, *trans, rv);
  if (rv.Failed()) {
    trans->Abort(aCx, rv);
    aRv = trans->GetAbortCode();
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
      Proxy()->Actor()->SendClose(false);
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
IDBDatabaseSync::Open(JSContext* aCx)
{
  mTransaction = IDBTransactionSync::Create(aCx, this, Sequence<nsString>(),
                                            IDBTransactionBase::VERSION_CHANGE);
  if (!mTransaction) {
    return false;
  }

  mTransaction->mProxy = new IDBTransactionSyncProxy(mTransaction);

  mProxy = new IDBDatabaseSyncProxy(this);

  Pin();

  AutoUnpin autoUnpin(this);
  AutoSyncLoopHolder syncLoop(mWorkerPrivate);

  nsRefPtr<OpenRunnable> runnable =
    new OpenRunnable(mWorkerPrivate, syncLoop.SyncQueueKey(), this);

  if (!runnable->Dispatch(aCx)) {
    ReleaseProxy();
    return false;
  }

  autoUnpin.Forget();

  if (!syncLoop.RunAndForget(aCx)) {
    mTransaction->mInvalid = true;
    mTransaction = nullptr;

    return false;
  }

  mTransaction->mInvalid = true;
  mTransaction = nullptr;

  return NS_SUCCEEDED(mUpgradeCode);
}
