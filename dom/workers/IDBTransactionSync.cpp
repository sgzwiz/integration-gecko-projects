/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBTransactionSync.h"

#include "mozilla/dom/DOMStringList.h"
#include "mozilla/dom/indexedDB/DatabaseInfo.h"

#include "DatabaseInfoMT.h"
#include "DOMBindingInlines.h"
#include "IDBDatabaseSync.h"
#include "IDBObjectStoreSync.h"
#include "IPCThreadUtils.h"
#include "WorkerPrivate.h"

#include "ipc/IndexedDBWorkerChild.h"

USING_WORKERS_NAMESPACE
using namespace mozilla::dom::indexedDB;
using namespace mozilla::dom::indexedDB::ipc;
using mozilla::dom::DOMStringList;
using mozilla::dom::NonNull;
using mozilla::dom::Sequence;
using mozilla::ErrorResult;

namespace {

class InitRunnable : public BlockWorkerThreadRunnable
{
public:
  InitRunnable(WorkerPrivate* aWorkerPrivate, IDBTransactionSync* aTransaction)
  : BlockWorkerThreadRunnable(aWorkerPrivate), mTransaction(aTransaction)
  { }

protected:
  nsresult
  IPCThreadRun()
  {
    NormalTransactionParams params;
    params.names().AppendElements(mTransaction->GetObjectStoreNames());
    params.mode() = mTransaction->GetMode();

    IndexedDBTransactionWorkerChild* actor =
      new IndexedDBTransactionWorkerChild();
    mTransaction->Db()->GetActor()->SendPIndexedDBTransactionConstructor(
                                                                        actor,
                                                                        params);
    actor->SetTransaction(mTransaction);

    return NS_OK;
  }

private:
  IDBTransactionSync* mTransaction;
};

class AbortRunnable : public BlockWorkerThreadRunnable
{
public:
  AbortRunnable(WorkerPrivate* aWorkerPrivate, IDBTransactionSync* aTransaction)
  : BlockWorkerThreadRunnable(aWorkerPrivate), mTransaction(aTransaction)
  { }

protected:
  nsresult
  IPCThreadRun()
  {
    mTransaction->GetActor()->SendAbort(mTransaction->GetAbortCode());

    return NS_OK;
  }

private:
  IDBTransactionSync* mTransaction;
};

class FinishRunnable : public BlockWorkerThreadRunnable
{
public:
  FinishRunnable(WorkerPrivate* aWorkerPrivate, uint32_t aSyncQueueKey,
                 IDBTransactionSync* aTransaction)
  : BlockWorkerThreadRunnable(aWorkerPrivate), mSyncQueueKey(aSyncQueueKey),
    mTransaction(aTransaction)
  { }

protected:
  nsresult
  IPCThreadRun()
  {
    if (mTransaction->GetMode() == IDBTransactionBase::VERSION_CHANGE) {
      MOZ_ASSERT(mTransaction->Db()->PrimarySyncQueueKey() == UINT32_MAX,
                 "Should be unset!");
      mTransaction->Db()->PrimarySyncQueueKey() = mSyncQueueKey;
    }
    else {
      MOZ_ASSERT(mTransaction->PrimarySyncQueueKey() == UINT32_MAX,
                 "Should be unset!");
      mTransaction->PrimarySyncQueueKey() = mSyncQueueKey;
    }

    mTransaction->GetActor()->SendAllRequestsFinished();

    return NS_OK;
  }

private:
  uint32_t mSyncQueueKey;
  IDBTransactionSync* mTransaction;
};

} // anonymous namespace

// static
IDBTransactionSync*
IDBTransactionSync::Create(JSContext* aCx, IDBDatabaseSync* aDatabase,
                           const Sequence<nsString>& aObjectStoreNames,
                           Mode aMode)
{
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);

  nsRefPtr<IDBTransactionSync> trans =
    new IDBTransactionSync(aCx, workerPrivate);

  trans->mDatabase = aDatabase;
  trans->mDatabaseInfo = aDatabase->Info();
  trans->mMode = aMode;
  trans->mObjectStoreNames.AppendElements(aObjectStoreNames);
  trans->mObjectStoreNames.Sort();

  if (!Wrap(aCx, nullptr, trans)) {
    return nullptr;
  }

  return trans;
}

IDBTransactionSync::IDBTransactionSync(JSContext* aCx,
                                       WorkerPrivate* aWorkerPrivate)
: IDBObjectSync(aCx, aWorkerPrivate), mDatabase(nullptr),
  mActorChild(nullptr), mInvalid(false)
{ }

IDBTransactionSync::~IDBTransactionSync()
{
  MOZ_ASSERT(!mActorChild, "Still have an actor object attached!");
}

void
IDBTransactionSync::RemoveObjectStore(const nsAString& aName)
{
  MOZ_ASSERT(mMode == IDBTransaction::VERSION_CHANGE,
             "Only remove object stores on VERSION_CHANGE transactions");

  mDatabaseInfo->RemoveObjectStore(aName);

  for (uint32_t i = 0; i < mCreatedObjectStores.Length(); i++) {
    if (mCreatedObjectStores[i]->Name() == aName) {
      nsRefPtr<IDBObjectStoreSync> objectStore = mCreatedObjectStores[i];
      mCreatedObjectStores.RemoveElementAt(i);
      mDeletedObjectStores.AppendElement(objectStore);
      break;
    }
  }
}

void
IDBTransactionSync::SetDBInfo(DatabaseInfoMT* aDBInfo)
{
  MOZ_ASSERT(aDBInfo != mDatabaseInfo, "This is nonsense");
  mDatabaseInfo = aDBInfo;
}

void
IDBTransactionSync::_trace(JSTracer* aTrc)
{
  IDBObjectSync::_trace(aTrc);
}

void
IDBTransactionSync::_finalize(JSFreeOp* aFop)
{
  IDBObjectSync::_finalize(aFop);
}

void
IDBTransactionSync::ReleaseIPCThreadObjects()
{
  AssertIsOnIPCThread();

  if (mActorChild) {
    mActorChild->Send__delete__(mActorChild);
    MOZ_ASSERT(!mActorChild, "Should have cleared in Send__delete__!");
  }
}

NS_IMPL_ISUPPORTS_INHERITED0(IDBTransactionSync, IDBObjectSync)

/*
void
IDBTransactionSync::GetMode(nsString& aMode)
{
  nsresult rv = IDBTransactionBase::GetMode(aMode);
  NS_ENSURE_SUCCESS(rv,);
}
*/

IDBDatabaseSync*
IDBTransactionSync::Db()
{
  MOZ_ASSERT(mDatabase, "This should never be null!");
  return mDatabase;
}

already_AddRefed<DOMStringList>
IDBTransactionSync::ObjectStoreNames(JSContext* aCx)
{
  nsRefPtr<DOMStringList> list(new DOMStringList());

  if (mMode == IDBTransactionBase::VERSION_CHANGE) {
    if(mDatabaseInfo) {
      mDatabaseInfo->GetObjectStoreNames(list->Names());
    }
  }
  else {
    list->Names() = mObjectStoreNames;
  }

  return list.forget();
}

IDBObjectStoreSync*
IDBTransactionSync::ObjectStore(JSContext* aCx, const nsAString& aName,
                                ErrorResult& aRv)
{
  if (IsInvalid()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return nullptr;
  }

  ObjectStoreInfo* info = nullptr;

  if (mMode == IDBTransaction::VERSION_CHANGE ||
      mObjectStoreNames.Contains(aName)) {
    MOZ_ASSERT(mDatabaseInfo, "mDatabaseInfo is null!");
    info = mDatabaseInfo->GetObjectStore(aName);
  }

  if (!info) {
    NS_WARNING("ObjectStoreInfo not found!");
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_FOUND_ERR);
    return nullptr;
  }

  IDBObjectStoreSync* objectStore = GetOrCreateObjectStore(aCx, info, false);
  if (!objectStore) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return nullptr;
  }

  return objectStore;
}

void
IDBTransactionSync::Abort(JSContext* aCx, ErrorResult& aRv)
{
  if (IsInvalid()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return;
  }

  mAbortCode = NS_ERROR_DOM_INDEXEDDB_ABORT_ERR;
  mInvalid = true;

  nsRefPtr<AbortRunnable> runnable =
    new AbortRunnable(mWorkerPrivate, this);

  if (!runnable->Dispatch(aCx)) {
    NS_WARNING("Runnable did not Dispatch!");
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  if (GetMode() == IDBTransactionBase::VERSION_CHANGE) {
    // If a version change transaction is aborted, we must revert the world
    // back to its previous state.
    mDatabase->RevertToPreviousState();

    DatabaseInfoMT* dbInfo = mDatabase->Info();

    for (uint32_t i = 0; i < mCreatedObjectStores.Length(); i++) {
      nsRefPtr<IDBObjectStoreSync>& objectStore = mCreatedObjectStores[i];
      ObjectStoreInfo* info = dbInfo->GetObjectStore(objectStore->Name());

      if (!info) {
        info = new ObjectStoreInfo(*objectStore->Info());
        info->indexes.Clear();
      }

      objectStore->SetInfo(info);
    }

    for (uint32_t i = 0; i < mDeletedObjectStores.Length(); i++) {
      nsRefPtr<IDBObjectStoreSync>& objectStore = mDeletedObjectStores[i];
      ObjectStoreInfo* info = dbInfo->GetObjectStore(objectStore->Name());

      if (!info) {
        info = new ObjectStoreInfo(*objectStore->Info());
        info->indexes.Clear();
      }

      objectStore->SetInfo(info);
    }

    // and then the db must be closed
    mDatabase->Close(aCx, aRv);
  }
}

bool
IDBTransactionSync::Init(JSContext* aCx)
{
  nsRefPtr<InitRunnable> runnable = new InitRunnable(mWorkerPrivate, this);

  return runnable->Dispatch(aCx);
}

IDBObjectStoreSync*
IDBTransactionSync::GetOrCreateObjectStore(JSContext* aCx,
                                           ObjectStoreInfo* aObjectStoreInfo,
                                           bool aCreating)
{
  MOZ_ASSERT(aObjectStoreInfo, "Null pointer!");
  MOZ_ASSERT(!aCreating || GetMode() == IDBTransaction::VERSION_CHANGE,
             "How else can we create here?!");

  IDBObjectStoreSync* retval = nullptr;

  for (uint32_t index = 0; index < mCreatedObjectStores.Length(); index++) {
    IDBObjectStoreSync* objectStore = mCreatedObjectStores[index];
    if (objectStore->Name() == aObjectStoreInfo->name) {
      retval = objectStore;
      return retval;
    }
  }

  retval =
    IDBObjectStoreSync::Create(aCx, this, aObjectStoreInfo);

  if (!retval) {
    NS_WARNING("objectStore not created!");
    return nullptr;
  }

  if (!retval->Init(aCx, aCreating)) {
    NS_WARNING("objectStore not initialized!");
    return nullptr;
  }

  mCreatedObjectStores.AppendElement(retval);

  return retval;
}

bool
IDBTransactionSync::Finish(JSContext* aCx)
{
  DOMBindingAnchor<IDBTransactionSync> selfAnchor(this);

  AutoSyncLoopHolder syncLoop(mWorkerPrivate);

  nsRefPtr<FinishRunnable> runnable =
    new FinishRunnable(mWorkerPrivate, syncLoop.SyncQueueKey(), this);

  if (!runnable->Dispatch(aCx)) {
    return false;
  }

  return syncLoop.RunAndForget(aCx);
}
