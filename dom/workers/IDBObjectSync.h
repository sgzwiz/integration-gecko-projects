/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_idbobjectsync_h__
#define mozilla_dom_workers_idbobjectsync_h__

#include "mozilla/dom/workers/bindings/DOMBindingBase.h"
#include "mozilla/dom/workers/bindings/EventTarget.h"

BEGIN_WORKERS_NAMESPACE

class WorkerPrivate;

class IDBObjectSyncBase
{
public:
  uint32_t&
  PrimarySyncQueueKey()
  {
    return mPrimarySyncQueueKey;
  }

  const uint32_t&
  PrimarySyncQueueKey() const
  {
    return mPrimarySyncQueueKey;
  }

  void
  UnblockWorkerThread(nsresult aErrorCode);

  virtual void
  ReleaseIPCThreadObjects() = 0;

protected:
  IDBObjectSyncBase(WorkerPrivate* aWorkerPrivate)
  : mWorkerPrivate(aWorkerPrivate), mPrimarySyncQueueKey(UINT32_MAX)
  { }

  void
  ProxyRelease();

  WorkerPrivate* mWorkerPrivate;
  uint32_t mPrimarySyncQueueKey;
};

class IDBObjectSync : public DOMBindingBase,
                      public IDBObjectSyncBase
{
public:
  NS_DECL_ISUPPORTS_INHERITED

protected:
  IDBObjectSync(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  : DOMBindingBase(aCx), IDBObjectSyncBase(aWorkerPrivate)
  { }

  virtual void
  _trace(JSTracer* aTrc) MOZ_OVERRIDE;

  virtual void
  _finalize(JSFreeOp* aFop) MOZ_OVERRIDE;
};

class IDBObjectSyncEventTarget : public EventTarget,
                                 public IDBObjectSyncBase
{
public:
  NS_DECL_ISUPPORTS_INHERITED

protected:
  IDBObjectSyncEventTarget(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  : EventTarget(aCx), IDBObjectSyncBase(aWorkerPrivate)
  { }

  virtual void
  _trace(JSTracer* aTrc) MOZ_OVERRIDE;

  virtual void
  _finalize(JSFreeOp* aFop) MOZ_OVERRIDE;
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_idbobjectsync_h__
