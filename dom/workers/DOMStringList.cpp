/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMStringList.h"

#include "DOMBindingInlines.h"

USING_WORKERS_NAMESPACE
using mozilla::ErrorResult;
using mozilla::dom::NonNull;

// static
DOMStringList*
DOMStringList::Create(JSContext* aCx)
{
  nsRefPtr<DOMStringList> stringList = new DOMStringList(aCx);

  if (!Wrap(aCx, nullptr, stringList)) {
    return nullptr;
  }

  return stringList;
}

// static
DOMStringList*
DOMStringList::Create(JSContext* aCx, const nsTArray<nsString>& aStrings)
{
  nsRefPtr<DOMStringList> stringList =
    new DOMStringList(aCx, aStrings);

  if (!Wrap(aCx, nullptr, stringList)) {
    return nullptr;
  }

  return stringList;
}

void
DOMStringList::_trace(JSTracer* aTrc)
{
  DOMBindingBase::_trace(aTrc);
}

void
DOMStringList::_finalize(JSFreeOp* aFop)
{
  DOMBindingBase::_finalize(aFop);
}

NS_IMPL_ISUPPORTS_INHERITED0(DOMStringList, DOMBindingBase)

void
DOMStringList::Item(uint32_t aIndex, nsString& aResult)
{
  if (aIndex >= mStrings.Length()) {
    SetDOMStringToNull(aResult);
  }
  else {
    aResult = mStrings[aIndex];
  }
}
