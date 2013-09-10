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
                       ClearingBehavior aClearingBehavior, nsresult aErrorCode)
  : WorkerSyncRunnable(aWorkerPrivate, aSyncQueueKey, false, aClearingBehavior),
    mErrorCode(aErrorCode)
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

    return true;
  }

private:
  nsresult mErrorCode;
};

} // anonymous namespace

#ifdef DEBUG
void
mozilla::dom::workers::AssertIsOnIPCThread()
{
  NS_ASSERTION(MessageLoop::current() == RuntimeService::IPCMessageLoop(),
               "Wrong thread!");
}
#endif

bool
mozilla::dom::workers::IsOnIPCThread()
{
  return MessageLoop::current() == RuntimeService::IPCMessageLoop();
}

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
                             WorkerRunnable::SkipWhenClearing, rv);

  if (!runnable->Dispatch(nullptr)) {
    NS_WARNING("Failed to dispatch runnable!");
  }
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
                             WorkerRunnable::RunWhenClearing, mErrorCode);

  if (!runnable->Dispatch(nullptr)) {
    NS_WARNING("Failed to dispatch runnable!");
  }
}
