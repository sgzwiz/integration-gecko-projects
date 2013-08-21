/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IndexedDBWorkerChild.h"

#include "mozilla/dom/quota/QuotaManager.h"

#include "BlockingHelperBase.h"
#include "DatabaseInfoSync.h"
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
: mFactory(NULL)
{
  MOZ_COUNT_CTOR(IndexedDBWorkerChild);
}

IndexedDBWorkerChild::~IndexedDBWorkerChild()
{
  MOZ_COUNT_DTOR(IndexedDBWorkerChild);
  MOZ_ASSERT(!mFactory);
}

void
IndexedDBWorkerChild::SetFactory(IDBFactorySync* aFactory)
{
  MOZ_ASSERT(aFactory);
  MOZ_ASSERT(!mFactory);

  aFactory->SetActor(this);
  mFactory = aFactory;
}

void
IndexedDBWorkerChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mFactory) {
    mFactory->SetActor(static_cast<IndexedDBWorkerChild*>(NULL));
#ifdef DEBUG
    mFactory = NULL;
#endif
  }
}

bool
IndexedDBWorkerChild::RecvResponse(const bool& aAllowed)
{
  mFactory->UnblockWorkerThread(aAllowed ? NS_OK : NS_ERROR_FAILURE);

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
: mDatabase(NULL)
{
  MOZ_COUNT_CTOR(IndexedDBDatabaseWorkerChild);
}

IndexedDBDatabaseWorkerChild::~IndexedDBDatabaseWorkerChild()
{
  MOZ_COUNT_DTOR(IndexedDBDatabaseWorkerChild);
}

void
IndexedDBDatabaseWorkerChild::SetDatabase(IDBDatabaseSync* aDatabase)
{
  MOZ_ASSERT(aDatabase);
  MOZ_ASSERT(!mDatabase);

  aDatabase->SetActor(this);
  mDatabase = aDatabase;
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

  // Get existing DatabaseInfoSync or create a new one
  nsRefPtr<DatabaseInfoSync> dbInfo;
  if (DatabaseInfoSync::Get(databaseId, getter_AddRefs(dbInfo))) {
    dbInfo->version = aDBInfo.version;
  }
  else {
    nsRefPtr<DatabaseInfoSync> newInfo = new DatabaseInfoSync();

    *static_cast<DatabaseInfoGuts*>(newInfo.get()) = aDBInfo;
    newInfo->id = databaseId;

    if (!DatabaseInfoSync::Put(newInfo)) {
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

  mDatabase->mDatabaseId = dbInfo->id;
  dbInfo.swap(mDatabase->mDatabaseInfo);

  return true;
}

void
IndexedDBDatabaseWorkerChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mDatabase) {
    mDatabase->SetActor(static_cast<IndexedDBDatabaseWorkerChild*>(NULL));
#ifdef DEBUG
    mDatabase = NULL;
#endif
  }
}

bool
IndexedDBDatabaseWorkerChild::RecvSuccess(
                           const DatabaseInfoGuts& aDBInfo,
                           const InfallibleTArray<ObjectStoreInfoGuts>& aOSInfo)
{
  NS_ASSERTION(aDBInfo.origin == mDatabase->mFactory->GetASCIIOrigin(), "Huh?");
  NS_ASSERTION(aDBInfo.name == mDatabase->mName, "Huh?");
  NS_ASSERTION(!mDatabase->mVersion || aDBInfo.version == mDatabase->mVersion,
               "Huh?");

  if (!EnsureDatabaseInfo(aDBInfo, aOSInfo)) {
    return false;
  }

  mDatabase->UnblockWorkerThread(NS_OK);

  return true;
}

bool
IndexedDBDatabaseWorkerChild::RecvError(const nsresult& aRv)
{
  mDatabase->UnblockWorkerThread(aRv);

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
  mDatabase->OnVersionChange(aOldVersion, aNewVersion);

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

  IndexedDBTransactionWorkerChild* actor =
    static_cast<IndexedDBTransactionWorkerChild*>(aActor);
  MOZ_ASSERT(!actor->GetTransaction());

  MOZ_ASSERT(aParams.type() ==
             TransactionParams::TVersionChangeTransactionParams);

  const VersionChangeTransactionParams& params =
    aParams.get_VersionChangeTransactionParams();

  const DatabaseInfoGuts& dbInfo = params.dbInfo();
  const InfallibleTArray<ObjectStoreInfoGuts>& osInfo = params.osInfo();
  uint64_t oldVersion = params.oldVersion();

  MOZ_ASSERT(dbInfo.origin == mDatabase->mFactory->GetASCIIOrigin());
  MOZ_ASSERT(dbInfo.name == mDatabase->mName);
  MOZ_ASSERT(!mDatabase->mVersion || dbInfo.version == mDatabase->mVersion);
  MOZ_ASSERT(!mDatabase->mVersion || oldVersion < mDatabase->mVersion);

  if (!EnsureDatabaseInfo(dbInfo, osInfo)) {
    return false;
  }

  mDatabase->mTransaction->SetDBInfo(mDatabase->mDatabaseInfo);

  mDatabase->EnterSetVersionTransaction();
  mDatabase->mPreviousDatabaseInfo->version = oldVersion;
  mDatabase->mUpgradeNeeded = true;

  actor->SetTransaction(mDatabase->mTransaction);

  mDatabase->UnblockWorkerThread(NS_OK);

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
: mTransaction(NULL)
{
  MOZ_COUNT_CTOR(IndexedDBTransactionWorkerChild);
}

IndexedDBTransactionWorkerChild::~IndexedDBTransactionWorkerChild()
{
  MOZ_COUNT_DTOR(IndexedDBTransactionWorkerChild);
}

void
IndexedDBTransactionWorkerChild::SetTransaction(
                                               IDBTransactionSync* aTransaction)
{
  MOZ_ASSERT(aTransaction);
  MOZ_ASSERT(!mTransaction);

  aTransaction->SetActor(this);
  mTransaction = aTransaction;
}

void
IndexedDBTransactionWorkerChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mTransaction) {
    mTransaction->SetActor(static_cast<IndexedDBTransactionWorkerChild*>(NULL));
#ifdef DEBUG
    mTransaction = NULL;
#endif
  }
}

bool
IndexedDBTransactionWorkerChild::RecvComplete(const CompleteParams& aParams)
{
  MOZ_ASSERT(mTransaction);

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

  if (mTransaction->GetMode() == IDBTransactionBase::VERSION_CHANGE) {
    mTransaction->Db()->ExitSetVersionTransaction();

    if (NS_FAILED(resultCode)) {
      // This will make the database take a snapshot of it's DatabaseInfo
      ErrorResult rv;
      mTransaction->Db()->Close(nullptr, rv);
      // Then remove the info from the hash as it contains invalid data.
      DatabaseInfoSync::Remove(mTransaction->Db()->Id());
    }
  }
  else {
    mTransaction->UnblockWorkerThread(NS_OK);
  }

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
: mObjectStore(NULL)
{
  MOZ_COUNT_CTOR(IndexedDBObjectStoreWorkerChild);
}

IndexedDBObjectStoreWorkerChild::~IndexedDBObjectStoreWorkerChild()
{
  MOZ_COUNT_DTOR(IndexedDBObjectStoreWorkerChild);
  MOZ_ASSERT(!mObjectStore);
}

void
IndexedDBObjectStoreWorkerChild::SetObjectStore(IDBObjectStoreSync* aObjectStore)
{
  MOZ_ASSERT(aObjectStore);
  MOZ_ASSERT(!mObjectStore);

  aObjectStore->SetActor(this);
  mObjectStore = aObjectStore;
}

void
IndexedDBObjectStoreWorkerChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mObjectStore) {
    mObjectStore->SetActor(static_cast<IndexedDBObjectStoreWorkerChild*>(NULL));
#ifdef DEBUG
    mObjectStore = NULL;
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
  NS_ASSERTION(requestActor, "Must have an actor here!");

  IDBCursorSync* cursor =
    static_cast<IDBCursorSync*>(requestActor->GetObject());
  NS_ASSERTION(cursor, "Must have an object here!");

//  NS_ASSERTION(static_cast<size_t>(aParams.direction()) ==
//               cursor->GetDirection(), "Huh?");

  Key emptyKey;
  cursor->SetCurrentKeysAndValue(aParams.key(), emptyKey, aParams.cloneInfo());

  actor->SetCursor(cursor);
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
: mIndex(NULL)
{
  MOZ_COUNT_CTOR(IndexedDBIndexWorkerChild);
}

IndexedDBIndexWorkerChild::~IndexedDBIndexWorkerChild()
{
  MOZ_COUNT_DTOR(IndexedDBIndexWorkerChild);
  MOZ_ASSERT(!mIndex);
}

void
IndexedDBIndexWorkerChild::SetIndex(IDBIndexSync* aIndex)
{
  MOZ_ASSERT(aIndex);
  MOZ_ASSERT(!mIndex);

  aIndex->SetActor(this);
  mIndex = aIndex;
}

void
IndexedDBIndexWorkerChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mIndex) {
    mIndex->SetActor(static_cast<IndexedDBIndexWorkerChild*>(NULL));
#ifdef DEBUG
    mIndex = NULL;
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
  NS_ASSERTION(requestActor, "Must have an actor here!");

  IDBCursorSync* cursor =
    static_cast<IDBCursorSync*>(requestActor->GetObject());
  NS_ASSERTION(cursor, "Must have an object here!");

//  NS_ASSERTION(static_cast<size_t>(aParams.direction()) ==
//               cursor->GetDirection(), "Huh?");

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

  actor->SetCursor(cursor);
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
: mCursor(NULL)
{
  MOZ_COUNT_CTOR(IndexedDBCursorWorkerChild);
}

IndexedDBCursorWorkerChild::~IndexedDBCursorWorkerChild()
{
  MOZ_COUNT_DTOR(IndexedDBCursorWorkerChild);
  MOZ_ASSERT(!mCursor);
}

void
IndexedDBCursorWorkerChild::SetCursor(IDBCursorSync* aCursor)
{
  MOZ_ASSERT(aCursor);
  MOZ_ASSERT(!mCursor);

  aCursor->SetActor(this);
  mCursor = aCursor;
}

void
IndexedDBCursorWorkerChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mCursor) {
    mCursor->SetActor(static_cast<IndexedDBCursorWorkerChild*>(NULL));
#ifdef DEBUG
    mCursor = NULL;
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
: mHelper(NULL)
{
  MOZ_COUNT_CTOR(IndexedDBRequestWorkerChildBase);
}

IndexedDBRequestWorkerChildBase::~IndexedDBRequestWorkerChildBase()
{
  MOZ_COUNT_DTOR(IndexedDBRequestWorkerChildBase);
}

void
IndexedDBRequestWorkerChildBase::SetHelper(BlockingHelperBase* aHelper)
{
  MOZ_ASSERT(aHelper);
  MOZ_ASSERT(!mHelper);

  aHelper->SetActor(this);
  mHelper = aHelper;
}

IDBObjectSync*
IndexedDBRequestWorkerChildBase::GetObject() const
{
  return mHelper->Object();
}

void
IndexedDBRequestWorkerChildBase::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mHelper) {
    mHelper->SetActor(static_cast<IndexedDBRequestWorkerChildBase*>(NULL));
#ifdef DEBUG
    mHelper = NULL;
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
  switch (aResponse.type()) {
    case ResponseValue::Tnsresult:
      break;
    case ResponseValue::TGetResponse:
      MOZ_ASSERT(mRequestType == ParamsUnionType::TGetParams);
      break;
    case ResponseValue::TGetAllResponse:
      MOZ_ASSERT(mRequestType == ParamsUnionType::TGetAllParams);
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
      MOZ_ASSERT(mRequestType == ParamsUnionType::TOpenCursorParams);
      break;

    default:
      MOZ_CRASH("Received invalid response parameters!");
      return false;
  }

  mHelper->OnRequestComplete(aResponse);

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

  mHelper->OnRequestComplete(aResponse);

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

  mHelper->OnRequestComplete(aResponse);

  return true;
}

/*******************************************************************************
 * IndexedDBDeleteDatabaseRequestWorkerChild
 ******************************************************************************/

IndexedDBDeleteDatabaseRequestWorkerChild::IndexedDBDeleteDatabaseRequestWorkerChild(
                                                  const nsACString& aDatabaseId)
: mHelper(NULL), mDatabaseId(aDatabaseId)
{
  MOZ_COUNT_CTOR(IndexedDBDeleteDatabaseRequestWorkerChild);
  MOZ_ASSERT(!aDatabaseId.IsEmpty());
}

IndexedDBDeleteDatabaseRequestWorkerChild::~IndexedDBDeleteDatabaseRequestWorkerChild()
{
  MOZ_COUNT_DTOR(IndexedDBDeleteDatabaseRequestWorkerChild);
}

void
IndexedDBDeleteDatabaseRequestWorkerChild::SetHelper(DeleteDatabaseHelper* aHelper)
{
  MOZ_ASSERT(aHelper);
  MOZ_ASSERT(!mHelper);

  aHelper->SetActor(this);
  mHelper = aHelper;
}

void
IndexedDBDeleteDatabaseRequestWorkerChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mHelper) {
    mHelper->SetActor(static_cast<IndexedDBDeleteDatabaseRequestWorkerChild*>(NULL));
#ifdef DEBUG
    mHelper = NULL;
#endif
  }
}

bool
IndexedDBDeleteDatabaseRequestWorkerChild::Recv__delete__(const nsresult& aRv)
{
  if (NS_SUCCEEDED(aRv)) {
    DatabaseInfoSync::Remove(mDatabaseId);
  }

  mHelper->OnRequestComplete(aRv);

  return true;
}

bool
IndexedDBDeleteDatabaseRequestWorkerChild::RecvBlocked(
                                                const uint64_t& aCurrentVersion)
{
  return true;
}
