/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBCursorSync.h"

#include "mozilla/dom/indexedDB/IndexedDatabaseInlines.h"
#include "mozilla/dom/indexedDB/Key.h"

#include "BlockingHelperBase.h"
#include "DatabaseInfoSync.h"
#include "DOMBindingInlines.h"
#include "IDBIndexSync.h"
#include "IDBKeyRange.h"
#include "IDBObjectStoreSync.h"
#include "IPCThreadUtils.h"
#include "WorkerPrivate.h"

#include "ipc/IndexedDBWorkerChild.h"

USING_WORKERS_NAMESPACE
using namespace mozilla::dom::indexedDB::ipc;
using mozilla::dom::Optional;
using mozilla::ErrorResult;

BEGIN_WORKERS_NAMESPACE

class OpenHelper : public BlockingHelperBase
{
  typedef mozilla::dom::workers::IDBKeyRange IDBKeyRange;

public:
  OpenHelper(WorkerPrivate* aWorkerPrivate, uint32_t aSyncQueueKey,
             IDBCursorSync* aCursor, IDBKeyRange* aKeyRange)
  : BlockingHelperBase(aWorkerPrivate, aCursor), mSyncQueueKey(aSyncQueueKey),
    mCursor(aCursor), mKeyRange(aKeyRange)
  { }

  virtual nsresult
  HandleResponse(const ResponseValue& aResponseValue);

protected:
  nsresult
  IPCThreadRun()
  {
    NS_ASSERTION(mPrimarySyncQueueKey == UINT32_MAX, "Should be unset!");
    mPrimarySyncQueueKey = mSyncQueueKey;

    IndexRequestParams params;

    OpenKeyCursorParams openKeyCursorParams;
    if (mKeyRange) {
      KeyRange keyRange;
      mKeyRange->ToSerializedKeyRange(keyRange);
      openKeyCursorParams.optionalKeyRange() = keyRange;
    }
    else {
      openKeyCursorParams.optionalKeyRange() = mozilla::void_t();
    }
    openKeyCursorParams.direction() = mCursor->GetDirection();

    params = openKeyCursorParams;

    IndexedDBIndexRequestWorkerChild* actor =
      new IndexedDBIndexRequestWorkerChild(params.type());
    mCursor->Index()->GetActor()->SendPIndexedDBRequestConstructor(actor,
                                                                   params);
    actor->SetHelper(this);

    return NS_OK;
  }

private:
  uint32_t mSyncQueueKey;
  IDBCursorSync* mCursor;

  // In-params.
  nsRefPtr<IDBKeyRange> mKeyRange;
};

class ContinueHelper : public BlockingHelperBase
{
public:
  ContinueHelper(WorkerPrivate* aWorkerPrivate, uint32_t aSyncQueueKey,
                 IDBCursorSync* aCursor, int32_t aCount)
  : BlockingHelperBase(aWorkerPrivate, aCursor), mSyncQueueKey(aSyncQueueKey),
    mCursor(aCursor), mCount(aCount)
  {
    NS_ASSERTION(aCount > 0, "Must have a count!");
  }

  virtual nsresult
  HandleResponse(const ResponseValue& aResponseValue);

protected:
  nsresult
  IPCThreadRun()
  {
    NS_ASSERTION(mPrimarySyncQueueKey == UINT32_MAX, "Should be unset!");
    mPrimarySyncQueueKey = mSyncQueueKey;

    CursorRequestParams params;

    ContinueParams continueParams;
    continueParams.key() = mCursor->mContinueToKey;
    continueParams.count() = uint32_t(mCount);

    params = continueParams;

    IndexedDBCursorRequestWorkerChild* actor =
      new IndexedDBCursorRequestWorkerChild(params.type());
    mCursor->GetActor()->SendPIndexedDBRequestConstructor(actor, params);
    actor->SetHelper(this);

    return NS_OK;
  }

private:
  uint32_t mSyncQueueKey;
  IDBCursorSync* mCursor;

  // In-params.
  int32_t mCount;
};

END_WORKERS_NAMESPACE

// For INDEXKEY cursors.
// static
IDBCursorSync*
IDBCursorSync::Create(JSContext* aCx, IDBIndexSync* aIndex,
                      Direction aDirection)
{
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);

  nsRefPtr<IDBCursorSync> cursor = new IDBCursorSync(aCx, workerPrivate);

  cursor->mIndex = aIndex;
  cursor->mTransaction = aIndex->ObjectStore()->Transaction();
  cursor->mType = INDEXKEY;
  cursor->mDirection = aDirection;

  if (!Wrap(aCx, nullptr, cursor)) {
    return nullptr;
  }

  return cursor;
}

IDBCursorSync::IDBCursorSync(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
: IDBObjectSync(aCx, aWorkerPrivate), mObjectStore(nullptr), mIndex(nullptr),
  mHaveValue(true), mActorChild(nullptr)
{
}

IDBCursorSync::~IDBCursorSync()
{
  // TODO: Clear mCloneReadInfo
}

void
IDBCursorSync::_trace(JSTracer* aTrc)
{
  JS_CallHeapValueTracer(aTrc, &mCachedKey, "mCachedKey");
  JS_CallHeapValueTracer(aTrc, &mCachedPrimaryKey, "mCachedPrimaryKey");

  IDBObjectSync::_trace(aTrc);
}

void
IDBCursorSync::_finalize(JSFreeOp* aFop)
{
  IDBObjectSync::_finalize(aFop);
}

void
IDBCursorSync::ReleaseIPCThreadObjects()
{
  AssertIsOnIPCThread();

  if (mActorChild) {
    mActorChild->Send__delete__(mActorChild);
    NS_ASSERTION(!mActorChild, "Should have cleared in Send__delete__!");
  }
}

NS_IMPL_ISUPPORTS_INHERITED0(IDBCursorSync, IDBObjectSync)

JSObject*
IDBCursorSync::Source(JSContext* aCx)
{
  return mType == OBJECTSTORE ? mObjectStore->GetJSObject() :
                                mIndex->GetJSObject();
}

JS::Value
IDBCursorSync::Key(JSContext* aCx)
{
  NS_ASSERTION(!mKey.IsUnset() || !mHaveValue, "Bad key!");

  if (!mHaveValue) {
    return JSVAL_VOID;
  }

  if (!mHaveCachedKey) {
    nsresult rv = mKey.ToJSVal(aCx, mCachedKey);
    NS_ENSURE_SUCCESS(rv, JSVAL_NULL);

    mHaveCachedKey = true;
  }

  return mCachedKey;
}

JS::Value
IDBCursorSync::PrimaryKey(JSContext* aCx)
{
  if (!mHaveValue) {
    return JSVAL_VOID;
  }

  if (!mHaveCachedPrimaryKey) {
    NS_ASSERTION(mType == OBJECTSTORE ? !mKey.IsUnset() :
                                        !mObjectKey.IsUnset(), "Bad key!");

    const indexedDB::Key& key = mType == OBJECTSTORE ? mKey : mObjectKey;

    nsresult rv = key.ToJSVal(aCx, mCachedPrimaryKey);
    NS_ENSURE_SUCCESS(rv, JSVAL_NULL);

    mHaveCachedPrimaryKey = true;
  }

  return mCachedPrimaryKey;
}

void
IDBCursorSync::Update(JSContext* aCx, JS::Value aValue, ErrorResult& aRv)
{
  if (mTransaction->IsInvalid()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return;
  }

  if (!mTransaction->IsWriteAllowed()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_READ_ONLY_ERR);
    return;
  }

  if (!mHaveValue || mType == INDEXKEY) {
    NS_WARNING("Dont have value or bad type!");
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return;
  }

  NS_ASSERTION(mObjectStore, "This cannot be null!");
  NS_ASSERTION(!mKey.IsUnset() , "Bad key!");
  NS_ASSERTION(mType != INDEXOBJECT || !mObjectKey.IsUnset(), "Bad key!");

  nsresult rv;

  JSAutoRequest ar(aCx);

  indexedDB::Key& objectKey = (mType == OBJECTSTORE) ? mKey : mObjectKey;

  if (mObjectStore->HasValidKeyPath()) {
    // Make sure the object given has the correct keyPath value set on it.
    const indexedDB::KeyPath& keyPath = mObjectStore->GetKeyPath();
    indexedDB::Key key;

    rv = keyPath.ExtractKey(aCx, aValue, key);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return;
    }

    if (key != objectKey) {
      NS_WARNING("key != objectKey");
      aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
      return;
    }

    Optional<JS::Handle<JS::Value> > emptyKey;
    mObjectStore->Put(aCx, aValue, emptyKey, aRv);

    return;
  }

  JS::Rooted<JS::Value> key(aCx);
  rv = objectKey.ToJSVal(aCx, &key);
  if (NS_FAILED(rv)) {
    NS_WARNING("ObjectKey.ToJSVal failed!");
    aRv.Throw(rv);
    return;
  }

  Optional<JS::Handle<JS::Value> > optionalKey;
  optionalKey.Construct(aCx, key);

  mObjectStore->Put(aCx, aValue, optionalKey, aRv);
}

bool
IDBCursorSync::Advance(JSContext* aCx, uint32_t aCount, ErrorResult& aRv)
{
  if (aCount < 1 || aCount > UINT32_MAX) {
    aRv.Throw(NS_ERROR_TYPE_ERR);
    return false;
  }

  indexedDB::Key key;
  return ContinueInternal(aCx, key, aCount, aRv);
}

bool
IDBCursorSync::Continue(JSContext* aCx,
                        const Optional<JS::Handle<JS::Value> >& aKey,
                        ErrorResult& aRv)
{
  indexedDB::Key key;
  if (aKey.WasPassed()) {
    nsresult rv = key.SetFromJSVal(aCx, aKey.Value());
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return false;
    }
  }

  if (!key.IsUnset()) {
    switch (mDirection) {
      case IDBCursorBase::NEXT:
      case IDBCursorBase::NEXT_UNIQUE:
        if (key <= mKey) {
          aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
          return false;
        }
        break;

      case IDBCursorBase::PREV:
      case IDBCursorBase::PREV_UNIQUE:
        if (key >= mKey) {
          aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
          return false;
        }
        break;

      default:
        NS_NOTREACHED("Unknown direction type!");
    }
  }

  return ContinueInternal(aCx, key, 1, aRv);
}

bool
IDBCursorSync::Delete(JSContext* aCx, ErrorResult& aRv)
{
  if (mTransaction->IsInvalid()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return false;
  }

  if (!mTransaction->IsWriteAllowed()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_READ_ONLY_ERR);
    return false;
  }

  if (!mHaveValue || mType == INDEXKEY) {
    NS_WARNING("Dont have value or bad type!");
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return false;
  }

  NS_ASSERTION(mObjectStore, "This cannot be null!");
  NS_ASSERTION(!mKey.IsUnset() , "Bad key!");

  indexedDB::Key& objectKey = (mType == OBJECTSTORE) ? mKey : mObjectKey;

  JS::Rooted<JS::Value> key(aCx);
  nsresult rv = objectKey.ToJSVal(aCx, &key);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return false;
  }

  mObjectStore->Delete(aCx, key, aRv);
  return !aRv.Failed();
}

bool
IDBCursorSync::Open(JSContext* aCx, IDBKeyRange* aKeyRange)
{
  DOMBindingAnchor<IDBCursorSync> selfAnchor(this);

  AutoSyncLoopHolder syncLoop(mWorkerPrivate);

  nsRefPtr<OpenHelper> helper =
    new OpenHelper(mWorkerPrivate, syncLoop.SyncQueueKey(), this, aKeyRange);

  if (!helper->Dispatch(aCx) || !syncLoop.RunAndForget(aCx)) {
    return false;
  }

  return true;
}

bool
IDBCursorSync::ContinueInternal(JSContext* aCx, 
                                const mozilla::dom::indexedDB::Key& aKey,
                                int32_t aCount,
                                ErrorResult& aRv)
{
  NS_ASSERTION(aCount > 0, "Must have a count!");

  if (mTransaction->IsInvalid()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return false;
  }

  if (!mHaveValue) {
    NS_WARNING("Dont have value!");
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return false;
  }

  mContinueToKey = aKey;

  DOMBindingAnchor<IDBCursorSync> selfAnchor(this);

  AutoSyncLoopHolder syncLoop(mWorkerPrivate);

  nsRefPtr<ContinueHelper> helper =
    new ContinueHelper(mWorkerPrivate, syncLoop.SyncQueueKey(), this, aCount);

  if (!helper->Dispatch(aCx) || !syncLoop.RunAndForget(aCx)) {
    NS_WARNING("Failed to distpatch or RunAndForget!");
    return false;
  }

  return mHaveValue;
}

void
IDBCursorSync::SetCurrentKeysAndValue(
                             const mozilla::dom::indexedDB::Key& aKey,
                             const mozilla::dom::indexedDB::Key& aObjectKey,
                             const SerializedStructuredCloneReadInfo aCloneInfo)
{
  mCachedKey = JSVAL_VOID;
  mCachedPrimaryKey = JSVAL_VOID;
  mCachedValue = JSVAL_VOID;
  mHaveCachedKey = false;
  mHaveCachedPrimaryKey = false;
  mHaveCachedValue = false;

  if (aKey.IsUnset()) {
    mHaveValue = false;
  }
  else {
    mKey = aKey;
    mObjectKey = aObjectKey;
    mContinueToKey.Unset();

    if (!mCloneReadInfo.SetFromSerialized(aCloneInfo)) {
      NS_WARNING("Failed to copy clone buffer!");
    }
  }
}

nsresult
OpenHelper::HandleResponse(const ResponseValue& aResponseValue)
{
  AssertIsOnIPCThread();

  NS_ASSERTION(aResponseValue.type() == ResponseValue::TOpenCursorResponse,
               "Bad response type!");
  NS_ASSERTION(aResponseValue.get_OpenCursorResponse().type() ==
               OpenCursorResponse::Tvoid_t ||
               aResponseValue.get_OpenCursorResponse().type() ==
               OpenCursorResponse::TPIndexedDBCursorChild,
               "Bad response union type!");

  const OpenCursorResponse& response =
    aResponseValue.get_OpenCursorResponse();

  switch (response.type()) {
    case OpenCursorResponse::Tvoid_t: {
      indexedDB::Key emptyKey;
      indexedDB::SerializedStructuredCloneReadInfo cloneInfo;
      mCursor->SetCurrentKeysAndValue(emptyKey, emptyKey, cloneInfo);
    } break;

    case OpenCursorResponse::TPIndexedDBCursorChild:
      break;

    default:
      NS_NOTREACHED("Unknown response union type!");
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  return NS_OK;
}

nsresult
ContinueHelper::HandleResponse(const ResponseValue& aResponseValue)
{
  NS_ASSERTION(aResponseValue.type() == ResponseValue::TContinueResponse,
               "Bad response type!");

  const ContinueResponse& response = aResponseValue.get_ContinueResponse();

  mCursor->SetCurrentKeysAndValue(response.key(), response.objectKey(),
                                  response.cloneInfo());

  return NS_OK;
}
