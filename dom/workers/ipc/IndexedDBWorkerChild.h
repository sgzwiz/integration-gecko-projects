/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_indexeddbworkerchild_h__
#define mozilla_dom_workers_indexeddbworkerchild_h__

#include "Workers.h"

#include "mozilla/dom/indexedDB/PIndexedDBChild.h"
#include "mozilla/dom/indexedDB/PIndexedDBCursorChild.h"
#include "mozilla/dom/indexedDB/PIndexedDBDatabaseChild.h"
#include "mozilla/dom/indexedDB/PIndexedDBDeleteDatabaseRequestChild.h"
#include "mozilla/dom/indexedDB/PIndexedDBIndexChild.h"
#include "mozilla/dom/indexedDB/PIndexedDBObjectStoreChild.h"
#include "mozilla/dom/indexedDB/PIndexedDBRequestChild.h"
#include "mozilla/dom/indexedDB/PIndexedDBTransactionChild.h"

BEGIN_WORKERS_NAMESPACE

class BlockingHelperBase;
class DeleteDatabaseHelper;
class IDBCursorSync;
class IDBDatabaseSync;
class IDBFactorySync;
class IDBIndexSync;
class IDBObjectStoreSync;
class IDBObjectSync;
class IDBTransactionSync;

/*******************************************************************************
 * IndexedDBChild
 ****************************
 **************************************************/

class IndexedDBWorkerChild : public mozilla::dom::indexedDB::PIndexedDBChild
{
  IDBFactorySync* mFactory;

public:
  IndexedDBWorkerChild();
  virtual ~IndexedDBWorkerChild();

  void
  SetFactory(IDBFactorySync* aProxy);

protected:
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool
  RecvResponse(const bool& aAllowed) MOZ_OVERRIDE;

  virtual PIndexedDBDatabaseChild*
  AllocPIndexedDBDatabaseChild(const nsString& aName, const uint64_t& aVersion,
                               const PersistenceType& aPersistenceType)
                               MOZ_OVERRIDE;

  virtual bool
  DeallocPIndexedDBDatabaseChild(PIndexedDBDatabaseChild* aActor) MOZ_OVERRIDE;

  virtual PIndexedDBDeleteDatabaseRequestChild*
  AllocPIndexedDBDeleteDatabaseRequestChild(
                                        const nsString& aName,
                                        const PersistenceType& aPersistenceType)
                                        MOZ_OVERRIDE;

  virtual bool
  DeallocPIndexedDBDeleteDatabaseRequestChild(
                                   PIndexedDBDeleteDatabaseRequestChild* aActor)
                                   MOZ_OVERRIDE;
};

/*******************************************************************************
 * IndexedDBDatabaseChild
 ******************************************************************************/

class IndexedDBDatabaseWorkerChild : public mozilla::dom::indexedDB::
                                            PIndexedDBDatabaseChild
{
  IDBDatabaseSync* mDatabase;

public:
  IndexedDBDatabaseWorkerChild(const nsString& aName, uint64_t aVersion);
  virtual ~IndexedDBDatabaseWorkerChild();

  void
  SetDatabase(IDBDatabaseSync* aDatabase);

protected:  bool
  EnsureDatabaseInfo(const DatabaseInfoGuts& aDBInfo,
                     const InfallibleTArray<ObjectStoreInfoGuts>& aOSInfo);

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool
  RecvSuccess(const DatabaseInfoGuts& aDBInfo,
              const InfallibleTArray<ObjectStoreInfoGuts>& aOSInfo)
              MOZ_OVERRIDE;

  virtual bool
  RecvError(const nsresult& aRv) MOZ_OVERRIDE;

  virtual bool
  RecvBlocked(const uint64_t& aOldVersion) MOZ_OVERRIDE;

  virtual bool
  RecvVersionChange(const uint64_t& aOldVersion, const uint64_t& aNewVersion)
                    MOZ_OVERRIDE;

  virtual bool
  RecvInvalidate() MOZ_OVERRIDE;

  virtual bool
  RecvPIndexedDBTransactionConstructor(PIndexedDBTransactionChild* aActor,
                                       const TransactionParams& aParams)
                                       MOZ_OVERRIDE;

  virtual PIndexedDBTransactionChild*
  AllocPIndexedDBTransactionChild(const TransactionParams& aParams)
                                  MOZ_OVERRIDE;

  virtual bool
  DeallocPIndexedDBTransactionChild(PIndexedDBTransactionChild* aActor)
                                    MOZ_OVERRIDE;
};

/*******************************************************************************
 * IndexedDBTransactionChild
 ******************************************************************************/

class IndexedDBTransactionWorkerChild : public mozilla::dom::indexedDB::
                                               PIndexedDBTransactionChild
{
  IDBTransactionSync* mTransaction;

public:
  IndexedDBTransactionWorkerChild();
  virtual ~IndexedDBTransactionWorkerChild();

  void
  SetTransaction(IDBTransactionSync* aTransaction);

  IDBTransactionSync*
  GetTransaction() const
  {
    return mTransaction;
  }

protected:
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool
  RecvComplete(const CompleteParams& aParams) MOZ_OVERRIDE;

  virtual PIndexedDBObjectStoreChild*
  AllocPIndexedDBObjectStoreChild(const ObjectStoreConstructorParams& aParams)
                                  MOZ_OVERRIDE;

  virtual bool
  DeallocPIndexedDBObjectStoreChild(PIndexedDBObjectStoreChild* aActor)
                                    MOZ_OVERRIDE;
};

/*******************************************************************************
 * IndexedDBObjectStoreWorkerChild
 ******************************************************************************/

class IndexedDBObjectStoreWorkerChild : public mozilla::dom::indexedDB::
                                               PIndexedDBObjectStoreChild
{
  IDBObjectStoreSync* mObjectStore;

public:
  IndexedDBObjectStoreWorkerChild();
  virtual ~IndexedDBObjectStoreWorkerChild();

  void
  SetObjectStore(IDBObjectStoreSync* aObjectStore);

protected:
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool
  RecvPIndexedDBCursorConstructor(
                              PIndexedDBCursorChild* aActor,
                              const ObjectStoreCursorConstructorParams& aParams)
                              MOZ_OVERRIDE;

  virtual PIndexedDBRequestChild*
  AllocPIndexedDBRequestChild(const ObjectStoreRequestParams& aParams)
                              MOZ_OVERRIDE;

  virtual bool
  DeallocPIndexedDBRequestChild(PIndexedDBRequestChild* aActor) MOZ_OVERRIDE;

  virtual PIndexedDBIndexChild*
  AllocPIndexedDBIndexChild(const IndexConstructorParams& aParams) MOZ_OVERRIDE;

  virtual bool
  DeallocPIndexedDBIndexChild(PIndexedDBIndexChild* aActor) MOZ_OVERRIDE;

  virtual PIndexedDBCursorChild*
  AllocPIndexedDBCursorChild(const ObjectStoreCursorConstructorParams& aParams)
                             MOZ_OVERRIDE;

  virtual bool
  DeallocPIndexedDBCursorChild(PIndexedDBCursorChild* aActor) MOZ_OVERRIDE;
};

/*******************************************************************************
 * IndexedDBIndexWorkerChild
 ******************************************************************************/

class IndexedDBIndexWorkerChild : public mozilla::dom::indexedDB::
                                         PIndexedDBIndexChild
{
  IDBIndexSync* mIndex;

public:
  IndexedDBIndexWorkerChild();
  virtual ~IndexedDBIndexWorkerChild();

  void
  SetIndex(IDBIndexSync* aIndex);

protected:
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool
  RecvPIndexedDBCursorConstructor(PIndexedDBCursorChild* aActor,
                                  const IndexCursorConstructorParams& aParams)
                                  MOZ_OVERRIDE;

  virtual PIndexedDBRequestChild*
  AllocPIndexedDBRequestChild(const IndexRequestParams& aParams) MOZ_OVERRIDE;

  virtual bool
  DeallocPIndexedDBRequestChild(PIndexedDBRequestChild* aActor) MOZ_OVERRIDE;

  virtual PIndexedDBCursorChild*
  AllocPIndexedDBCursorChild(const IndexCursorConstructorParams& aParams)
                             MOZ_OVERRIDE;

  virtual bool
  DeallocPIndexedDBCursorChild(PIndexedDBCursorChild* aActor) MOZ_OVERRIDE;
};

/*******************************************************************************
 * IndexedDBCursorWorkerChild
 ******************************************************************************/

class IndexedDBCursorWorkerChild : public mozilla::dom::indexedDB::
                                          PIndexedDBCursorChild
{
  IDBCursorSync* mCursor;

public:
  IndexedDBCursorWorkerChild();
  virtual ~IndexedDBCursorWorkerChild();

  void
  SetCursor(IDBCursorSync* aCursor);

protected:
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual PIndexedDBRequestChild*
  AllocPIndexedDBRequestChild(const CursorRequestParams& aParams) MOZ_OVERRIDE;

  virtual bool
  DeallocPIndexedDBRequestChild(PIndexedDBRequestChild* aActor) MOZ_OVERRIDE;
};

/*******************************************************************************
 * IndexedDBRequestWorkerChildBase
 ******************************************************************************/

class IndexedDBRequestWorkerChildBase : public mozilla::dom::indexedDB::
                                        PIndexedDBRequestChild
{
protected:
  BlockingHelperBase* mHelper;

public:
  void
  SetHelper(BlockingHelperBase* aHelper);

  IDBObjectSync*
  GetObject() const;

protected:
  IndexedDBRequestWorkerChildBase();
  virtual ~IndexedDBRequestWorkerChildBase();

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool
  Recv__delete__(const ResponseValue& aResponse) MOZ_OVERRIDE;
};

/*******************************************************************************
 * IndexedDBObjectStoreRequestWorkerChild
 ******************************************************************************/

class IndexedDBObjectStoreRequestWorkerChild : public IndexedDBRequestWorkerChildBase
{
  typedef mozilla::dom::indexedDB::ipc::ObjectStoreRequestParams
                                                                ParamsUnionType;
  typedef ParamsUnionType::Type RequestType;
  DebugOnly<RequestType> mRequestType;

public:
  IndexedDBObjectStoreRequestWorkerChild(RequestType aRequestType);
  virtual ~IndexedDBObjectStoreRequestWorkerChild();

protected:
  virtual bool
  Recv__delete__(const ResponseValue& aResponse) MOZ_OVERRIDE;
};

/*******************************************************************************
 * IndexedDBIndexRequestWorkerChild
 ******************************************************************************/

class IndexedDBIndexRequestWorkerChild : public IndexedDBRequestWorkerChildBase
{
  typedef mozilla::dom::indexedDB::ipc::IndexRequestParams ParamsUnionType;
  typedef ParamsUnionType::Type RequestType;
  DebugOnly<RequestType> mRequestType;

public:
  IndexedDBIndexRequestWorkerChild(RequestType aRequestType);
  virtual ~IndexedDBIndexRequestWorkerChild();

protected:
  virtual bool
  Recv__delete__(const ResponseValue& aResponse) MOZ_OVERRIDE;
};

/*******************************************************************************
 * IndexedDBCursorRequestWorkerChild
 ******************************************************************************/

class IndexedDBCursorRequestWorkerChild : public IndexedDBRequestWorkerChildBase
{
  typedef mozilla::dom::indexedDB::ipc::CursorRequestParams ParamsUnionType;
  typedef ParamsUnionType::Type RequestType;
  DebugOnly<RequestType> mRequestType;

public:
  IndexedDBCursorRequestWorkerChild(RequestType aRequestType);
  virtual ~IndexedDBCursorRequestWorkerChild();

protected:
  virtual bool
  Recv__delete__(const ResponseValue& aResponse) MOZ_OVERRIDE;
};

/*******************************************************************************
 * IndexedDBDeleteDatabaseRequestWorkerChild
 ******************************************************************************/

class IndexedDBDeleteDatabaseRequestWorkerChild :
  public mozilla::dom::indexedDB::PIndexedDBDeleteDatabaseRequestChild
{
  DeleteDatabaseHelper* mHelper;
  nsCString mDatabaseId;

public:
  IndexedDBDeleteDatabaseRequestWorkerChild(const nsACString& aDatabaseId);
  virtual ~IndexedDBDeleteDatabaseRequestWorkerChild();

  void
  SetHelper(DeleteDatabaseHelper* aHelper);

protected:
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool
  Recv__delete__(const nsresult& aRv) MOZ_OVERRIDE;

  virtual bool
  RecvBlocked(const uint64_t& aCurrentVersion) MOZ_OVERRIDE;
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_indexeddbworkerchild_h__
