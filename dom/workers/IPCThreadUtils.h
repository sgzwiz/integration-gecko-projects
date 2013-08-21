/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_ipcthreadutils_h__
#define mozilla_dom_workers_ipcthreadutils_h__

#include "Workers.h"

BEGIN_WORKERS_NAMESPACE

#ifdef DEBUG
void
AssertIsOnIPCThread();
#else
inline void
AssertIsOnIPCThread()
{ }
#endif

bool
IsOnIPCThread();

class WorkerPrivate;

class BlockWorkerThreadRunnable
{
public:
  BlockWorkerThreadRunnable(WorkerPrivate* aWorkerPrivate);

  virtual ~BlockWorkerThreadRunnable()
  { }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(BlockWorkerThreadRunnable)

  bool
  Dispatch(JSContext* aCx);

protected:
  virtual nsresult
  IPCThreadRun() = 0;

  WorkerPrivate* mWorkerPrivate;
  uint32_t mSyncQueueKey;

private:
  void
  Run();
};

class UnblockWorkerThreadRunnable
{
public:
  UnblockWorkerThreadRunnable(WorkerPrivate* aWorkerPrivate,
                              uint32_t aSyncQueueKey,
                              nsresult aErrorCode)
  : mWorkerPrivate(aWorkerPrivate), mSyncQueueKey(aSyncQueueKey),
    mErrorCode(aErrorCode)
  {
    AssertIsOnIPCThread();
  }

  ~UnblockWorkerThreadRunnable()
  {
    AssertIsOnIPCThread();
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(UnblockWorkerThreadRunnable)

  bool
  Dispatch();

protected:
  WorkerPrivate* mWorkerPrivate;
  uint32_t mSyncQueueKey;

private:
  void
  Run();

  nsresult mErrorCode;
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_ipcthreadutils_h__
