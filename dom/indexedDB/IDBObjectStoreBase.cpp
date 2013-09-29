/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBObjectStoreBase.h"
#include "DatabaseInfo.h"

USING_INDEXEDDB_NAMESPACE

IDBObjectStoreBase::IDBObjectStoreBase()
: mId(INT64_MIN), mKeyPath(0), mCachedKeyPath(JSVAL_VOID),
  mAutoIncrement(false)
{ }

IDBObjectStoreBase::~IDBObjectStoreBase()
{ }

// static
nsresult
IDBObjectStoreBase::AppendIndexUpdateInfo(
                                    int64_t aIndexID,
                                    const KeyPath& aKeyPath,
                                    bool aUnique,
                                    bool aMultiEntry,
                                    JSContext* aCx,
                                    JS::Handle<JS::Value> aVal,
                                    nsTArray<IndexUpdateInfo>& aUpdateInfoArray)
{
  nsresult rv;

  if (!aMultiEntry) {
    Key key;
    rv = aKeyPath.ExtractKey(aCx, aVal, key);

    // If an index's keypath doesn't match an object, we ignore that object.
    if (rv == NS_ERROR_DOM_INDEXEDDB_DATA_ERR || key.IsUnset()) {
      return NS_OK;
    }

    if (NS_FAILED(rv)) {
      return rv;
    }

    IndexUpdateInfo* updateInfo = aUpdateInfoArray.AppendElement();
    updateInfo->indexId = aIndexID;
    updateInfo->indexUnique = aUnique;
    updateInfo->value = key;

    return NS_OK;
  }

  JS::Rooted<JS::Value> val(aCx);
  if (NS_FAILED(aKeyPath.ExtractKeyAsJSVal(aCx, aVal, val.address()))) {
    return NS_OK;
  }

  if (!JSVAL_IS_PRIMITIVE(val) &&
      JS_IsArrayObject(aCx, JSVAL_TO_OBJECT(val))) {
    JS::Rooted<JSObject*> array(aCx, JSVAL_TO_OBJECT(val));
    uint32_t arrayLength;
    if (!JS_GetArrayLength(aCx, array, &arrayLength)) {
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    for (uint32_t arrayIndex = 0; arrayIndex < arrayLength; arrayIndex++) {
      JS::Rooted<JS::Value> arrayItem(aCx);
      if (!JS_GetElement(aCx, array, arrayIndex, &arrayItem)) {
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }

      Key value;
      if (NS_FAILED(value.SetFromJSVal(aCx, arrayItem)) ||
          value.IsUnset()) {
        // Not a value we can do anything with, ignore it.
        continue;
      }

      IndexUpdateInfo* updateInfo = aUpdateInfoArray.AppendElement();
      updateInfo->indexId = aIndexID;
      updateInfo->indexUnique = aUnique;
      updateInfo->value = value;
    }
  }
  else {
    Key value;
    if (NS_FAILED(value.SetFromJSVal(aCx, val)) ||
        value.IsUnset()) {
      // Not a value we can do anything with, ignore it.
      return NS_OK;
    }

    IndexUpdateInfo* updateInfo = aUpdateInfoArray.AppendElement();
    updateInfo->indexId = aIndexID;
    updateInfo->indexUnique = aUnique;
    updateInfo->value = value;
  }

  return NS_OK;
}

bool
IDBObjectStoreBase::InfoContainsIndexName(const nsAString &aName)
{
  uint32_t indexCount = mInfo->indexes.Length();
  for (uint32_t index = 0; index < indexCount; index++) {
    if (mInfo->indexes[index].name == aName) {
      return true;
    }
  }

  return false;
}

void
IDBObjectStoreBase::SetInfo(ObjectStoreInfo* aInfo)
{
  MOZ_ASSERT(aInfo != mInfo, "This is nonsense");

  mInfo = aInfo;
}
