/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_indexeddbsyncproxies_h__
#define mozilla_dom_workers_indexeddbsyncproxies_h__

#include "Workers.h"

#include "ipc/IndexedDBWorkerChild.h"

BEGIN_WORKERS_NAMESPACE

class BlockingHelperBase;
class DeleteDatabaseHelper;
class IDBCursorSync;
class IDBDatabaseSync;
class IDBFactorySync;
class IDBIndexSync;
class IDBObjectStoreSync;
class IDBObjectSyncBase;
class IDBTransactionSync;

class IDBObjectSyncProxyBase : public UnblockListener
{
public:
  // Read on multiple threads.
  WorkerPrivate* mWorkerPrivate;
  IDBObjectSyncBase* mObject;

  // Only touched on the IPC thread.
  uint32_t mSyncQueueKey;
  bool mExpectingResponse;

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(IDBObjectSyncProxyBase)

  IDBObjectSyncProxyBase(IDBObjectSyncBase* aObject)
  : mWorkerPrivate(nullptr), mObject(aObject), mSyncQueueKey(UINT32_MAX),
    mExpectingResponse(false)
  { }

  virtual ~IDBObjectSyncProxyBase()
  {
    MOZ_ASSERT(!mExpectingResponse, "We're dying too early!");
  }

  virtual void
  OnUnblockRequested() MOZ_OVERRIDE;

  virtual void
  OnUnblockPerformed(WorkerPrivate* aWorkerPrivate) MOZ_OVERRIDE;

  void
  UnblockWorkerThread(nsresult aErrorCode, bool aDispatch = true);

  virtual void
  Teardown() = 0;

protected:
  void
  MaybeUnpinObject();
};

template<class ActorType>
class IDBObjectSyncProxyWithActor : public IDBObjectSyncProxyBase
{
public:
  IDBObjectSyncProxyWithActor(IDBObjectSyncBase* aObject)
  : IDBObjectSyncProxyBase(aObject), mActor(nullptr)
  { }

  virtual ~IDBObjectSyncProxyWithActor()
  {
    MOZ_ASSERT(!mActor, "Still have an actor attached!");
  }

  ActorType*
  Actor() const
  {
    AssertIsOnIPCThread();
    return mActor;
  }

  void
  SetActor(ActorType* aActor)
  {
    AssertIsOnIPCThread();
    MOZ_ASSERT(!aActor || !mActor);

    if (mActor && !aActor) {
      MaybeUnpinObject();
    }

    mActor = aActor;
  }

protected:
  ActorType* mActor;
};

template<class ActorType>
class IDBObjectSyncProxy : public IDBObjectSyncProxyWithActor<ActorType>
{
public:
  IDBObjectSyncProxy(IDBObjectSyncBase* aObject)
  : IDBObjectSyncProxyWithActor<ActorType>(aObject)
  { }

  virtual void
  Teardown() MOZ_OVERRIDE
  {
    AssertIsOnIPCThread();
#define mActor IDBObjectSyncProxyWithActor<ActorType>::mActor
    if (mActor) {
      IDBObjectSyncProxyBase::MaybeUnpinObject();

      mActor->Send__delete__(mActor);
      MOZ_ASSERT(!mActor);
    }
#undef mActor
  }
};

class IDBFactorySyncProxy : public IDBObjectSyncProxy<IndexedDBWorkerChild>
{
public:
  IDBFactorySyncProxy(IDBFactorySync* aFactory);
};

class IDBDatabaseSyncProxy : public IDBObjectSyncProxy<IndexedDBDatabaseWorkerChild>
{
public:
  IDBDatabaseSyncProxy(IDBDatabaseSync* aDatabase);

  IDBDatabaseSync*
  Database();
};

class IDBTransactionSyncProxy : public IDBObjectSyncProxy<IndexedDBTransactionWorkerChild>
{
public:
  IDBTransactionSyncProxy(IDBTransactionSync* aTransaction);

  IDBTransactionSync*
  Transaction();
};

class IDBObjectStoreSyncProxy : public IDBObjectSyncProxy<IndexedDBObjectStoreWorkerChild>
{
public:
  IDBObjectStoreSyncProxy(IDBObjectStoreSync* aObjectStore);
};

class IDBIndexSyncProxy : public IDBObjectSyncProxy<IndexedDBIndexWorkerChild>
{
public:
  IDBIndexSyncProxy(IDBIndexSync* aIndex);
};

class IDBCursorSyncProxy : public IDBObjectSyncProxy<IndexedDBCursorWorkerChild>
{
public:
  IDBCursorSyncProxy(IDBCursorSync* aCursor);
};

class BlockingHelperProxy : public IDBObjectSyncProxyWithActor<IndexedDBRequestWorkerChildBase>
{
public:
  typedef mozilla::dom::indexedDB::ipc::ResponseValue ResponseValue;

  BlockingHelperProxy(BlockingHelperBase* aHelper);

  virtual void
  Teardown() MOZ_OVERRIDE;

  BlockingHelperBase*
  Helper();

  void
  OnRequestComplete(const ResponseValue& aResponseValue);
};

class DeleteDatabaseProxy : public IDBObjectSyncProxyWithActor<IndexedDBDeleteDatabaseRequestWorkerChild>
{
public:
  DeleteDatabaseProxy(DeleteDatabaseHelper* aHelper);

  virtual void
  Teardown() MOZ_OVERRIDE;
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_indexeddbsyncproxies_h__
