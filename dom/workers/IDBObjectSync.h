/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_idbobjectsync_h__
#define mozilla_dom_workers_idbobjectsync_h__

#include "nsDOMEventTargetHelper.h"

#include "IPCThreadUtils.h"
#include "WorkerFeature.h"

BEGIN_WORKERS_NAMESPACE

class IDBObjectSyncProxyBase;
class WorkerPrivate;

class IDBObjectSyncBase : public WorkerFeature
{
public:
  NS_IMETHOD_(nsrefcnt)
  AddRef() = 0;

  NS_IMETHOD_(nsrefcnt)
  Release() = 0;

  void
  Unpin();

  bool
  Notify(JSContext* aCx, Status aStatus) MOZ_OVERRIDE;

protected:
  class AutoUnpin
  {
  public:
    AutoUnpin(IDBObjectSyncBase* aObject)
    : mObject(aObject)
    {
      MOZ_ASSERT(aObject);
    }

    ~AutoUnpin()
    {
      if (mObject) {
        mObject->Unpin();
      }
    }

    void
    Forget()
    {
      mObject = nullptr;
    }

  private:
    IDBObjectSyncBase* mObject;
  };

  IDBObjectSyncBase(WorkerPrivate* aWorkerPrivate);
  virtual ~IDBObjectSyncBase();

  enum ReleaseType { Default, ObjectIsGoingAway, WorkerIsGoingAway };

  void
  ReleaseProxy(ReleaseType aType = Default);

  void
  Pin();

  WorkerPrivate* mWorkerPrivate;
  nsRefPtr<IDBObjectSyncProxyBase> mProxy;
  bool mRooted;

private:
  bool mCanceled;
};

class IDBObjectSync : public nsISupports,
                      public nsWrapperCache,
                      public IDBObjectSyncBase
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(IDBObjectSync)

protected:
  IDBObjectSync(WorkerPrivate* aWorkerPrivate);
};

class IDBObjectSyncEventTarget : public nsDOMEventTargetHelper,
                                 public IDBObjectSyncBase
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(
                                                       IDBObjectSyncEventTarget,
                                                       nsDOMEventTargetHelper);

protected:
  IDBObjectSyncEventTarget(WorkerPrivate* aWorkerPrivate);
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_idbobjectsync_h__
