/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Thread.h"
#include "WorkerPrivate.h"

BEGIN_WORKERS_NAMESPACE

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
      if (!mWorkerPrivate->DispatchXPCOMEvent(aEvent)) {
        return NS_ERROR_FAILURE;
      }
    }
  }

  nsCOMPtr<nsIThreadObserver> obs = GetObserver();
  if (obs) {
    obs->OnDispatchedEvent(this);
  }

  return NS_OK;
}

END_WORKERS_NAMESPACE