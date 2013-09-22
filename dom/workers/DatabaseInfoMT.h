/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_databaseinfomt_h__
#define mozilla_dom_workers_databaseinfomt_h__

#include "Workers.h"

#include "mozilla/dom/indexedDB/DatabaseInfo.h"
#include "mozilla/StaticMutex.h"

namespace mozilla {
namespace dom {
namespace indexedDB {
struct ObjectStoreInfo;
} // namespace indexedDB
} // namespace dom
} // namespace mozilla

BEGIN_WORKERS_NAMESPACE

struct DatabaseInfoMT : public indexedDB::DatabaseInfoBase
{
  typedef mozilla::dom::indexedDB::ObjectStoreInfo ObjectStoreInfo;

  DatabaseInfoMT();

  ~DatabaseInfoMT();

  static bool Get(nsCString& aId, DatabaseInfoMT** aInfo);

  static bool Put(DatabaseInfoMT* aInfo);

  static void Remove(const nsACString& aId);

  bool GetObjectStoreNames(nsTArray<nsString>& aNames);

  bool ContainsStoreName(const nsAString& aName);

  ObjectStoreInfo* GetObjectStore(const nsAString& aName);

  bool PutObjectStore(ObjectStoreInfo* aInfo);

  void RemoveObjectStore(const nsAString& aName);

  already_AddRefed<DatabaseInfoMT> Clone();

  nsCString id;

  static StaticMutex sDatabaseInfoMutex;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DatabaseInfoMT)
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_databaseinfomt_h__
