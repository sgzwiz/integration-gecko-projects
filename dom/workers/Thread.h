/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_thread_h__
#define mozilla_dom_workers_thread_h__

#include "Workers.h"

#include "nsThread.h"

BEGIN_WORKERS_NAMESPACE

// We don't actually run the XPCOM event loop during the life of a
// WorkerPrivate.  It has a separate event loop implementation inside itself.
// But we need things like NS_DispatchToCurrentThread to work so we forward
// events from the XPCOM event loop to the WorkerPrivate's normal event loop.
class Thread : public nsThread
{
public:
  Thread(uint32_t aStackSize)
  : nsThread(NOT_MAIN_THREAD, aStackSize),
    mWorkerPrivate(nullptr)
  {
  }

  // This can only be called on the worker thread.
  inline void
  SetWorkerPrivate(WorkerPrivate* aWorkerPrivate)
  {
    NS_ASSERTION(this == NS_GetCurrentThread(), "Wrong thread!");

    MutexAutoLock autoLock(mLock);
    mWorkerPrivate = aWorkerPrivate;
  }

  virtual nsresult
  PutEvent(nsIRunnable* aEvent) MOZ_OVERRIDE;

private:
  // NB: nsThread has a protected mLock member that we use to protect
  // mWorkerPrivate

  // Our current WorkerPrivate.  May be null.  If it is not null we forward
  // XPCOM events to it.
  WorkerPrivate* mWorkerPrivate;
};
/*
nsresult
Thread::PutEvent(nsIRunnable* aEvent)
{
  {
    MutexAutoLock autoLock(mLock);

    if (!mWorkerPrivate) {
      // NB: We can't just unlock and call nsThread::PutEvent here, because
      // we need to ensure that mWorkerPrivate does not change until after
      // we are done.
      nsresult rv = PutEventInternal(aEvent);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
      mWorkerPrivate->DispatchXPCOMEvent(aEvent);
    }
  }

  nsCOMPtr<nsIThreadObserver> obs = GetObserver();
  if (obs) {
    obs->OnDispatchedEvent(this);
  }

  return NS_OK;
}
*/
END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_thread_h__
