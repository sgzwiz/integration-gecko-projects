/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbtransactionbase_h__
#define mozilla_dom_indexeddb_idbtransactionbase_h__

#include "mozilla/dom/indexedDB/IndexedDatabase.h"

#include "mozilla/dom/IDBTransactionBinding.h"

BEGIN_INDEXEDDB_NAMESPACE

class IDBTransactionBase
{
public:
  enum Mode
  {
    READ_ONLY = 0,
    READ_WRITE,
    VERSION_CHANGE,

    // Only needed for IPC serialization helper, should never be used in code.
    MODE_INVALID
  };

  bool IsAborted() const
  {
    return NS_FAILED(mAbortCode);
  }

  bool IsWriteAllowed() const
  {
    return mMode == READ_WRITE || mMode == VERSION_CHANGE;
  }

  IDBTransactionMode
  GetMode(ErrorResult& aRv) const;

  nsresult
  GetAbortCode() const
  {
    return mAbortCode;
  }

protected:
  IDBTransactionBase()
  : mMode(READ_ONLY), mAbortCode(NS_OK)
  { }

  virtual ~IDBTransactionBase()
  { }

  Mode mMode;
  nsTArray<nsString> mObjectStoreNames;
  nsresult mAbortCode;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_idbtransactionbase_h__
