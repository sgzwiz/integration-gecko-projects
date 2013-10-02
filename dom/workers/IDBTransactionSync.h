/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_idbtransactionsync_h__
#define mozilla_dom_workers_idbtransactionsync_h__

#include "Workers.h"

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/indexedDB/IDBTransactionBase.h"

#include "IDBObjectSync.h"

namespace mozilla {
namespace dom {
class DOMStringList;
namespace indexedDB {
struct ObjectStoreInfo;
} // namespace indexedDB
} // namespace dom
} // namespace mozilla

BEGIN_WORKERS_NAMESPACE

struct DatabaseInfoMT;
class IDBDatabaseSync;
class IDBObjectStoreSync;
class IndexedDBTransactionWorkerChild;


class IDBTransactionSync MOZ_FINAL : public IDBObjectSync,
                                     public indexedDB::IDBTransactionBase
{
  friend class IDBDatabaseSync;

public:
  NS_DECL_ISUPPORTS_INHERITED

  static IDBTransactionSync*
  Create(JSContext* aCx, IDBDatabaseSync* aDatabase,
         const Sequence<nsString>& aObjectStoreNames, Mode aMode);

  virtual void
  _trace(JSTracer* aTrc) MOZ_OVERRIDE;

  virtual void
  _finalize(JSFreeOp* aFop) MOZ_OVERRIDE;

  DatabaseInfoMT*
  DBInfo() const
  {
    return mDatabaseInfo;
  }

  // 'Get' prefix is to avoid name collisions with the enum
  Mode
  GetMode()
  {
    return mMode;
  }

  nsTArray<nsString>&
  GetObjectStoreNames()
  {
    return mObjectStoreNames;
  }

  void
  RemoveObjectStore(const nsAString& aName);

  bool
  IsInvalid()
  {
    return mInvalid;
  }

  void
  SetDBInfo(DatabaseInfoMT* aDBInfo);

  // Methods called on the IPC thread.
  virtual void
  ReleaseIPCThreadObjects();

  IndexedDBTransactionWorkerChild*
  GetActor() const
  {
    return mActorChild;
  }

  void
  SetActor(IndexedDBTransactionWorkerChild* aActorChild)
  {
    MOZ_ASSERT(!aActorChild || !mActorChild, "Shouldn't have more than one!");
    mActorChild = aActorChild;
  }

  // WebIDL
  IDBTransactionMode
  GetMode(ErrorResult& aRv) const
  {
    return IDBTransactionBase::GetMode(aRv);
  }

  IDBDatabaseSync*
  Db();

  already_AddRefed<DOMStringList>
  ObjectStoreNames(JSContext* aCx);

  IDBObjectStoreSync*
  ObjectStore(JSContext* aCx, const nsAString& aName, ErrorResult& aRv);

  void
  Abort(JSContext* aCx, ErrorResult& aRv);

private:
  IDBTransactionSync(JSContext* aCx, WorkerPrivate* aWorkerPrivate);
  ~IDBTransactionSync();

  bool
  Init(JSContext* aCx);

  IDBObjectStoreSync*
  GetOrCreateObjectStore(JSContext* aCx,
                         indexedDB::ObjectStoreInfo* aObjectStoreInfo,
                         bool aCreating);

  bool
  Finish(JSContext* aCx);

  IDBDatabaseSync* mDatabase;
  nsRefPtr<DatabaseInfoMT> mDatabaseInfo;
  nsTArray<nsRefPtr<IDBObjectStoreSync> > mCreatedObjectStores;
  nsTArray<nsRefPtr<IDBObjectStoreSync> > mDeletedObjectStores;

  // Only touched on the IPC thread.
  IndexedDBTransactionWorkerChild* mActorChild;

  bool mInvalid;
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_idbtransactionsync_h__
