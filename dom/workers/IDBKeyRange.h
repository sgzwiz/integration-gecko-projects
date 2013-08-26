/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_idbkeyrange_h__
#define mozilla_dom_workers_idbkeyrange_h__

#include "Workers.h"

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/indexedDB/IDBKeyRangeBase.h"

#include "mozilla/dom/workers/bindings/DOMBindingBase.h"

BEGIN_WORKERS_NAMESPACE

class IDBKeyRange MOZ_FINAL : public DOMBindingBase,
                              public indexedDB::IDBKeyRangeBase
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  static nsresult
  FromJSVal(JSContext* aCx, const jsval& aVal, IDBKeyRange** aKeyRange);

  virtual void
  _trace(JSTracer* aTrc) MOZ_OVERRIDE;

  virtual void
  _finalize(JSFreeOp* aFop) MOZ_OVERRIDE;

  // WebIDL
  JS::Value
  GetLower(JSContext* aCx, ErrorResult& aRv);

  JS::Value
  GetUpper(JSContext* aCx, ErrorResult& aRv);

  bool
  LowerOpen() const
  {
    return IsLowerOpen();
  }

  bool
  UpperOpen() const
  {
    return IsUpperOpen();
  }

  static IDBKeyRange*
  Only(const GlobalObject& aGlobal, const JS::Value& aValue,
       ErrorResult& aRv);

  static IDBKeyRange*
  LowerBound(const GlobalObject& aGlobal, const JS::Value& aValue,
             bool aOpen, ErrorResult& aRv);

  static IDBKeyRange*
  UpperBound(const GlobalObject& aGlobal, const JS::Value& aValue,
             bool aOpen, ErrorResult& aRv);

  static IDBKeyRange*
  Bound(const GlobalObject& aGlobal, const JS::Value& aLower,
        const JS::Value& aUpper, bool aLowerOpen, bool aUpperOpen,
        ErrorResult& aRv);

private:
  IDBKeyRange(JSContext* aCx, bool aLowerOpen, bool aUpperOpen, bool aIsOnly)
  : DOMBindingBase(aCx),
    IDBKeyRangeBase(aLowerOpen, aUpperOpen, aIsOnly)
  { }
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_idbkeyrange_h__
