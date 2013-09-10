/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DatabaseInfoSync.h"

#include "nsDataHashtable.h"

USING_WORKERS_NAMESPACE
using namespace mozilla::dom::indexedDB;

namespace {

typedef nsDataHashtable<nsCStringHashKey, DatabaseInfoSync*>
        DatabaseHash;

DatabaseHash* gDatabaseHash = nullptr;

PLDHashOperator
CloneObjectStoreInfo(const nsAString& aKey,
                     ObjectStoreInfo* aData,
                     void* aUserArg)
{
  ObjectStoreInfoHash* hash = static_cast<ObjectStoreInfoHash*>(aUserArg);

  nsRefPtr<ObjectStoreInfo> newInfo(new ObjectStoreInfo(*aData));

  hash->Put(aKey, newInfo);

  return PL_DHASH_NEXT;
}

}

DatabaseInfoSync::DatabaseInfoSync()
{
}

DatabaseInfoSync::~DatabaseInfoSync()
{
  // Clones are never in the hash.
  if (!cloned) {
    DatabaseInfoSync::Remove(id);
  }
}

// static
bool
DatabaseInfoSync::Get(nsCString& aId,
                      DatabaseInfoSync** aInfo)
{
  NS_ASSERTION(!aId.IsEmpty(), "Bad id!");

  if (gDatabaseHash &&
      gDatabaseHash->Get(aId, aInfo)) {
    NS_IF_ADDREF(*aInfo);
    return true;
  }
  return false;
}

// static
bool
DatabaseInfoSync::Put(DatabaseInfoSync* aInfo)
{
  NS_ASSERTION(aInfo, "Null pointer!");

  if (!gDatabaseHash) {
    nsAutoPtr<DatabaseHash> databaseHash(new DatabaseHash());
    gDatabaseHash = databaseHash.forget();
  }

  if (gDatabaseHash->Get(aInfo->id, nullptr)) {
    NS_ERROR("Already know about this database!");
    return false;
  }

  gDatabaseHash->Put(aInfo->id, aInfo);

  return true;
}

// static
void
DatabaseInfoSync::Remove(const nsACString& aId)
{

  if (gDatabaseHash) {
    gDatabaseHash->Remove(aId);

    if (!gDatabaseHash->Count()) {
      delete gDatabaseHash;
      gDatabaseHash = nullptr;
    }
  }
}

already_AddRefed<DatabaseInfoSync>
DatabaseInfoSync::Clone()
{
  nsRefPtr<DatabaseInfoSync> dbInfo(new DatabaseInfoSync());

  dbInfo->cloned = true;
  dbInfo->name = name;
  dbInfo->origin = origin;
  dbInfo->version = version;
  dbInfo->id = id;
  dbInfo->filePath = filePath;
  dbInfo->nextObjectStoreId = nextObjectStoreId;
  dbInfo->nextIndexId = nextIndexId;

  if (objectStoreHash) {
    dbInfo->objectStoreHash = new ObjectStoreInfoHash();
    objectStoreHash->EnumerateRead(CloneObjectStoreInfo,
                                   dbInfo->objectStoreHash);
  }

  return dbInfo.forget();
}
