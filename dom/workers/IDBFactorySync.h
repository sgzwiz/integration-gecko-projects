/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_idbfactorysync_h__
#define mozilla_dom_workers_idbfactorysync_h__

#include "Workers.h"

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/indexedDB/IDBFactoryBase.h"
#include "mozilla/dom/quota/PersistenceType.h"

#include "IDBObjectSync.h"
#include "IPCThreadUtils.h"

namespace mozilla {
namespace dom {
struct IDBOpenDBOptions;
class IDBVersionChangeCallbackWorkers;
} // namespace dom
} // namespace mozilla

BEGIN_WORKERS_NAMESPACE

class IDBDatabaseSync;
class IndexedDBDeleteDatabaseRequestWorkerChild;
class IndexedDBWorkerChild;

class IDBFactorySync MOZ_FINAL : public IDBObjectSync,
                                 public indexedDB::IDBFactoryBase
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  static IDBFactorySync*
  Create(JSContext* aCx, JSObject* aGlobal);

  virtual void
  _trace(JSTracer* aTrc) MOZ_OVERRIDE;

  virtual void
  _finalize(JSFreeOp* aFop) MOZ_OVERRIDE;

  uint32_t&
  DeleteDatabaseSyncQueueKey()
  {
    return mDeleteDatabaseSyncQueueKey;
  }

  const uint32_t&
  DeleteDatabaseSyncQueueKey() const
  {
    return mDeleteDatabaseSyncQueueKey;
  }

  // Methods called on the IPC thread.
  virtual void
  ReleaseIPCThreadObjects();

  IndexedDBWorkerChild*
  GetActor() const
  {
    return mActorChild;
  }

  void
  SetActor(IndexedDBWorkerChild* aActorChild)
  {
    NS_ASSERTION(!aActorChild || !mActorChild, "Shouldn't have more than one!");
    mActorChild = aActorChild;
  }

  // WebIDL
  IDBDatabaseSync*
  Open(JSContext* aCx, const nsAString& aName, uint64_t aVersion,
       const Optional<OwningNonNull<IDBVersionChangeCallbackWorkers> >& aUpgradeCallback,
       const Optional<uint32_t>& aTimeout,
       ErrorResult& aRv);

  IDBDatabaseSync*
  Open(JSContext* aCx, const nsAString& aName,
       const IDBOpenDBOptions& aOptions,
       const Optional<OwningNonNull<IDBVersionChangeCallbackWorkers> >& aUpgradeCallback,
       const Optional<uint32_t>& aTimeout, ErrorResult& aRv);

  void
  DeleteDatabase(JSContext* aCx, const nsAString& aName,
                 const IDBOpenDBOptions& aOptions, ErrorResult& aRv);

private:
  IDBFactorySync(JSContext* aCx, WorkerPrivate* aWorkerPrivate);
  ~IDBFactorySync();

  uint32_t mDeleteDatabaseSyncQueueKey;

  // Only touched on the IPC thread.
  IndexedDBWorkerChild* mActorChild;
};

class DeleteDatabaseHelper : public BlockWorkerThreadRunnable
{
public:
  DeleteDatabaseHelper(WorkerPrivate* aWorkerPrivate, uint32_t aSyncQueueKey,
                       IDBFactorySync* aFactory, const nsAString& aName,
                       quota::PersistenceType aPersistenceType)
  : BlockWorkerThreadRunnable(aWorkerPrivate), mSyncQueueKey(aSyncQueueKey),
    mFactory(aFactory), mActorChild(nullptr), mName(aName),
    mPersistenceType(aPersistenceType)
  { }

  virtual
  ~DeleteDatabaseHelper()
  {
    NS_ASSERTION(!mActorChild, "Still have an actor object attached!");
  }

  void
  SetActor(IndexedDBDeleteDatabaseRequestWorkerChild* aActorChild)
  {
    NS_ASSERTION(!aActorChild || !mActorChild, "Shouldn't have more than one!");
    mActorChild = aActorChild;
  }

  void
  OnRequestComplete(nsresult aRv);

protected:
  nsresult
  IPCThreadRun();

private:
  uint32_t mSyncQueueKey;
  IDBFactorySync* mFactory;

  // Only touched on the IPC thread.
  IndexedDBDeleteDatabaseRequestWorkerChild* mActorChild;

  // In-params.
  nsString mName;
  quota::PersistenceType mPersistenceType;
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_idbfactorysync_h__
