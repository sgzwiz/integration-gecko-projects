/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBKeyRange.h"

#include "DOMBindingInlines.h"

USING_WORKERS_NAMESPACE
using mozilla::ErrorResult;
using mozilla::dom::indexedDB::Key;
using mozilla::dom::GlobalObject;

namespace {

inline
nsresult
GetKeyFromJSVal(JSContext* aCx,
                jsval aVal,
                Key& aKey)
{
  nsresult rv = aKey.SetFromJSVal(aCx, aVal);
  if (NS_FAILED(rv)) {
    NS_ASSERTION(NS_ERROR_GET_MODULE(rv) == NS_ERROR_MODULE_DOM_INDEXEDDB,
                 "Bad error code!");
    return rv;
  }

  if (aKey.IsUnset()) {
    return NS_ERROR_DOM_INDEXEDDB_DATA_ERR;
  }

  return NS_OK;
}

}

NS_IMPL_ISUPPORTS_INHERITED0(mozilla::dom::workers::IDBKeyRange, DOMBindingBase)

// static
nsresult
IDBKeyRange::FromJSVal(JSContext* aCx, const jsval& aVal,
                       IDBKeyRange** aKeyRange)
{
  nsRefPtr<IDBKeyRange> keyRange;

  if (aVal.isNullOrUndefined()) {
    // undefined and null returns no IDBKeyRange.
    keyRange.forget(aKeyRange);
    return NS_OK;
  }

  JS::RootedObject obj(aCx, aVal.isObject() ? &aVal.toObject() : NULL);
  if (aVal.isPrimitive() || JS_IsArrayObject(aCx, obj) ||
      JS_ObjectIsDate(aCx, obj)) {
    // A valid key returns an 'only' IDBKeyRange.
    keyRange = new IDBKeyRange(aCx, false, false, true);

    nsresult rv = GetKeyFromJSVal(aCx, aVal, keyRange->Lower());
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  else {
    MOZ_ASSERT(aVal.isObject());
    // An object is not permitted unless it's another IDBKeyRange.
    if (NS_FAILED(UNWRAP_WORKER_OBJECT(IDBKeyRange, aCx, obj, keyRange))) {
      return NS_ERROR_DOM_INDEXEDDB_DATA_ERR;
    }
  }

  keyRange.forget(aKeyRange);
  return NS_OK;
}

void
IDBKeyRange::_trace(JSTracer* aTrc)
{
  JS_CallHeapValueTracer(aTrc, &mCachedLowerVal, "mCachedLowerVal");
  JS_CallHeapValueTracer(aTrc, &mCachedUpperVal, "mCachedUpperVal");

  DOMBindingBase::_trace(aTrc);
}

void
IDBKeyRange::_finalize(JSFreeOp* aFop)
{
  DOMBindingBase::_finalize(aFop);
}

JS::Value
IDBKeyRange::GetLower(JSContext* aCx, ErrorResult& aRv)
{
  if (!mHaveCachedLowerVal) {
    nsresult rv = Lower().ToJSVal(aCx, mCachedLowerVal);
    NS_ENSURE_SUCCESS(rv, JSVAL_NULL);

    mHaveCachedLowerVal = true;
  }

  return mCachedLowerVal;
}

JS::Value
IDBKeyRange::GetUpper(JSContext* aCx, ErrorResult& aRv)
{
  if (!mHaveCachedUpperVal) {
    nsresult rv = Upper().ToJSVal(aCx, mCachedUpperVal);
    NS_ENSURE_SUCCESS(rv, JSVAL_NULL);

    mHaveCachedUpperVal = true;
  }

  return mCachedUpperVal;
}

// static
IDBKeyRange*
IDBKeyRange::Only(const GlobalObject& aGlobal, const JS::Value& aValue,
                  ErrorResult& aRv)
{
  JSContext* cx = aGlobal.GetContext();

  nsRefPtr<IDBKeyRange> keyRange = new IDBKeyRange(cx, false, false, true);

  nsresult rv = GetKeyFromJSVal(cx, aValue, keyRange->Lower());
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  if (!Wrap(cx, aGlobal.Get(), keyRange)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return keyRange;
}

// static
IDBKeyRange*
IDBKeyRange::LowerBound(const GlobalObject& aGlobal,
                        const JS::Value& aValue, bool aOpen, ErrorResult& aRv)
{
  JSContext* cx = aGlobal.GetContext();

  nsRefPtr<IDBKeyRange> keyRange = new IDBKeyRange(cx, aOpen, true, false);

  nsresult rv = GetKeyFromJSVal(cx, aValue, keyRange->Lower());
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  if (!Wrap(cx, aGlobal.Get(), keyRange)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return keyRange;
}

// static
IDBKeyRange*
IDBKeyRange::UpperBound(const GlobalObject& aGlobal,
                        const JS::Value& aValue, bool aOpen, ErrorResult& aRv)
{
  JSContext* cx = aGlobal.GetContext();

  nsRefPtr<IDBKeyRange> keyRange = new IDBKeyRange(cx, true, aOpen, false);

  nsresult rv = GetKeyFromJSVal(cx, aValue, keyRange->Upper());
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  if (!Wrap(cx, aGlobal.Get(), keyRange)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return keyRange;
}

// static
IDBKeyRange*
IDBKeyRange::Bound(const GlobalObject& aGlobal,
                   const JS::Value& aLower, const JS::Value& aUpper,
                   bool aLowerOpen, bool aUpperOpen, ErrorResult& aRv)
{
  JSContext* cx = aGlobal.GetContext();

  nsRefPtr<IDBKeyRange> keyRange =
    new IDBKeyRange(cx, aLowerOpen, aUpperOpen, false);

  nsresult rv = GetKeyFromJSVal(cx, aLower, keyRange->Lower());
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  rv = GetKeyFromJSVal(cx, aUpper, keyRange->Upper());
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  if (keyRange->Lower() > keyRange->Upper() ||
      (keyRange->Lower() == keyRange->Upper() && (aLowerOpen || aUpperOpen))) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
    return nullptr;
  }

  if (!Wrap(cx, aGlobal.Get(), keyRange)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return keyRange;
}
