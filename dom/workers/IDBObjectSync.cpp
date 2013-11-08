/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBObjectSync.h"

#include "base/message_loop.h"

#include "IndexedDBSyncProxies.h"
#include "RuntimeService.h"
#include "WorkerPrivate.h"

#include "ipc/IndexedDBWorkerChild.h"

USING_WORKERS_NAMESPACE
using namespace mozilla;
using namespace mozilla::dom::indexedDB;

namespace {

class AsyncTeardownRunnable : public nsRunnable
{
  nsRefPtr<IDBObjectSyncProxyBase> mProxy;

public:
  AsyncTeardownRunnable(IDBObjectSyncProxyBase* aProxy)
  {
    mProxy = aProxy;
    MOZ_ASSERT(mProxy);
  }

  bool
  Dispatch()
  {
    if (RuntimeService::IsMainProcess()) {
      MessageLoop* ipcLoop = RuntimeService::IPCMessageLoop();
      ipcLoop->PostTask(FROM_HERE,
                        NewRunnableMethod(this, &AsyncTeardownRunnable::Run));
    }
    else {
      if (NS_FAILED(NS_DispatchToMainThread(this, NS_DISPATCH_NORMAL))) {
        return false;
      }
    }

    return true;
  }

  NS_IMETHOD
  Run()
  {
    AssertIsOnIPCThread();

    mProxy->Teardown();
    mProxy = nullptr;

    return NS_OK;
  }
};

class SyncTeardownRunnable : public BlockWorkerThreadRunnable
{
  nsRefPtr<IDBObjectSyncProxyBase> mProxy;

public:
  SyncTeardownRunnable(WorkerPrivate* aWorkerPrivate, IDBObjectSyncProxyBase* aProxy)
  : BlockWorkerThreadRunnable(aWorkerPrivate), mProxy(aProxy)
  {
    MOZ_ASSERT(aWorkerPrivate);
    MOZ_ASSERT(aProxy);
  }

  virtual nsresult
  IPCThreadRun() MOZ_OVERRIDE
  {
    AssertIsOnIPCThread();

    mProxy->Teardown();

    return NS_OK;
  }
};

} // anonymous namespace

IDBObjectSyncBase::IDBObjectSyncBase(WorkerPrivate* aWorkerPrivate)
: mWorkerPrivate(aWorkerPrivate), mRooted(false), mCanceled(false)
{
}

IDBObjectSyncBase::~IDBObjectSyncBase()
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  ReleaseProxy(ObjectIsGoingAway);

  MOZ_ASSERT(!mRooted);
}

void
IDBObjectSyncBase::Unpin()
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  MOZ_ASSERT(mRooted, "Mismatched calls to Unpin!");

  JSContext* cx = GetCurrentThreadJSContext();

  mWorkerPrivate->RemoveFeature(cx, this);

  mRooted = false;

  NS_RELEASE_THIS();
}

bool
IDBObjectSyncBase::Notify(JSContext* aCx, Status aStatus)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(mWorkerPrivate->GetJSContext() == aCx);

  if (aStatus >= Canceling && !mCanceled) {
    mCanceled = true;
    ReleaseProxy(WorkerIsGoingAway);
  }

  return true;
}

void
IDBObjectSyncBase::ReleaseProxy(ReleaseType aType)
{
  // Can't assert that we're on the worker thread here because mWorkerPrivate
  // may be gone.

  if (!mProxy) {
    return;
  }

  if (aType == ObjectIsGoingAway) {
    // We're in a GC finalizer, so we can't do a sync call here (and we don't
    // need to).
    nsRefPtr<AsyncTeardownRunnable> runnable =
      new AsyncTeardownRunnable(mProxy);
    mProxy = nullptr;

    if (!runnable->Dispatch()) {
      NS_ERROR("Failed to dispatch teardown runnable!");
    }
  } else {
    // We need to make a sync call here.
    nsRefPtr<SyncTeardownRunnable> runnable =
      new SyncTeardownRunnable(mWorkerPrivate, mProxy);
    mProxy = nullptr;

    if (!runnable->Dispatch(nullptr)) {
      NS_ERROR("Failed to dispatch teardown runnable!");
    }
  }
}

void
IDBObjectSyncBase::Pin()
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  MOZ_ASSERT(!mRooted);

  JSContext* cx = GetCurrentThreadJSContext();

  if (!mWorkerPrivate->AddFeature(cx, this)) {
    return;
  }

  NS_ADDREF_THIS();

  mRooted = true;
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(IDBObjectSync)
NS_IMPL_CYCLE_COLLECTING_RELEASE(IDBObjectSync)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IDBObjectSync)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBObjectSync)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(IDBObjectSync)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->ReleaseProxy(ObjectIsGoingAway);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(IDBObjectSync)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(IDBObjectSync)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

IDBObjectSync::IDBObjectSync(WorkerPrivate* aWorkerPrivate)
: IDBObjectSyncBase(aWorkerPrivate)
{
}

NS_IMPL_ADDREF_INHERITED(IDBObjectSyncEventTarget, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(IDBObjectSyncEventTarget, nsDOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(IDBObjectSyncEventTarget)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBObjectSyncEventTarget)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IDBObjectSyncEventTarget,
                                                nsDOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IDBObjectSyncEventTarget,
                                                  nsDOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(IDBObjectSyncEventTarget,
                                               nsDOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

IDBObjectSyncEventTarget::IDBObjectSyncEventTarget(
                                                  WorkerPrivate* aWorkerPrivate)
: IDBObjectSyncBase(aWorkerPrivate)
{
}
