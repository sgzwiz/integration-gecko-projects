/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IndexedDBSyncProxies.h"

#include "BlockingHelperBase.h"
#include "IDBCursorSync.h"
#include "IDBDatabaseSync.h"
#include "IDBFactorySync.h"
#include "IDBIndexSync.h"
#include "IDBObjectStoreSync.h"
#include "IDBTransactionSync.h"
#include "WorkerPrivate.h"

USING_WORKERS_NAMESPACE

namespace {

class UnpinRunnable : public WorkerControlRunnable
{
  IDBObjectSyncBase* mObject;

public:
  UnpinRunnable(WorkerPrivate* aWorkerPrivate, IDBObjectSyncBase* aObject)
  : WorkerControlRunnable(aWorkerPrivate, WorkerThread, UnchangedBusyCount),
    mObject(aObject)
  { }

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
    mObject->Unpin();
    return true;
  }
};

} // anonymous namespace

void
IDBObjectSyncProxyBase::OnUnblockRequested()
{
  AssertIsOnIPCThread();

  mExpectingResponse = false;

  mWorkerPrivate = nullptr;
  mSyncQueueKey = UINT32_MAX;
}

void
IDBObjectSyncProxyBase::OnUnblockPerformed(WorkerPrivate* aWorkerPrivate)
{
  aWorkerPrivate->AssertIsOnWorkerThread();

  mObject->Unpin();
}

void
IDBObjectSyncProxyBase::UnblockWorkerThread(nsresult aErrorCode, bool aDispatch)
{
  nsRefPtr<UnblockWorkerThreadRunnable> runnable =
    new UnblockWorkerThreadRunnable(mWorkerPrivate, mSyncQueueKey,
                                    aErrorCode, this);

  if (aDispatch) {
    if (!runnable->Dispatch()) {
      NS_WARNING("Failed to dispatch runnable!");
    }
  }
  else {
    runnable->RunImmediatelly();
  }
}

void
IDBObjectSyncProxyBase::MaybeUnpinObject()
{
  AssertIsOnIPCThread();

  if (mExpectingResponse) {
    mExpectingResponse = false;

    nsRefPtr<UnpinRunnable> runnable =
      new UnpinRunnable(mWorkerPrivate, mObject);
    if (!runnable->Dispatch(nullptr)) {
      NS_RUNTIMEABORT("We're going to hang at shutdown anyways.");
    }

    mWorkerPrivate = nullptr;
    mSyncQueueKey = UINT32_MAX;
  }
}

IDBFactorySyncProxy::IDBFactorySyncProxy(IDBFactorySync* aFactory)
: IDBObjectSyncProxy<IndexedDBWorkerChild>(aFactory)
{
}

IDBDatabaseSyncProxy::IDBDatabaseSyncProxy(IDBDatabaseSync* aDatabase)
: IDBObjectSyncProxy<IndexedDBDatabaseWorkerChild>(aDatabase)
{
}

IDBDatabaseSync*
IDBDatabaseSyncProxy::Database()
{
  return static_cast<IDBDatabaseSync*>(mObject);
}

IDBTransactionSyncProxy::IDBTransactionSyncProxy(
                                               IDBTransactionSync* aTransaction)
: IDBObjectSyncProxy<IndexedDBTransactionWorkerChild>(aTransaction)
{
}

IDBTransactionSync*
IDBTransactionSyncProxy::Transaction()
{
  return static_cast<IDBTransactionSync*>(mObject);
}

IDBObjectStoreSyncProxy::IDBObjectStoreSyncProxy(
                                               IDBObjectStoreSync* aObjectStore)
: IDBObjectSyncProxy<IndexedDBObjectStoreWorkerChild>(aObjectStore)
{
}

IDBIndexSyncProxy::IDBIndexSyncProxy(IDBIndexSync* aIndex)
: IDBObjectSyncProxy<IndexedDBIndexWorkerChild>(aIndex)
{
}

IDBCursorSyncProxy::IDBCursorSyncProxy(IDBCursorSync* aCursor)
: IDBObjectSyncProxy<IndexedDBCursorWorkerChild>(aCursor)
{
}

BlockingHelperProxy::BlockingHelperProxy(BlockingHelperBase* aHelper)
: IDBObjectSyncProxyWithActor<IndexedDBRequestWorkerChildBase>(aHelper)
{
}

void
BlockingHelperProxy::Teardown()
{
  AssertIsOnIPCThread();
  if (mActor) {
    MaybeUnpinObject();

    mActor->Disconnect();
    MOZ_ASSERT(!mActor);
  }
}

BlockingHelperBase*
BlockingHelperProxy::Helper()
{
  return static_cast<BlockingHelperBase*>(mObject);
}

void
BlockingHelperProxy::OnRequestComplete(const ResponseValue& aResponseValue)
{
  nsresult rv;

  if (aResponseValue.type() == ResponseValue::Tnsresult) {
    MOZ_ASSERT(NS_FAILED(aResponseValue.get_nsresult()), "Huh?");
    rv = aResponseValue.get_nsresult();
  }
  else {
    rv = Helper()->UnpackResponse(aResponseValue);
  }

  UnblockWorkerThread(rv, false);
}

DeleteDatabaseProxy::DeleteDatabaseProxy(DeleteDatabaseHelper* aHelper)
: IDBObjectSyncProxyWithActor<IndexedDBDeleteDatabaseRequestWorkerChild>(aHelper)
{
}

DeleteDatabaseProxy*
DeleteDatabaseHelper::Proxy() const
{
  return static_cast<DeleteDatabaseProxy*>(mProxy.get());
}

void
DeleteDatabaseProxy::Teardown()
{
  AssertIsOnIPCThread();
  if (mActor) {
    MaybeUnpinObject();

    mActor->Disconnect();
    MOZ_ASSERT(!mActor);
  }
}
