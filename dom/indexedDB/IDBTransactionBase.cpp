/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBTransactionBase.h"

USING_INDEXEDDB_NAMESPACE

mozilla::dom::IDBTransactionMode
IDBTransactionBase::GetMode(mozilla::ErrorResult& aRv) const
{
  switch (mMode) {
    case READ_ONLY:
      return mozilla::dom::IDBTransactionMode::Readonly;

    case READ_WRITE:
      return mozilla::dom::IDBTransactionMode::Readwrite;

    case VERSION_CHANGE:
      return mozilla::dom::IDBTransactionMode::Versionchange;

    case MODE_INVALID:
    default:
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return mozilla::dom::IDBTransactionMode::Readonly;
  }
}
