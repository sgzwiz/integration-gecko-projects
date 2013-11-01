/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_idbindexsync_h__
#define mozilla_dom_workers_idbindexsync_h__

#include "Workers.h"

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/indexedDB/IDBIndexBase.h"
#include "DatabaseInfoMT.h"

#include "IDBObjectSync.h"

namespace mozilla {
namespace dom {
namespace indexedDB {
struct IndexInfo;
} // namespace indexedDB
} // namespace dom
} // namespace mozilla

BEGIN_WORKERS_NAMESPACE

class IDBCursorSync;
class IDBObjectStoreSync;
class IndexedDBIndexWorkerChild;

class IDBIndexSync MOZ_FINAL : public IDBObjectSync,
                               public indexedDB::IDBIndexBase
{
  friend class IDBObjectStoreSync;

  typedef mozilla::dom::indexedDB::IndexInfo IndexInfo;

public:
  NS_DECL_ISUPPORTS_INHERITED

  static IDBIndexSync*
  Create(JSContext* aCx, IDBObjectStoreSync* aObjectStore,
         IndexInfo* aIndexInfo);

  virtual void
  _trace(JSTracer* aTrc) MOZ_OVERRIDE;

  virtual void
  _finalize(JSFreeOp* aFop) MOZ_OVERRIDE;

  const indexedDB::KeyPath&
  GetKeyPath() const
  {
    return mKeyPath;
  }

  // Methods called on the IPC thread.
  virtual void
  ReleaseIPCThreadObjects();

  IndexedDBIndexWorkerChild*
  GetActor() const
  {
    return mActorChild;
  }

  void
  SetActor(IndexedDBIndexWorkerChild* aActorChild)
  {
    MOZ_ASSERT(!aActorChild || !mActorChild, "Shouldn't have more than one!");
    mActorChild = aActorChild;
  }

  // WebIDL
  void
  GetName(nsString& aName)
  {
    aName = mName;
  }

  void
  GetStoreName(nsString& aStoreName);

  IDBObjectStoreSync*
  ObjectStore()
  {
    return mObjectStore;
  }

  JS::Value
  GetKeyPath(JSContext* aCx, ErrorResult& aRv);

  bool
  MultiEntry()
  {
    return mMultiEntry;
  }

  bool
  Unique()
  {
    return mUnique;
  }

  IDBCursorSync*
  OpenCursor(JSContext* aCx, const Optional<JS::Handle<JS::Value> >& aRange,
             IDBCursorDirection aDirection, ErrorResult& aRv);

  IDBCursorSync*
  OpenKeyCursor(JSContext* aCx, const Optional<JS::Handle<JS::Value> >& aRange,
                IDBCursorDirection aDirection, ErrorResult& aRv);

  JS::Value
  Get(JSContext* aCx, JS::Value aKey, ErrorResult& aRv);

  JS::Value
  GetKey(JSContext* aCx, JS::Value aKey, ErrorResult& aRv);

  JS::Value
  GetAll(JSContext* aCx, const Optional<JS::Handle<JS::Value> >& aKey,
            const Optional<uint32_t>& aLimit, ErrorResult& aRv);

  JS::Value
  GetAllKeys(JSContext* aCx, const Optional<JS::Handle<JS::Value> >& aKey,
                const Optional<uint32_t>& aLimit, ErrorResult& aRv);

  uint64_t
  Count(JSContext* aCx, const Optional<JS::Handle<JS::Value> >& aValue,
        ErrorResult& aRv);

private:
  IDBIndexSync(JSContext* aCx, WorkerPrivate* aWorkerPrivate);
  ~IDBIndexSync();

  bool
  Init(JSContext* aCx, IndexInfo* aIndexInfo, bool aCreating);

  IDBObjectStoreSync* mObjectStore;

  // Only touched on the IPC thread.
  IndexedDBIndexWorkerChild* mActorChild;
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_idbindexsync_h__
