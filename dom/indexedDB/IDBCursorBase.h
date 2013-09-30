/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbcursorbase_h__
#define mozilla_dom_indexeddb_idbcursorbase_h__

#include "mozilla/dom/indexedDB/IndexedDatabase.h"

#include "mozilla/dom/IDBCursorBinding.h"

#include "mozilla/dom/indexedDB/Key.h"

BEGIN_INDEXEDDB_NAMESPACE

class IDBCursorBase
{
public:
  enum Type
  {
    OBJECTSTORE = 0,
    OBJECTSTOREKEY,
    INDEXKEY,
    INDEXOBJECT
  };

  enum Direction
  {
    NEXT = 0,
    NEXT_UNIQUE,
    PREV,
    PREV_UNIQUE,

    // Only needed for IPC serialization helper, should never be used in code.
    DIRECTION_INVALID
  };

  static Direction
  ConvertDirection(IDBCursorDirection aDirection);

  IDBCursorDirection
  GetDirection(ErrorResult& aRv) const;

protected:
  IDBCursorBase()
  : mType(OBJECTSTORE), mDirection(NEXT), mCachedKey(JSVAL_VOID),
    mCachedPrimaryKey(JSVAL_VOID), mCachedValue(JSVAL_VOID),
    mHaveCachedKey(false), mHaveCachedPrimaryKey(false), mHaveCachedValue(false)
  { }

  virtual ~IDBCursorBase()
  { }

  Type mType;
  Direction mDirection;

  JS::Heap<JS::Value> mCachedKey;
  JS::Heap<JS::Value> mCachedPrimaryKey;
  JS::Heap<JS::Value> mCachedValue;

  Key mKey;
  Key mObjectKey;

  bool mHaveCachedKey;
  bool mHaveCachedPrimaryKey;
  bool mHaveCachedValue;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_idbcursorbase_h__
