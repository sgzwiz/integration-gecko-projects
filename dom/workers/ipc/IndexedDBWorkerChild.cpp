/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IndexedDBWorkerChild.h"

#include "mozilla/dom/quota/QuotaManager.h"

#include "BlockingHelperBase.h"
#include "DatabaseInfoMT.h"
#include "IDBCursorSync.h"
#include "IDBDatabaseSync.h"
#include "IDBFactorySync.h"
#include "IDBIndexSync.h"
#include "IDBObjectStoreSync.h"
#include "IDBTransactionSync.h"

USING_WORKERS_NAMESPACE

using namespace mozilla::dom::indexedDB;
using mozilla::dom::quota::QuotaManager;

/*******************************************************************************
 * IndexedDBWorkerChild
 ******************************************************************************/

IndexedDBWorkerChild::IndexedDBWorkerChild()
: mFactoryProxy(nullptr)
{
  MOZ_COUNT_CTOR(IndexedDBWorkerChild);
}

IndexedDBWorkerChild::~IndexedDBWorkerChild()
{
  MOZ_COUNT_DTOR(IndexedDBWorkerChild);
  MOZ_ASSERT(!mFactoryProxy);
}

void
IndexedDBWorkerChild::SetFactoryProxy(IDBFactorySyncProxy* aFactoryProxy)
{
  MOZ_ASSERT(aFactoryProxy);
  MOZ_ASSERT(!mFactoryProxy);

  aFactoryProxy->SetActor(this);
  mFactoryProxy = aFactoryProxy;
}

void
IndexedDBWorkerChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mFactoryProxy) {
    mFactoryProxy->SetActor(static_cast<IndexedDBWorkerChild*>(nullptr));
#ifdef DEBUG
    mFactoryProxy = nullptr;
#endif
  }
}

bool
IndexedDBWorkerChild::RecvResponse(const bool& aAllowed)
{
  MOZ_ASSERT(mFactoryProxy->mExpectingResponse);

  mFactoryProxy->UnblockWorkerThread(aAllowed ? NS_OK : NS_ERROR_FAILURE);

  return true;
}

PIndexedDBDatabaseChild*
IndexedDBWorkerChild::AllocPIndexedDBDatabaseChild(
                                        const nsString& aName,
                                        const uint64_t& aVersion,
                                        const PersistenceType& aPersistenceType)
{
  return new IndexedDBDatabaseWorkerChild(aName, aVersion);
}

bool
IndexedDBWorkerChild::DeallocPIndexedDBDatabaseChild(
                                                PIndexedDBDatabaseChild* aActor)
{
  delete aActor;
  return true;
}

PIndexedDBDeleteDatabaseRequestChild*
IndexedDBWorkerChild::AllocPIndexedDBDeleteDatabaseRequestChild(
                                        const nsString& aName,
                                        const PersistenceType& aPersistenceType)
{
  MOZ_CRASH("Caller is supposed to manually construct a request!");
  return NULL;
}

bool
IndexedDBWorkerChild::DeallocPIndexedDBDeleteDatabaseRequestChild(
                                   PIndexedDBDeleteDatabaseRequestChild* aActor)
{
  delete aActor;
  return true;
}

/*******************************************************************************
 * IndexedDBDatabaseWorkerChild
 ******************************************************************************/

IndexedDBDatabaseWorkerChild::IndexedDBDatabaseWorkerChild(const nsString& aName,
                                               uint64_t aVersion)
: mDatabaseProxy(NULL)
{
  MOZ_COUNT_CTOR(IndexedDBDatabaseWorkerChild);
}

IndexedDBDatabaseWorkerChild::~IndexedDBDatabaseWorkerChild()
{
  MOZ_COUNT_DTOR(IndexedDBDatabaseWorkerChild);
  MOZ_ASSERT(!mDatabaseProxy);
}

void
IndexedDBDatabaseWorkerChild::SetDatabaseProxy(
                                           IDBDatabaseSyncProxy* aDatabaseProxy)
{
  MOZ_ASSERT(aDatabaseProxy);
  MOZ_ASSERT(!mDatabaseProxy);

  aDatabaseProxy->SetActor(this);
  mDatabaseProxy = aDatabaseProxy;
}

bool
IndexedDBDatabaseWorkerChild::EnsureDatabaseInfo(
                           const DatabaseInfoGuts& aDBInfo,
                           const InfallibleTArray<ObjectStoreInfoGuts>& aOSInfo)
{
  nsCString databaseId;
  QuotaManager::GetStorageId(aDBInfo.persistenceType, aDBInfo.origin,
                             aDBInfo.name, databaseId);
  NS_ENSURE_TRUE(!databaseId.IsEmpty(), false);

  // Get existing DatabaseInfoMT or create a new one
  nsRefPtr<DatabaseInfoMT> dbInfo;
  if (DatabaseInfoMT::Get(databaseId, getter_AddRefs(dbInfo))) {
    dbInfo->version = aDBInfo.version;
  }
  else {
    nsRefPtr<DatabaseInfoMT> newInfo = new DatabaseInfoMT();

    *static_cast<DatabaseInfoGuts*>(newInfo.get()) = aDBInfo;
    newInfo->id = databaseId;

    if (!DatabaseInfoMT::Put(newInfo)) {
      NS_WARNING("Out of memory!");
      return false;
    }

    newInfo.swap(dbInfo);

    // This is more or less copied from IDBFactory::SetDatabaseMetadata.
    for (uint32_t i = 0; i < aOSInfo.Length(); i++) {
      nsRefPtr<ObjectStoreInfo> newInfo = new ObjectStoreInfo();
      *static_cast<ObjectStoreInfoGuts*>(newInfo.get()) = aOSInfo[i];

      if (!dbInfo->PutObjectStore(newInfo)) {
        NS_WARNING("Out of memory!");
        return false;
      }
    }
  }

  IDBDatabaseSync* database = mDatabaseProxy->Database();

  database->mDatabaseId = dbInfo->id;
  dbInfo.swap(database->mDatabaseInfo);

  return true;
}

void
IndexedDBDatabaseWorkerChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mDatabaseProxy) {
    mDatabaseProxy->SetActor(static_cast<IndexedDBDatabaseWorkerChild*>(NULL));
#ifdef DEBUG
    mDatabaseProxy = NULL;
#endif
  }
}

bool
IndexedDBDatabaseWorkerChild::RecvSuccess(
                           const DatabaseInfoGuts& aDBInfo,
                           const InfallibleTArray<ObjectStoreInfoGuts>& aOSInfo)
{
  MOZ_ASSERT(mDatabaseProxy->mExpectingResponse);

  IDBDatabaseSync* database = mDatabaseProxy->Database();

  MOZ_ASSERT(aDBInfo.origin == database->mFactory->GetASCIIOrigin(), "Huh?");
  MOZ_ASSERT(aDBInfo.name == database->mName, "Huh?");
  MOZ_ASSERT(!database->mVersion || aDBInfo.version == database->mVersion,
             "Huh?");

  if (!EnsureDatabaseInfo(aDBInfo, aOSInfo)) {
    return false;
  }

  mDatabaseProxy->UnblockWorkerThread(NS_OK);

  return true;
}

bool
IndexedDBDatabaseWorkerChild::RecvError(const nsresult& aRv)
{
  MOZ_ASSERT(mDatabaseProxy->mExpectingResponse);

  mDatabaseProxy->UnblockWorkerThread(aRv);

  return true;
}

bool
IndexedDBDatabaseWorkerChild::RecvBlocked(const uint64_t& aOldVersion)
{
  return true;
}

bool
IndexedDBDatabaseWorkerChild::RecvVersionChange(const uint64_t& aOldVersion,
                                                const uint64_t& aNewVersion)
{
  IDBDatabaseSync* database = mDatabaseProxy->Database();

  database->OnVersionChange(aOldVersion, aNewVersion);

  return true;
}

bool
IndexedDBDatabaseWorkerChild::RecvInvalidate()
{
  return true;
}

bool
IndexedDBDatabaseWorkerChild::RecvPIndexedDBTransactionConstructor(
                                             PIndexedDBTransactionChild* aActor,
                                             const TransactionParams& aParams)
{
  // This only happens when the parent has created a version-change transaction
  // for us.

  MOZ_ASSERT(mDatabaseProxy->mExpectingResponse);

  IndexedDBTransactionWorkerChild* actor =
    static_cast<IndexedDBTransactionWorkerChild*>(aActor);
  MOZ_ASSERT(!actor->GetTransactionProxy());

  MOZ_ASSERT(aParams.type() ==
             TransactionParams::TVersionChangeTransactionParams);

  const VersionChangeTransactionParams& params =
    aParams.get_VersionChangeTransactionParams();

  const DatabaseInfoGuts& dbInfo = params.dbInfo();
  const InfallibleTArray<ObjectStoreInfoGuts>& osInfo = params.osInfo();
  uint64_t oldVersion = params.oldVersion();

  IDBDatabaseSync* database = mDatabaseProxy->Database();

  MOZ_ASSERT(dbInfo.origin == database->mFactory->GetASCIIOrigin());
  MOZ_ASSERT(dbInfo.name == database->mName);
  MOZ_ASSERT(!database->mVersion || dbInfo.version == database->mVersion);
  MOZ_ASSERT(!database->mVersion || oldVersion < database->mVersion);

  if (!EnsureDatabaseInfo(dbInfo, osInfo)) {
    return false;
  }

  database->mTransaction->SetDBInfo(database->mDatabaseInfo);

  database->EnterSetVersionTransaction();
  database->mPreviousDatabaseInfo->version = oldVersion;

  actor->SetTransactionProxy(database->mTransaction->Proxy());

  database->OnUpgradeNeeded();

  return true;
}

PIndexedDBTransactionChild*
IndexedDBDatabaseWorkerChild::AllocPIndexedDBTransactionChild(
                                               const TransactionParams& aParams)
{
  MOZ_ASSERT(aParams.type() ==
             TransactionParams::TVersionChangeTransactionParams);
  return new IndexedDBTransactionWorkerChild();
}

bool
IndexedDBDatabaseWorkerChild::DeallocPIndexedDBTransactionChild(
                                             PIndexedDBTransactionChild* aActor)
{
  delete aActor;
  return true;
}

/*******************************************************************************
 * IndexedDBTransactionWorkerChild
 ******************************************************************************/

IndexedDBTransactionWorkerChild::IndexedDBTransactionWorkerChild()
: mTransactionProxy(NULL)
{
  MOZ_COUNT_CTOR(IndexedDBTransactionWorkerChild);
}

IndexedDBTransactionWorkerChild::~IndexedDBTransactionWorkerChild()
{
  MOZ_COUNT_DTOR(IndexedDBTransactionWorkerChild);
}

void
IndexedDBTransactionWorkerChild::SetTransactionProxy(
                                     IDBTransactionSyncProxy* aTransactionProxy)
{
  MOZ_ASSERT(aTransactionProxy);
  MOZ_ASSERT(!mTransactionProxy);

  aTransactionProxy->SetActor(this);
  mTransactionProxy = aTransactionProxy;
}

void
IndexedDBTransactionWorkerChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mTransactionProxy) {
    mTransactionProxy->SetActor(
                           static_cast<IndexedDBTransactionWorkerChild*>(NULL));
#ifdef DEBUG
    mTransactionProxy = NULL;
#endif
  }
}

bool
IndexedDBTransactionWorkerChild::RecvComplete(const CompleteParams& aParams)
{
  MOZ_ASSERT(mTransactionProxy);
  MOZ_ASSERT(mTransactionProxy->mExpectingResponse);

  nsresult resultCode;
  switch (aParams.type()) {
    case CompleteParams::TCompleteResult:
      resultCode = NS_OK;
      break;
    case CompleteParams::TAbortResult:
      resultCode = aParams.get_AbortResult().errorCode();
      if (NS_SUCCEEDED(resultCode)) {
        resultCode = NS_ERROR_DOM_INDEXEDDB_ABORT_ERR;
      }
      break;

    default:
      MOZ_CRASH("Unknown union type!");
      return false;
  }

  IDBTransactionSync* transaction = mTransactionProxy->Transaction();

  if (transaction->GetMode() == IDBTransactionBase::VERSION_CHANGE) {
    transaction->Db()->ExitSetVersionTransaction();

    if (NS_FAILED(resultCode)) {
      // This will make the database take a snapshot of it's DatabaseInfo
      ErrorResult rv;
      transaction->Db()->Close(nullptr, rv);
      // Then remove the info from the hash as it contains invalid data.
      DatabaseInfoMT::Remove(transaction->Db()->Id());
    }
  }

  mTransactionProxy->UnblockWorkerThread(NS_OK);

  return true;
}

PIndexedDBObjectStoreChild*
IndexedDBTransactionWorkerChild::AllocPIndexedDBObjectStoreChild(
                                    const ObjectStoreConstructorParams& aParams)
{
  MOZ_CRASH("Caller is supposed to manually construct an object store!");
  return NULL;
}

bool
IndexedDBTransactionWorkerChild::DeallocPIndexedDBObjectStoreChild(
                                             PIndexedDBObjectStoreChild* aActor)
{
  delete aActor;
  return true;
}

/*******************************************************************************
 * IndexedDBObjectStoreWorkerChild
 ******************************************************************************/

IndexedDBObjectStoreWorkerChild::IndexedDBObjectStoreWorkerChild()
: mObjectStoreProxy(NULL)
{
  MOZ_COUNT_CTOR(IndexedDBObjectStoreWorkerChild);
}

IndexedDBObjectStoreWorkerChild::~IndexedDBObjectStoreWorkerChild()
{
  MOZ_COUNT_DTOR(IndexedDBObjectStoreWorkerChild);
  MOZ_ASSERT(!mObjectStoreProxy);
}

void
IndexedDBObjectStoreWorkerChild::SetObjectStoreProxy(
                                     IDBObjectStoreSyncProxy* aObjectStoreProxy)
{
  MOZ_ASSERT(aObjectStoreProxy);
  MOZ_ASSERT(!mObjectStoreProxy);

  aObjectStoreProxy->SetActor(this);
  mObjectStoreProxy = aObjectStoreProxy;
}

void
IndexedDBObjectStoreWorkerChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mObjectStoreProxy) {
    mObjectStoreProxy->SetActor(static_cast<IndexedDBObjectStoreWorkerChild*>(NULL));
#ifdef DEBUG
    mObjectStoreProxy = NULL;
#endif
  }
}

bool
IndexedDBObjectStoreWorkerChild::RecvPIndexedDBCursorConstructor(
                              PIndexedDBCursorChild* aActor,
                              const ObjectStoreCursorConstructorParams& aParams)
{
  IndexedDBCursorWorkerChild* actor =
    static_cast<IndexedDBCursorWorkerChild*>(aActor);

  IndexedDBObjectStoreRequestWorkerChild* requestActor =
    static_cast<IndexedDBObjectStoreRequestWorkerChild*>(aParams.requestChild());
  MOZ_ASSERT(requestActor, "Must have an actor here!");

  IDBCursorSync* cursor =
    static_cast<IDBCursorSync*>(requestActor->GetObject());
  MOZ_ASSERT(cursor, "Must have an object here!");

  typedef indexedDB::ipc::OptionalStructuredCloneReadInfo CursorUnionType;
  SerializedStructuredCloneReadInfo cloneInfo;

  switch (aParams.optionalCloneInfo().type()) {
    case CursorUnionType::TSerializedStructuredCloneReadInfo: {
      cloneInfo =
        aParams.optionalCloneInfo().get_SerializedStructuredCloneReadInfo();
    } break;

    case CursorUnionType::Tvoid_t:
      MOZ_ASSERT(aParams.blobsChild().IsEmpty());
      break;

    default:
      MOZ_CRASH("Unknown union type!");
      return false;
  }

  Key emptyKey;
  cursor->SetCurrentKeysAndValue(aParams.key(), emptyKey, cloneInfo);

  actor->SetCursorProxy(cursor->Proxy());
  return true;
}

PIndexedDBRequestChild*
IndexedDBObjectStoreWorkerChild::AllocPIndexedDBRequestChild(
                                        const ObjectStoreRequestParams& aParams)
{
  MOZ_CRASH("Caller is supposed to manually construct a request!");
  return NULL;
}

bool
IndexedDBObjectStoreWorkerChild::DeallocPIndexedDBRequestChild(
                                                 PIndexedDBRequestChild* aActor)
{
  delete aActor;
  return false;
}

PIndexedDBIndexChild*
IndexedDBObjectStoreWorkerChild::AllocPIndexedDBIndexChild(
                                          const IndexConstructorParams& aParams)
{
  MOZ_CRASH("Caller is supposed to manually construct an index!");
  return NULL;
}

bool
IndexedDBObjectStoreWorkerChild::DeallocPIndexedDBIndexChild(
                                                   PIndexedDBIndexChild* aActor)
{
  delete aActor;
  return true;
}

PIndexedDBCursorChild*
IndexedDBObjectStoreWorkerChild::AllocPIndexedDBCursorChild(
                              const ObjectStoreCursorConstructorParams& aParams)
{
  return new IndexedDBCursorWorkerChild();
}

bool
IndexedDBObjectStoreWorkerChild::DeallocPIndexedDBCursorChild(
                                                  PIndexedDBCursorChild* aActor)
{
  delete aActor;
  return true;
}

/*******************************************************************************
 * IndexedDBIndexWorkerChild
 ******************************************************************************/

IndexedDBIndexWorkerChild::IndexedDBIndexWorkerChild()
: mIndexProxy(NULL)
{
  MOZ_COUNT_CTOR(IndexedDBIndexWorkerChild);
}

IndexedDBIndexWorkerChild::~IndexedDBIndexWorkerChild()
{
  MOZ_COUNT_DTOR(IndexedDBIndexWorkerChild);
  MOZ_ASSERT(!mIndexProxy);
}

void
IndexedDBIndexWorkerChild::SetIndexProxy(IDBIndexSyncProxy* aIndexProxy)
{
  MOZ_ASSERT(aIndexProxy);
  MOZ_ASSERT(!mIndexProxy);

  aIndexProxy->SetActor(this);
  mIndexProxy = aIndexProxy;
}

void
IndexedDBIndexWorkerChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mIndexProxy) {
    mIndexProxy->SetActor(static_cast<IndexedDBIndexWorkerChild*>(NULL));
#ifdef DEBUG
    mIndexProxy = NULL;
#endif
  }
}

bool
IndexedDBIndexWorkerChild::RecvPIndexedDBCursorConstructor(
                                    PIndexedDBCursorChild* aActor,
                                    const IndexCursorConstructorParams& aParams)
{
  IndexedDBCursorWorkerChild* actor =
    static_cast<IndexedDBCursorWorkerChild*>(aActor);

  IndexedDBIndexRequestWorkerChild* requestActor =
    static_cast<IndexedDBIndexRequestWorkerChild*>(aParams.requestChild());
  MOZ_ASSERT(requestActor, "Must have an actor here!");

  IDBCursorSync* cursor =
    static_cast<IDBCursorSync*>(requestActor->GetObject());
  MOZ_ASSERT(cursor, "Must have an object here!");

  typedef indexedDB::ipc::OptionalStructuredCloneReadInfo CursorUnionType;
  SerializedStructuredCloneReadInfo cloneInfo;

  switch (aParams.optionalCloneInfo().type()) {
    case CursorUnionType::TSerializedStructuredCloneReadInfo: {
      cloneInfo =
        aParams.optionalCloneInfo().get_SerializedStructuredCloneReadInfo();
    } break;

    case CursorUnionType::Tvoid_t:
      MOZ_ASSERT(aParams.blobsChild().IsEmpty());
      break;

    default:
      MOZ_CRASH("Unknown union type!");
      return false;
 }

  cursor->SetCurrentKeysAndValue(aParams.key(), aParams.objectKey(),
                                 cloneInfo);

  actor->SetCursorProxy(cursor->Proxy());
  return true;
}

PIndexedDBRequestChild*
IndexedDBIndexWorkerChild::AllocPIndexedDBRequestChild(
                                              const IndexRequestParams& aParams)
{
  MOZ_CRASH("Caller is supposed to manually construct a request!");
  return NULL;
}

bool
IndexedDBIndexWorkerChild::DeallocPIndexedDBRequestChild(
                                                 PIndexedDBRequestChild* aActor)
{
  delete aActor;
  return true;
}

PIndexedDBCursorChild*
IndexedDBIndexWorkerChild::AllocPIndexedDBCursorChild(
                                    const IndexCursorConstructorParams& aParams)
{
  return new IndexedDBCursorWorkerChild();
}

bool
IndexedDBIndexWorkerChild::DeallocPIndexedDBCursorChild(
                                                  PIndexedDBCursorChild* aActor)
{
  delete aActor;
  return true;
}

/*******************************************************************************
 * IndexedDBCursorWorkerChild
 ******************************************************************************/

IndexedDBCursorWorkerChild::IndexedDBCursorWorkerChild()
: mCursorProxy(NULL)
{
  MOZ_COUNT_CTOR(IndexedDBCursorWorkerChild);
}

IndexedDBCursorWorkerChild::~IndexedDBCursorWorkerChild()
{
  MOZ_COUNT_DTOR(IndexedDBCursorWorkerChild);
  MOZ_ASSERT(!mCursorProxy);
}

void
IndexedDBCursorWorkerChild::SetCursorProxy(IDBCursorSyncProxy* aCursorProxy)
{
  MOZ_ASSERT(aCursorProxy);
  MOZ_ASSERT(!mCursorProxy);

  aCursorProxy->SetActor(this);
  mCursorProxy = aCursorProxy;
}

void
IndexedDBCursorWorkerChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mCursorProxy) {
    mCursorProxy->SetActor(static_cast<IndexedDBCursorWorkerChild*>(NULL));
#ifdef DEBUG
    mCursorProxy = NULL;
#endif
  }
}

PIndexedDBRequestChild*
IndexedDBCursorWorkerChild::AllocPIndexedDBRequestChild(
                                             const CursorRequestParams& aParams)
{
  MOZ_CRASH("Caller is supposed to manually construct a request!");
  return NULL;
}

bool
IndexedDBCursorWorkerChild::DeallocPIndexedDBRequestChild(
                                                 PIndexedDBRequestChild* aActor)
{
  delete aActor;
  return true;
}

/*******************************************************************************
 * IndexedDBRequestWorkerChildBase
 ******************************************************************************/

IndexedDBRequestWorkerChildBase::IndexedDBRequestWorkerChildBase()
: mHelperProxy(nullptr)
{
  MOZ_COUNT_CTOR(IndexedDBRequestWorkerChildBase);
}

IndexedDBRequestWorkerChildBase::~IndexedDBRequestWorkerChildBase()
{
  MOZ_COUNT_DTOR(IndexedDBRequestWorkerChildBase);
}

void
IndexedDBRequestWorkerChildBase::SetHelperProxy(
                                              BlockingHelperProxy* aHelperProxy)
{
  MOZ_ASSERT(aHelperProxy);
  MOZ_ASSERT(!mHelperProxy);

  aHelperProxy->SetActor(this);
  mHelperProxy = aHelperProxy;
}

void
IndexedDBRequestWorkerChildBase::Disconnect()
{
  MOZ_ASSERT(mHelperProxy);

  mHelperProxy->SetActor(static_cast<IndexedDBRequestWorkerChildBase*>(NULL));
  mHelperProxy = nullptr;
}

IDBObjectSync*
IndexedDBRequestWorkerChildBase::GetObject() const
{
  return mHelperProxy->Helper()->Object();
}

void
IndexedDBRequestWorkerChildBase::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mHelperProxy) {
    mHelperProxy->SetActor(static_cast<IndexedDBRequestWorkerChildBase*>(NULL));
#ifdef DEBUG
    mHelperProxy = NULL;
#endif
  }
}

bool
IndexedDBRequestWorkerChildBase::Recv__delete__(const ResponseValue& aResponse)
{
  MOZ_CRASH("This should be overridden!");
  return false;
}

/*******************************************************************************
 * IndexedDBObjectStoreRequestWorkerChild
 ******************************************************************************/

IndexedDBObjectStoreRequestWorkerChild::IndexedDBObjectStoreRequestWorkerChild(
                                                 RequestType aRequestType)
: mRequestType(aRequestType)
{
  MOZ_COUNT_CTOR(IndexedDBObjectStoreRequestWorkerChild);
  MOZ_ASSERT(aRequestType > ParamsUnionType::T__None &&
             aRequestType <= ParamsUnionType::T__Last);
}

IndexedDBObjectStoreRequestWorkerChild::~IndexedDBObjectStoreRequestWorkerChild()
{
  MOZ_COUNT_DTOR(IndexedDBObjectStoreRequestWorkerChild);
}

bool
IndexedDBObjectStoreRequestWorkerChild::Recv__delete__(const ResponseValue& aResponse)
{
  MOZ_ASSERT(mHelperProxy->mExpectingResponse);

  switch (aResponse.type()) {
    case ResponseValue::Tnsresult:
      break;
    case ResponseValue::TGetResponse:
      MOZ_ASSERT(mRequestType == ParamsUnionType::TGetParams);
      break;
    case ResponseValue::TGetAllResponse:
      MOZ_ASSERT(mRequestType == ParamsUnionType::TGetAllParams);
      break;
    case ResponseValue::TGetAllKeysResponse:
      MOZ_ASSERT(mRequestType == ParamsUnionType::TGetAllKeysParams);
      break;
    case ResponseValue::TAddResponse:
      MOZ_ASSERT(mRequestType == ParamsUnionType::TAddParams);
      break;
    case ResponseValue::TPutResponse:
      MOZ_ASSERT(mRequestType == ParamsUnionType::TPutParams);
      break;
    case ResponseValue::TDeleteResponse:
      MOZ_ASSERT(mRequestType == ParamsUnionType::TDeleteParams);
      break;
    case ResponseValue::TClearResponse:
      MOZ_ASSERT(mRequestType == ParamsUnionType::TClearParams);
      break;
    case ResponseValue::TCountResponse:
      MOZ_ASSERT(mRequestType == ParamsUnionType::TCountParams);
      break;
    case ResponseValue::TOpenCursorResponse:
      MOZ_ASSERT(mRequestType == ParamsUnionType::TOpenCursorParams ||
                 mRequestType == ParamsUnionType::TOpenKeyCursorParams);
      break;

    default:
      MOZ_CRASH("Received invalid response parameters!");
      return false;
  }

  mHelperProxy->OnRequestComplete(aResponse);

  return true;
}

/*******************************************************************************
 * IndexedDBIndexRequestChild
 ******************************************************************************/

IndexedDBIndexRequestWorkerChild::IndexedDBIndexRequestWorkerChild(
                                                       RequestType aRequestType)
: mRequestType(aRequestType)
{
  MOZ_COUNT_CTOR(IndexedDBIndexRequestWorkerChild);
  MOZ_ASSERT(aRequestType > ParamsUnionType::T__None &&
             aRequestType <= ParamsUnionType::T__Last);
}

IndexedDBIndexRequestWorkerChild::~IndexedDBIndexRequestWorkerChild()
{
  MOZ_COUNT_DTOR(IndexedDBIndexRequestWorkerChild);
}

bool
IndexedDBIndexRequestWorkerChild::Recv__delete__(const ResponseValue& aResponse)
{
  MOZ_ASSERT(mHelperProxy->mExpectingResponse);

  switch (aResponse.type()) {
    case ResponseValue::Tnsresult:
      break;
    case ResponseValue::TGetResponse:
      MOZ_ASSERT(mRequestType == ParamsUnionType::TGetParams);
      break;
    case ResponseValue::TGetKeyResponse:
      MOZ_ASSERT(mRequestType == ParamsUnionType::TGetKeyParams);
      break;
    case ResponseValue::TGetAllResponse:
      MOZ_ASSERT(mRequestType == ParamsUnionType::TGetAllParams);
      break;
    case ResponseValue::TGetAllKeysResponse:
      MOZ_ASSERT(mRequestType == ParamsUnionType::TGetAllKeysParams);
      break;
    case ResponseValue::TCountResponse:
      MOZ_ASSERT(mRequestType == ParamsUnionType::TCountParams);
      break;
    case ResponseValue::TOpenCursorResponse:
      MOZ_ASSERT(mRequestType == ParamsUnionType::TOpenCursorParams ||
                 mRequestType == ParamsUnionType::TOpenKeyCursorParams);
      break;

    default:
      MOZ_CRASH("Received invalid response parameters!");
      return false;
  }

  mHelperProxy->OnRequestComplete(aResponse);

  return true;
}

/*******************************************************************************
 * IndexedDBCursorRequestWorkerChild
 ******************************************************************************/

IndexedDBCursorRequestWorkerChild::IndexedDBCursorRequestWorkerChild(
                                                       RequestType aRequestType)
: mRequestType(aRequestType)
{
  MOZ_COUNT_CTOR(IndexedDBCursorRequestWorkerChild);
  MOZ_ASSERT(aRequestType > ParamsUnionType::T__None &&
             aRequestType <= ParamsUnionType::T__Last);
}

IndexedDBCursorRequestWorkerChild::~IndexedDBCursorRequestWorkerChild()
{
  MOZ_COUNT_DTOR(IndexedDBCursorRequestWorkerChild);
}

bool
IndexedDBCursorRequestWorkerChild::Recv__delete__(
                                                 const ResponseValue& aResponse)
{
  MOZ_ASSERT(mHelperProxy->mExpectingResponse);

  switch (aResponse.type()) {
    case ResponseValue::Tnsresult:
      break;
    case ResponseValue::TContinueResponse:
      MOZ_ASSERT(mRequestType == ParamsUnionType::TContinueParams);
      break;

    default:
      MOZ_CRASH("Received invalid response parameters!");
      return false;
  }

  mHelperProxy->OnRequestComplete(aResponse);

  return true;
}

/*******************************************************************************
 * IndexedDBDeleteDatabaseRequestWorkerChild
 ******************************************************************************/

IndexedDBDeleteDatabaseRequestWorkerChild::IndexedDBDeleteDatabaseRequestWorkerChild(
                                                  const nsACString& aDatabaseId)
: mHelperProxy(NULL), mDatabaseId(aDatabaseId)
{
  MOZ_COUNT_CTOR(IndexedDBDeleteDatabaseRequestWorkerChild);
  MOZ_ASSERT(!aDatabaseId.IsEmpty());
}

IndexedDBDeleteDatabaseRequestWorkerChild::~IndexedDBDeleteDatabaseRequestWorkerChild()
{
  MOZ_COUNT_DTOR(IndexedDBDeleteDatabaseRequestWorkerChild);
}

void
IndexedDBDeleteDatabaseRequestWorkerChild::SetHelperProxy(DeleteDatabaseProxy* aHelperProxy)
{
  MOZ_ASSERT(aHelperProxy);
  MOZ_ASSERT(!mHelperProxy);

  aHelperProxy->SetActor(this);
  mHelperProxy = aHelperProxy;
}

void
IndexedDBDeleteDatabaseRequestWorkerChild::Disconnect()
{
  MOZ_ASSERT(mHelperProxy);

  mHelperProxy->SetActor(static_cast<IndexedDBDeleteDatabaseRequestWorkerChild*>(NULL));
  mHelperProxy = nullptr;
}

void
IndexedDBDeleteDatabaseRequestWorkerChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mHelperProxy) {
    mHelperProxy->SetActor(static_cast<IndexedDBDeleteDatabaseRequestWorkerChild*>(NULL));
#ifdef DEBUG
    mHelperProxy = NULL;
#endif
  }
}

bool
IndexedDBDeleteDatabaseRequestWorkerChild::Recv__delete__(const nsresult& aRv)
{
  if (NS_SUCCEEDED(aRv)) {
    DatabaseInfoMT::Remove(mDatabaseId);
  }

  mHelperProxy->UnblockWorkerThread(aRv, false);

  return true;
}

bool
IndexedDBDeleteDatabaseRequestWorkerChild::RecvBlocked(
                                                const uint64_t& aCurrentVersion)
{
  return true;
}
