/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IPCThreadUtils.h"

#include "base/thread.h"
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

  MessageLoop* ipcLoop = RuntimeService::IPCMessageLoop();
  ipcLoop->PostTask(FROM_HERE,
                    NewRunnableMethod(this, &BlockWorkerThreadRunnable::Run));

  return syncLoop.RunAndForget(aCx);
}

void
BlockWorkerThreadRunnable::Run()
{
  AssertIsOnIPCThread();

  nsresult rv = IPCThreadRun();

  nsRefPtr<StopSyncloopRunnable> runnable =
    new StopSyncloopRunnable(mWorkerPrivate, mSyncQueueKey,
                             WorkerRunnable::SkipWhenClearing, rv, nullptr);

  if (!runnable->Dispatch(nullptr)) {
    NS_WARNING("Failed to dispatch runnable!");
  }
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

  MessageLoop* ipcLoop = RuntimeService::IPCMessageLoop();
  ipcLoop->PostTask(FROM_HERE,
                    NewRunnableMethod(this, &UnblockWorkerThreadRunnable::Run));

  return true;
}

void
UnblockWorkerThreadRunnable::Run()
{
  AssertIsOnIPCThread();

  nsRefPtr<StopSyncloopRunnable> runnable =
    new StopSyncloopRunnable(mWorkerPrivate, mSyncQueueKey,
                             WorkerRunnable::RunWhenClearing, mErrorCode,
                             mListener);

  if (!runnable->Dispatch(nullptr)) {
    NS_WARNING("Failed to dispatch runnable!");
    return;
  }

  if (mListener) {
    mListener->OnUnblockRequested();
  }
}

bool
mozilla::dom::workers::IsOnIPCThread()
{
  MOZ_ASSERT(RuntimeService::IPCMessageLoop(), "IPC not yet initialized!");

  return MessageLoop::current() == RuntimeService::IPCMessageLoop();
}

void
mozilla::dom::workers::AssertIsOnIPCThread()
{
  MOZ_ASSERT(IsOnIPCThread(), "Wrong thread!");
}
