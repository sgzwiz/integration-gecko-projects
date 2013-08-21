/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_domstringlist_h__
#define mozilla_dom_workers_domstringlist_h__

#include "Workers.h"
#include "mozilla/dom/workers/bindings/DOMBindingBase.h"

#include "mozilla/dom/BindingUtils.h"

BEGIN_WORKERS_NAMESPACE

class DOMStringList MOZ_FINAL : public DOMBindingBase
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  static DOMStringList*
  Create(JSContext* aCx);

  static DOMStringList*
  Create(JSContext* aCx, const nsTArray<nsString>& aStrings);

  virtual void
  _trace(JSTracer* aTrc) MOZ_OVERRIDE;

  virtual void
  _finalize(JSFreeOp* aFop) MOZ_OVERRIDE;

  void
  Add(const nsAString& aString)
  {
    mStrings.AppendElement(aString);
  }

  // WebIDL
  uint32_t
  Length()
  {
    return mStrings.Length();
  }

  void
  Item(uint32_t aIndex, nsString& aResult);

  bool
  Contains(const nsAString& aString)
  {
    return mStrings.Contains(aString);
  }

private:
  DOMStringList(JSContext* aCx)
  : DOMBindingBase(aCx)
  { }

  DOMStringList(JSContext* aCx, const nsTArray<nsString>& aStrings)
  : DOMBindingBase(aCx), mStrings(aStrings)
  { }

  nsTArray<nsString> mStrings;
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_domstringlist_h__
