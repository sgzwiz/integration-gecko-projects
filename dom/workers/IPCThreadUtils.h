/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_ipcthreadutils_h__
#define mozilla_dom_workers_ipcthreadutils_h__

#include "Workers.h"

BEGIN_WORKERS_NAMESPACE

class WorkerPrivate;

class BlockWorkerThreadRunnable
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(BlockWorkerThreadRunnable)

  BlockWorkerThreadRunnable(WorkerPrivate* aWorkerPrivate);

  virtual ~BlockWorkerThreadRunnable()
  { }

  bool
  Dispatch(JSContext* aCx);

protected:
  virtual nsresult
  IPCThreadRun() = 0;

  WorkerPrivate* mWorkerPrivate;

private:
  void
  Run();

  uint32_t mSyncQueueKey;
};

class UnblockListener
{
public:
  NS_IMETHOD_(nsrefcnt)
  AddRef() = 0;

  NS_IMETHOD_(nsrefcnt)
  Release() = 0;

  virtual void
  OnUnblockRequested() = 0;

  virtual void
  OnUnblockPerformed(WorkerPrivate* aWorkerPrivate) = 0;
};

class UnblockWorkerThreadRunnable
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(UnblockWorkerThreadRunnable)

  UnblockWorkerThreadRunnable(WorkerPrivate* aWorkerPrivate,
                              uint32_t aSyncQueueKey,
                              nsresult aErrorCode,
                              UnblockListener* aListener);

  ~UnblockWorkerThreadRunnable();

  void
  RunImmediatelly()
  {
    this->Run();
  }

  bool
  Dispatch();

private:
  void
  Run();

  WorkerPrivate* mWorkerPrivate;
  uint32_t mSyncQueueKey;
  nsresult mErrorCode;
  nsRefPtr<UnblockListener> mListener;
};

bool
IsOnIPCThread();

void
AssertIsOnIPCThread();

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_ipcthreadutils_h__
