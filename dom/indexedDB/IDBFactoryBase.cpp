/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBFactoryBase.h"

#include "mozilla/ErrorResult.h"

#include "Key.h"

USING_INDEXEDDB_NAMESPACE

int16_t
IDBFactoryBase::Cmp(JSContext* aCx, JS::Handle<JS::Value> aFirst,
                    JS::Handle<JS::Value> aSecond, mozilla::ErrorResult& aRv)
{
  Key first, second;
  nsresult rv = first.SetFromJSVal(aCx, aFirst);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return 0;
  }

  rv = second.SetFromJSVal(aCx, aSecond);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return 0;
  }

  if (first.IsUnset() || second.IsUnset()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
    return 0;
  }

  return Key::CompareKeys(first, second);
}
