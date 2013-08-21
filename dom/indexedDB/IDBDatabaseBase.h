/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbdatabasebase_h__
#define mozilla_dom_indexeddb_idbdatabasebase_h__

#include "mozilla/dom/indexedDB/IndexedDatabase.h"

BEGIN_INDEXEDDB_NAMESPACE

class IDBDatabaseBase
{
public:
  const nsString& Name() const
  {
    return mName;
  }

  bool IsClosed() const
  {
    return mClosed;
  }

protected:
  IDBDatabaseBase();
  virtual ~IDBDatabaseBase();

  nsString mName;
  bool mClosed;
  bool mRunningVersionChange;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_idbdatabasebase_h__
