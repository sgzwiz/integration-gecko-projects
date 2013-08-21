/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbindexbase_h__
#define mozilla_dom_indexeddb_idbindexbase_h__

#include "mozilla/dom/indexedDB/KeyPath.h"

BEGIN_INDEXEDDB_NAMESPACE

class IDBIndexBase
{
public:
  const int64_t Id() const
  {
    return mId;
  }

  const nsString& Name() const
  {
    return mName;
  }

  bool IsUnique() const
  {
    return mUnique;
  }

  bool IsMultiEntry() const
  {
    return mMultiEntry;
  }

protected:
  IDBIndexBase()
  : mId(INT64_MIN), mKeyPath(0), mCachedKeyPath(JSVAL_VOID),
    mUnique(false), mMultiEntry(false)
  { }

  virtual ~IDBIndexBase()
  { }

  int64_t mId;
  nsString mName;
  KeyPath mKeyPath;
  JS::Heap<JS::Value> mCachedKeyPath;
  bool mUnique;
  bool mMultiEntry;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_idbindexbase_h__
