/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IPCThreadUtils.h"

#include "base/message_loop.h"
#include "mozilla/dom/Exceptions.h"

#include "RuntimeService.h"
#include "WorkerPrivate.h"

USING_WORKERS_NAMESPACE

using mozilla::dom::Throw;

namespace {

class StopSyncloopRunnable : public WorkerSyncRunnable
{
public:
  StopSyncloopRunnable(WorkerPrivate* aWorkerPrivate, uint32_t aSyncQueueKey,
                       ClearingBehavior aClearingBehavior, nsresult aErrorCode,
                       UnblockListener* aListener)
  : WorkerSyncRunnable(aWorkerPrivate, aSyncQueueKey, false, aClearingBehavior),
    mErrorCode(aErrorCode), mListener(aListener)
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
    if (NS_FAILED(mErrorCode)) {
      Throw(aCx, mErrorCode);
      aWorkerPrivate->StopSyncLoop(mSyncQueueKey, false);
    }
    else {
      aWorkerPrivate->StopSyncLoop(mSyncQueueKey, true);
    }

    if (mListener) {
      mListener->OnUnblockPerformed(aWorkerPrivate);
    }

    return true;
  }

private:
  nsresult mErrorCode;
  nsRefPtr<UnblockListener> mListener;
};

} // anonymous namespace

BlockWorkerThreadRunnable::BlockWorkerThreadRunnable(
                                                  WorkerPrivate* aWorkerPrivate)
: mWorkerPrivate(aWorkerPrivate), mSyncQueueKey(0)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
}

bool
BlockWorkerThreadRunnable::Dispatch(JSContext* aCx)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  AutoSyncLoopHolder syncLoop(mWorkerPrivate);
  mSyncQueueKey = syncLoop.SyncQueueKey();

  if (RuntimeService::IsMainProcess()) {
    MessageLoop* ipcLoop = RuntimeService::IPCMessageLoop();
    ipcLoop->PostTask(FROM_HERE,
                      NewRunnableMethod(this, &BlockWorkerThreadRunnable::Run));
  }
  else {
    if (NS_FAILED(NS_DispatchToMainThread(this, NS_DISPATCH_NORMAL))) {
      return false;
    }
  }

  return syncLoop.RunAndForget(aCx);
}

NS_IMETHODIMP
BlockWorkerThreadRunnable::Run()
{
  AssertIsOnIPCThread();

  nsresult rv = IPCThreadRun();

  PostRun();

  nsRefPtr<StopSyncloopRunnable> runnable =
    new StopSyncloopRunnable(mWorkerPrivate, mSyncQueueKey,
                             WorkerRunnable::SkipWhenClearing, rv, nullptr);

  if (!runnable->Dispatch(nullptr)) {
    NS_WARNING("Failed to dispatch runnable!");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

UnblockWorkerThreadRunnable::UnblockWorkerThreadRunnable(
                                                  WorkerPrivate* aWorkerPrivate,
                                                  uint32_t aSyncQueueKey,
                                                  nsresult aErrorCode,
                                                  UnblockListener* aListener)
: mWorkerPrivate(aWorkerPrivate), mSyncQueueKey(aSyncQueueKey),
  mErrorCode(aErrorCode), mListener(aListener)
{
  AssertIsOnIPCThread();
}

UnblockWorkerThreadRunnable::~UnblockWorkerThreadRunnable()
{
  AssertIsOnIPCThread();
}

bool
UnblockWorkerThreadRunnable::Dispatch()
{
  AssertIsOnIPCThread();

  if (RuntimeService::IsMainProcess()) {
    MessageLoop* ipcLoop = RuntimeService::IPCMessageLoop();
    ipcLoop->PostTask(FROM_HERE,
                      NewRunnableMethod(this, &UnblockWorkerThreadRunnable::Run));
  }
  else {
    if (NS_FAILED(NS_DispatchToMainThread(this, NS_DISPATCH_NORMAL))) {
      return false;
    }
  }

  return true;
}

NS_IMETHODIMP
UnblockWorkerThreadRunnable::Run()
{
  AssertIsOnIPCThread();

  nsRefPtr<StopSyncloopRunnable> runnable =
    new StopSyncloopRunnable(mWorkerPrivate, mSyncQueueKey,
                             WorkerRunnable::RunWhenClearing, mErrorCode,
                             mListener);

  if (!runnable->Dispatch(nullptr)) {
    NS_WARNING("Failed to dispatch runnable!");
    return NS_ERROR_FAILURE;
  }

  if (mListener) {
    mListener->OnUnblockRequested();
  }

  return NS_OK;
}

bool
mozilla::dom::workers::IsOnIPCThread()
{
  if (RuntimeService::IsMainProcess()) {
    MOZ_ASSERT(RuntimeService::IPCMessageLoop(), "IPC not yet initialized!");

    return MessageLoop::current() == RuntimeService::IPCMessageLoop();
  }

  return NS_IsMainThread();
}

void
mozilla::dom::workers::AssertIsOnIPCThread()
{
  MOZ_ASSERT(IsOnIPCThread(), "Wrong thread!");
}
