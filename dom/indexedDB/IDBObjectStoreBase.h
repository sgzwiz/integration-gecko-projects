/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbobjectstorebase_h__
#define mozilla_dom_indexeddb_idbobjectstorebase_h__

#include "mozilla/dom/indexedDB/KeyPath.h"

BEGIN_INDEXEDDB_NAMESPACE

struct IndexUpdateInfo;
struct ObjectStoreInfo;

class IDBObjectStoreBase
{
public:
  static nsresult
  AppendIndexUpdateInfo(int64_t aIndexID,
                        const KeyPath& aKeyPath,
                        bool aUnique,
                        bool aMultiEntry,
                        JSContext* aCx,
                        JS::Handle<JS::Value> aObject,
                        nsTArray<IndexUpdateInfo>& aUpdateInfoArray);

  bool
  InfoContainsIndexName(const nsAString& aName);

  const bool HasValidKeyPath() const
  {
    return mKeyPath.IsValid();
  }

  int64_t Id() const
  {
    NS_ASSERTION(mId != INT64_MIN, "Don't ask for this yet!");
    return mId;
  }

  ObjectStoreInfo* Info()
  {
    return mInfo;
  }

  bool IsAutoIncrement() const
  {
    return mAutoIncrement;
  }

  const nsString& Name() const
  {
    return mName;
  }

  void
  SetInfo(ObjectStoreInfo* aInfo);

protected:
  IDBObjectStoreBase();
  virtual ~IDBObjectStoreBase();

  int64_t mId;
  nsString mName;
  KeyPath mKeyPath;
  JS::Heap<JS::Value> mCachedKeyPath;
  bool mAutoIncrement;
  nsRefPtr<ObjectStoreInfo> mInfo;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_idbobjectstorebase_h__
