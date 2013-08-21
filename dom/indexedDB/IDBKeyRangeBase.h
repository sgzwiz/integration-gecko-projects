/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbkeyrangebase_h__
#define mozilla_dom_indexeddb_idbkeyrangebase_h__

#include "mozilla/dom/indexedDB/IndexedDatabase.h"

#include "mozilla/dom/indexedDB/Key.h"

BEGIN_INDEXEDDB_NAMESPACE

class IDBKeyRangeBase
{
public:
  const Key& Lower() const
  {
    return mLower;
  }

  Key& Lower()
  {
    return mLower;
  }

  const Key& Upper() const
  {
    return mIsOnly ? mLower : mUpper;
  }

  Key& Upper()
  {
    return mIsOnly ? mLower : mUpper;
  }

  bool IsLowerOpen() const
  {
    return mLowerOpen;
  }

  bool IsUpperOpen() const
  {
    return mUpperOpen;
  }

  bool IsOnly() const
  {
    return mIsOnly;
  }

  template <class T>
  void ToSerializedKeyRange(T& aKeyRange);

protected:
  IDBKeyRangeBase(bool aLowerOpen, bool aUpperOpen, bool aIsOnly)
  : mCachedLowerVal(JSVAL_VOID), mCachedUpperVal(JSVAL_VOID),
    mLowerOpen(aLowerOpen), mUpperOpen(aUpperOpen), mIsOnly(aIsOnly),
    mHaveCachedLowerVal(false), mHaveCachedUpperVal(false)
  { }

  virtual ~IDBKeyRangeBase()
  { }

  Key mLower;
  Key mUpper;
  JS::Heap<JS::Value> mCachedLowerVal;
  JS::Heap<JS::Value> mCachedUpperVal;
  bool mLowerOpen;
  bool mUpperOpen;
  bool mIsOnly;
  bool mHaveCachedLowerVal;
  bool mHaveCachedUpperVal;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_idbkeyrangebase_h__
