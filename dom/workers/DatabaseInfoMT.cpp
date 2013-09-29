/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DatabaseInfoMT.h"

#include "nsDataHashtable.h"
#include "mozilla/StaticMutex.h"

USING_WORKERS_NAMESPACE
using namespace mozilla::dom::indexedDB;

namespace {

typedef nsDataHashtable<nsCStringHashKey, DatabaseInfoMT*>
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

DatabaseInfoMT::DatabaseInfoMT()
{
}

DatabaseInfoMT::~DatabaseInfoMT()
{
  // Clones are never in the hash.
  if (!cloned) {
    DatabaseInfoMT::Remove(id);
  }
}

// static 
mozilla::StaticMutex DatabaseInfoMT::sDatabaseInfoMutex;

// static
bool
DatabaseInfoMT::Get(nsCString& aId,
                      DatabaseInfoMT** aInfo)
{
  MOZ_ASSERT(!aId.IsEmpty(), "Bad id!");
  
  StaticMutexAutoLock lock(sDatabaseInfoMutex);
  if (gDatabaseHash &&
      gDatabaseHash->Get(aId, aInfo)) {
    NS_IF_ADDREF(*aInfo);
    return true;
  }
  return false;
}

// static
bool
DatabaseInfoMT::Put(DatabaseInfoMT* aInfo)
{
  MOZ_ASSERT(aInfo, "Null pointer!");

  StaticMutexAutoLock lock(sDatabaseInfoMutex);
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
DatabaseInfoMT::Remove(const nsACString& aId)
{
  StaticMutexAutoLock lock(sDatabaseInfoMutex);
  if (gDatabaseHash) {
    gDatabaseHash->Remove(aId);

    if (!gDatabaseHash->Count()) {
      delete gDatabaseHash;
      gDatabaseHash = nullptr;
    }
  }
}

bool
DatabaseInfoMT::GetObjectStoreNames(nsTArray<nsString>& aNames)
{
  StaticMutexAutoLock lock(sDatabaseInfoMutex);

  return DatabaseInfoBase::GetObjectStoreNames(aNames);
}

bool
DatabaseInfoMT::ContainsStoreName(const nsAString& aName)
{
  StaticMutexAutoLock lock(sDatabaseInfoMutex);

  return DatabaseInfoBase::ContainsStoreName(aName);
}

ObjectStoreInfo*
DatabaseInfoMT::GetObjectStore(const nsAString& aName)
{
  StaticMutexAutoLock lock(sDatabaseInfoMutex);

  return DatabaseInfoBase::GetObjectStore(aName);
}

bool
DatabaseInfoMT::PutObjectStore(ObjectStoreInfo* aInfo)
{
  StaticMutexAutoLock lock(sDatabaseInfoMutex);

  return DatabaseInfoBase::PutObjectStore(aInfo);
}

void
DatabaseInfoMT::RemoveObjectStore(const nsAString& aName)
{
  StaticMutexAutoLock lock(sDatabaseInfoMutex);

  DatabaseInfoBase::RemoveObjectStore(aName);
}

already_AddRefed<DatabaseInfoMT>
DatabaseInfoMT::Clone()
{
  nsRefPtr<DatabaseInfoMT> dbInfo(new DatabaseInfoMT());

  dbInfo->cloned = true;
  dbInfo->name = name;
  dbInfo->origin = origin;
  dbInfo->version = version;
  dbInfo->id = id;
  dbInfo->filePath = filePath;
  dbInfo->nextObjectStoreId = nextObjectStoreId;
  dbInfo->nextIndexId = nextIndexId;

  StaticMutexAutoLock lock(sDatabaseInfoMutex);
  if (objectStoreHash) {
    dbInfo->objectStoreHash = new ObjectStoreInfoHash();
    objectStoreHash->EnumerateRead(CloneObjectStoreInfo,
                                   dbInfo->objectStoreHash);
  }

  return dbInfo.forget();
}
