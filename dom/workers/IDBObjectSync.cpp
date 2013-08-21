/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBObjectSync.h"

#include "base/thread.h"

#include "IPCThreadUtils.h"
#include "RuntimeService.h"
#include "WorkerPrivate.h"

USING_WORKERS_NAMESPACE
using namespace mozilla;

namespace {

class ProxyReleaseHelper
{
public:
  ProxyReleaseHelper(IDBObjectSyncBase* aObject)
  : mObject(aObject),
    mMutex(RuntimeService::IPCMutex()),
    mCondVar(mMutex, "ProxyReleaseHelper::mCondVar"),
    mWaiting(true)
  {
    NS_ASSERTION(mObject, "Null object!");
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ProxyReleaseHelper)

  void
  DispatchAndWaitForResult()
  {
    MessageLoop* ipcLoop = RuntimeService::IPCMessageLoop();
    ipcLoop->PostTask(FROM_HERE,
                      NewRunnableMethod(this, &ProxyReleaseHelper::Run));

    MutexAutoLock autolock(mMutex);
    while (mWaiting) {
      mCondVar.Wait();
    }
  }

private:
  void
  Run() {
    AssertIsOnIPCThread();

    mObject->ReleaseIPCThreadObjects();

    MutexAutoLock lock(mMutex);
    NS_ASSERTION(mWaiting, "Huh?!");

    mObject = nullptr;

    mWaiting = false;
    mCondVar.NotifyAll();
  }

  IDBObjectSyncBase* mObject;
  mozilla::Mutex& mMutex;
  mozilla::CondVar mCondVar;
  bool mWaiting;
};

} // anonymous namespace

void
IDBObjectSyncBase::UnblockWorkerThread(nsresult aErrorCode)
{
  nsRefPtr<UnblockWorkerThreadRunnable> runnable =
    new UnblockWorkerThreadRunnable(mWorkerPrivate, mPrimarySyncQueueKey,
                                    aErrorCode);

  if (!runnable->Dispatch()) {
    NS_WARNING("Failed to dispatch runnable!");
  }

  mPrimarySyncQueueKey = UINT32_MAX;
}

void
IDBObjectSyncBase::ProxyRelease()
{
  nsRefPtr<ProxyReleaseHelper> helper = new ProxyReleaseHelper(this);
  helper->DispatchAndWaitForResult();
}

NS_IMPL_ISUPPORTS_INHERITED0(IDBObjectSync, DOMBindingBase)

void
IDBObjectSync::_trace(JSTracer* aTrc)
{
  DOMBindingBase::_trace(aTrc);
}

void
IDBObjectSync::_finalize(JSFreeOp* aFop)
{
  // Can't assert that we're on the worker thread here because mWorkerPrivate
  // may be gone.

  ProxyRelease();

  DOMBindingBase::_finalize(aFop);
}

NS_IMPL_ISUPPORTS_INHERITED0(IDBObjectSyncEventTarget, EventTarget)

void
IDBObjectSyncEventTarget::_trace(JSTracer* aTrc)
{
  EventTarget::_trace(aTrc);
}

void
IDBObjectSyncEventTarget::_finalize(JSFreeOp* aFop)
{
  // Can't assert that we're on the worker thread here because mWorkerPrivate
  // may be gone.

  ProxyRelease();

  EventTarget::_finalize(aFop);
}
