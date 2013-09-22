/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_databaseinfo_h__
#define mozilla_dom_indexeddb_databaseinfo_h__

#include "mozilla/dom/indexedDB/IndexedDatabase.h"

#include "mozilla/dom/quota/PersistenceType.h"
#include "nsRefPtrHashtable.h"
#include "nsHashKeys.h"

#include "mozilla/dom/indexedDB/Key.h"
#include "mozilla/dom/indexedDB/KeyPath.h"
#include "mozilla/dom/indexedDB/IDBObjectStore.h"

BEGIN_INDEXEDDB_NAMESPACE

class IndexedDBDatabaseChild;
struct ObjectStoreInfo;

typedef nsRefPtrHashtable<nsStringHashKey, ObjectStoreInfo>
        ObjectStoreInfoHash;

struct DatabaseInfoGuts
{
  typedef mozilla::dom::quota::PersistenceType PersistenceType;

  DatabaseInfoGuts()
  : nextObjectStoreId(1), nextIndexId(1)
  { }

  bool operator==(const DatabaseInfoGuts& aOther) const
  {
    return this->name == aOther.name &&
           this->group == aOther.group &&
           this->origin == aOther.origin &&
           this->version == aOther.version &&
           this->persistenceType == aOther.persistenceType &&
           this->nextObjectStoreId == aOther.nextObjectStoreId &&
           this->nextIndexId == aOther.nextIndexId;
  };

  // Make sure to update ipc/SerializationHelpers.h when changing members here!
  nsString name;
  nsCString group;
  nsCString origin;
  uint64_t version;
  PersistenceType persistenceType;
  int64_t nextObjectStoreId;
  int64_t nextIndexId;
};

struct DatabaseInfoBase : public DatabaseInfoGuts
{
  DatabaseInfoBase()
  : cloned(false)
  { }

  bool GetObjectStoreNames(nsTArray<nsString>& aNames);

  bool ContainsStoreName(const nsAString& aName);

  ObjectStoreInfo* GetObjectStore(const nsAString& aName);

  bool PutObjectStore(ObjectStoreInfo* aInfo);

  void RemoveObjectStore(const nsAString& aName);

  nsString filePath;
  bool cloned;
  nsAutoPtr<ObjectStoreInfoHash> objectStoreHash;
};

struct DatabaseInfo : public DatabaseInfoBase
{
  DatabaseInfo()
  { }

  ~DatabaseInfo();

  static bool Get(nsIAtom* aId,
                  DatabaseInfo** aInfo);

  static bool Put(DatabaseInfo* aInfo);

  static void Remove(nsIAtom* aId);

  bool GetObjectStoreNames(nsTArray<nsString>& aNames);

  bool ContainsStoreName(const nsAString& aName);

  ObjectStoreInfo* GetObjectStore(const nsAString& aName);

  bool PutObjectStore(ObjectStoreInfo* aInfo);

  void RemoveObjectStore(const nsAString& aName);

  already_AddRefed<DatabaseInfo> Clone();

  nsCOMPtr<nsIAtom> id;

  NS_INLINE_DECL_REFCOUNTING(DatabaseInfo)
};

struct IndexInfo
{
#ifdef NS_BUILD_REFCNT_LOGGING
  IndexInfo();
  IndexInfo(const IndexInfo& aOther);
  ~IndexInfo();
#else
  IndexInfo()
  : id(INT64_MIN), keyPath(0), unique(false), multiEntry(false) { }
#endif

  bool operator==(const IndexInfo& aOther) const
  {
    return this->name == aOther.name &&
           this->id == aOther.id &&
           this->keyPath == aOther.keyPath &&
           this->unique == aOther.unique &&
           this->multiEntry == aOther.multiEntry;
  };

  // Make sure to update ipc/SerializationHelpers.h when changing members here!
  nsString name;
  int64_t id;
  KeyPath keyPath;
  bool unique;
  bool multiEntry;
};

struct ObjectStoreInfoGuts
{
  ObjectStoreInfoGuts()
  : id(0), keyPath(0), autoIncrement(false)
  { }

  bool operator==(const ObjectStoreInfoGuts& aOther) const
  {
    return this->name == aOther.name &&
           this->id == aOther.id;
  };

  // Make sure to update ipc/SerializationHelpers.h when changing members here!

  // Constant members, can be gotten on any thread
  nsString name;
  int64_t id;
  KeyPath keyPath;
  bool autoIncrement;

  // Main-thread only members. This must *not* be touched on the database
  // thread.
  nsTArray<IndexInfo> indexes;
};

struct ObjectStoreInfo : public ObjectStoreInfoGuts
{
#ifdef NS_BUILD_REFCNT_LOGGING
  ObjectStoreInfo();
#else
  ObjectStoreInfo()
  : nextAutoIncrementId(0), comittedAutoIncrementId(0) { }
#endif

  ObjectStoreInfo(ObjectStoreInfo& aOther);

private:
#ifdef NS_BUILD_REFCNT_LOGGING
  ~ObjectStoreInfo();
#else
  ~ObjectStoreInfo() {}
#endif
public:

  // Database-thread members. After the ObjectStoreInfo has been initialized,
  // these can *only* be touced on the database thread.
  int64_t nextAutoIncrementId;
  int64_t comittedAutoIncrementId;

  // This is threadsafe since the ObjectStoreInfos are created on the database
  // thread but then only used from the main thread. Ideal would be if we
  // could transfer ownership from the database thread to the main thread, but
  // we don't have that ability yet.
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ObjectStoreInfo)
};

struct IndexUpdateInfo
{
#ifdef NS_BUILD_REFCNT_LOGGING
  IndexUpdateInfo();
  IndexUpdateInfo(const IndexUpdateInfo& aOther);
  ~IndexUpdateInfo();
#endif

  bool operator==(const IndexUpdateInfo& aOther) const
  {
    return this->indexId == aOther.indexId &&
           this->indexUnique == aOther.indexUnique &&
           this->value == aOther.value;
  };

  // Make sure to update ipc/SerializationHelpers.h when changing members here!
  int64_t indexId;
  bool indexUnique;
  Key value;
};

class MOZ_STACK_CLASS AutoRemoveIndex
{
public:
  AutoRemoveIndex(ObjectStoreInfo* aObjectStoreInfo,
                  const nsAString& aIndexName)
  : mObjectStoreInfo(aObjectStoreInfo), mIndexName(aIndexName)
  { }

  ~AutoRemoveIndex()
  {
    if (mObjectStoreInfo) {
      for (uint32_t i = 0; i < mObjectStoreInfo->indexes.Length(); i++) {
        if (mObjectStoreInfo->indexes[i].name == mIndexName) {
          mObjectStoreInfo->indexes.RemoveElementAt(i);
          break;
        }
      }
    }
  }

  void forget()
  {
    mObjectStoreInfo = nullptr;
  }

private:
  ObjectStoreInfo* mObjectStoreInfo;
  nsString mIndexName;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_databaseinfo_h__
