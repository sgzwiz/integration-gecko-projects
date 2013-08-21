/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_databaseinfosync_h__
#define mozilla_dom_workers_databaseinfosync_h__

#include "Workers.h"

#include "mozilla/dom/indexedDB/DatabaseInfo.h"

BEGIN_WORKERS_NAMESPACE

struct DatabaseInfoSync : public indexedDB::DatabaseInfoBase
{
  DatabaseInfoSync();

  ~DatabaseInfoSync();

  static bool Get(nsCString& aId, DatabaseInfoSync** aInfo);

  static bool Put(DatabaseInfoSync* aInfo);

  static void Remove(const nsACString& aId);

  already_AddRefed<DatabaseInfoSync> Clone();

  nsCString id;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DatabaseInfoSync)
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_databaseinfosync_h__
