/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_idbdatabasesync_h__
#define mozilla_dom_workers_idbdatabasesync_h__

#include "Workers.h"

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/indexedDB/IDBDatabaseBase.h"
#include "mozilla/dom/quota/PersistenceType.h"

#include "DatabaseInfoMT.h"
#include "IDBObjectSync.h"

namespace mozilla {
namespace dom {
class DOMStringList;
struct IDBObjectStoreParameters;
//struct IDBOpenDBOptionsWorkers;
class IDBTransactionCallback;
class IDBVersionChangeCallback;
namespace indexedDB {
struct ObjectStoreInfo;
} // namespace indexedDB
} // namespace dom
} // namespace mozilla

BEGIN_WORKERS_NAMESPACE

class IDBFactorySync;
class IDBObjectStoreSync;
class IDBTransactionSync;
class IndexedDBDatabaseWorkerChild;

class IDBDatabaseSync MOZ_FINAL : public IDBObjectSyncEventTarget,
                                  public indexedDB::IDBDatabaseBase
{
  friend class IDBFactorySync;
  friend class IndexedDBDatabaseWorkerChild;

  typedef mozilla::dom::IDBObjectStoreParameters IDBObjectStoreParameters;
  typedef mozilla::dom::indexedDB::ObjectStoreInfo ObjectStoreInfo;
  typedef mozilla::dom::quota::PersistenceType PersistenceType;

public:
  NS_DECL_ISUPPORTS_INHERITED

  static IDBDatabaseSync*
  Create(JSContext* aCx, IDBFactorySync* aFactory, const nsAString& aName,
         uint64_t aVersion, PersistenceType aPersistenceType);

  virtual void
  _trace(JSTracer* aTrc) MOZ_OVERRIDE;

  virtual void
  _finalize(JSFreeOp* aFop) MOZ_OVERRIDE;

  nsCString Id() const
  {
    return mDatabaseId;
  }

  uint64_t
  RequestedVersion()
  {
    return mVersion;
  }

  PersistenceType
  Type() const
  {
    return mPersistenceType;
  }

  DatabaseInfoMT*
  Info() const
  {
    return mDatabaseInfo;
  }

  void EnterSetVersionTransaction();
  void ExitSetVersionTransaction();

  // Called when a versionchange transaction is aborted to reset the
  // DatabaseInfoMT.
  void RevertToPreviousState();

  // Methods called on the IPC thread.
  virtual void
  ReleaseIPCThreadObjects();

  IndexedDBDatabaseWorkerChild*
  GetActor() const
  {
    return mActorChild;
  }

  void
  SetActor(IndexedDBDatabaseWorkerChild* aActorChild)
  {
    MOZ_ASSERT(!aActorChild || !mActorChild, "Shouldn't have more than one!");
    mActorChild = aActorChild;
  }

  void
  OnVersionChange(uint64_t aOldVersion, uint64_t aNewVersion);

  // WebIDL
  void
  GetName(nsString& aName)
  {
    aName = mName;
  }

  uint64_t
  Version()
  {
    return mDatabaseInfo->version;
  }

  mozilla::dom::StorageType
  Storage() const
  {
    return PersistenceTypeToStorage(mPersistenceType);
  }

  already_AddRefed<DOMStringList>
  GetObjectStoreNames(JSContext* aCx, ErrorResult& aRv);

  IDBObjectStoreSync*
  CreateObjectStore(JSContext* aCx, const nsAString& aName,
                    const IDBObjectStoreParameters& aOptionalParameters,
                    ErrorResult& aRv);

  void
  DeleteObjectStore(JSContext* aCx, const nsAString& aName, ErrorResult& aRv);

  void
  Transaction(JSContext* aCx, const nsAString& aStoreName,
              IDBTransactionCallback& aCallback, IDBTransactionMode aMode,
              const Optional<uint32_t>& aTimeout, ErrorResult& aRv)
  {
    Sequence<nsString> list;
    list.AppendElement(aStoreName);
    Transaction(aCx, list, aCallback, aMode, aTimeout, aRv);
  }

  void
  Transaction(JSContext* aCx, const Sequence<nsString>& aStoreNames,
              IDBTransactionCallback& aCallback, IDBTransactionMode aMode,
              const Optional<uint32_t>& aTimeout, ErrorResult& aRv);

  JS::Value
  MozCreateFileHandle(JSContext* aCx, const nsAString& aName,
                      const Optional<nsAString>& aType, ErrorResult& aRv)
  {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
    return JSVAL_VOID;
  }

  void
  Close(JSContext* aCx, ErrorResult& aRv);

  already_AddRefed<EventHandlerNonNull>
  GetOnversionchange(ErrorResult& aRv)
  {
    return GetEventListener(NS_LITERAL_STRING("versionchange"), aRv);
  }

  void
  SetOnversionchange(EventHandlerNonNull* aListener, ErrorResult& aRv)
  {
    SetEventListener(NS_LITERAL_STRING("versionchange"), aListener, aRv);
  }

private:
  IDBDatabaseSync(JSContext* aCx, WorkerPrivate* aWorkerPrivate);
  ~IDBDatabaseSync();

  bool
  Open(JSContext* aCx, IDBVersionChangeCallback* aUpgradeCallback);

  IDBFactorySync* mFactory;
  nsRefPtr<DatabaseInfoMT> mDatabaseInfo;

  // Set to a copy of the existing DatabaseInfo when starting a versionchange
  // transaction.
  nsRefPtr<DatabaseInfoMT> mPreviousDatabaseInfo;

  nsCString mDatabaseId;
  uint64_t mVersion;
  PersistenceType mPersistenceType;

  IDBTransactionSync* mTransaction;
  bool mUpgradeNeeded;

  // Only touched on the IPC thread.
  IndexedDBDatabaseWorkerChild* mActorChild;
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_idbdatabasesync_h__
