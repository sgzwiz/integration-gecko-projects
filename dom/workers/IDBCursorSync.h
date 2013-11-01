/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_idbcursorsync_h__
#define mozilla_dom_workers_idbcursorsync_h__

#include "Workers.h"

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/indexedDB/IDBCursorBase.h"

#include "IDBObjectSync.h"

namespace mozilla {
namespace dom {
namespace indexedDB {
class IDBKeyRange;
class Key;
} // namespace indexedDB
} // namespace dom
} // namespace mozilla

BEGIN_WORKERS_NAMESPACE

class ContinueHelper;
class IDBIndexSync;
class IDBObjectStoreSync;
class IDBTransactionSync;
class IndexedDBCursorWorkerChild;
class IndexedDBIndexWorkerChild;
class OpenHelper;

class IDBCursorSync : public IDBObjectSync,
                      public indexedDB::IDBCursorBase
{
  friend class ContinueHelper;
  friend class IDBIndexSync;
  friend class IDBObjectStoreSync;
  friend class IndexedDBIndexWorkerChild;
  friend class IndexedDBObjectStoreWorkerChild;
  friend class OpenHelper;

  typedef mozilla::dom::indexedDB::StructuredCloneReadInfo
                                                        StructuredCloneReadInfo;
  typedef mozilla::dom::indexedDB::SerializedStructuredCloneReadInfo
                                              SerializedStructuredCloneReadInfo;

public:
  NS_DECL_ISUPPORTS_INHERITED

  // For INDEXKEY cursors.
  static IDBCursorSync*
  Create(JSContext* aCx, IDBIndexSync* aIndex, Direction aDirection);

  // For OBJECTSTOREKEY cursors.
  static IDBCursorSync*
  Create(JSContext* aCx, IDBObjectStoreSync* aObjectStore,
         Direction aDirection);

  // For INDEXOBJECT cursors.
  static IDBCursorSync*
  CreateWithValue(JSContext* aCx, IDBIndexSync* aIndex, Direction aDirection);

  // For OBJECTSTORE cursors.
  static IDBCursorSync*
  CreateWithValue(JSContext* aCx, IDBObjectStoreSync* aObjectStore,
                  Direction aDirection);

  virtual void
  _trace(JSTracer* aTrc) MOZ_OVERRIDE;

  virtual void
  _finalize(JSFreeOp* aFop) MOZ_OVERRIDE;

  // 'Get' prefix is to avoid name collisions with the enum
  Direction GetDirection()
  {
    return mDirection;
  }

  IDBIndexSync* Index() const
  {
    return mIndex;
  }

  IDBObjectStoreSync* ObjectStore() const
  {
    return mObjectStore;
  }

  // Methods called on the IPC thread.
  virtual void
  ReleaseIPCThreadObjects();

  IndexedDBCursorWorkerChild*
  GetActor() const
  {
    return mActorChild;
  }

  void
  SetActor(IndexedDBCursorWorkerChild* aActorChild)
  {
    MOZ_ASSERT(!aActorChild || !mActorChild, "Shouldn't have more than one!");
    mActorChild = aActorChild;
  }

  // WebIDL
  JSObject*
  Source(JSContext* aCx);

  IDBCursorDirection
  GetDirection(ErrorResult& aRv) const
  {
    return IDBCursorBase::GetDirection(aRv);
  }

  JS::Value
  Key(JSContext* aCx);

  JS::Value
  PrimaryKey(JSContext* aCx);

  void
  Update(JSContext* aCx, JS::Value aValue, ErrorResult& aRv);

  bool
  Advance(JSContext* aCx, uint32_t aCount, ErrorResult& aRv);

  bool
  Continue(JSContext* aCx, const Optional<JS::Handle<JS::Value> >& aKey,
           ErrorResult& aRv);

  bool
  Delete(JSContext* aCx, ErrorResult& aRv);

  JS::Value
  GetValue(JSContext* aCx, ErrorResult& aRv);

protected:
  typedef mozilla::dom::indexedDB::IDBKeyRange IDBKeyRange;

  IDBCursorSync(JSContext* aCx, WorkerPrivate* aWorkerPrivate);
  virtual ~IDBCursorSync();

  bool
  Open(JSContext* aCx, IDBKeyRange* aKeyRange);

  bool
  ContinueInternal(JSContext* aCx, const indexedDB::Key& aKey, int32_t aCount,
                   ErrorResult& aRv);

  void
  SetCurrentKeysAndValue(const indexedDB::Key& aKey,
                         const indexedDB::Key& aObjectKey,
                         const SerializedStructuredCloneReadInfo aCloneInfo);

  IDBObjectStoreSync* mObjectStore;
  IDBIndexSync* mIndex;
  IDBTransactionSync* mTransaction;

  indexedDB::Key mContinueToKey;
  StructuredCloneReadInfo mCloneReadInfo;
  bool mHaveValue;

  // Only touched on the IPC thread.
  IndexedDBCursorWorkerChild* mActorChild;
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_idbcursorsync_h__
