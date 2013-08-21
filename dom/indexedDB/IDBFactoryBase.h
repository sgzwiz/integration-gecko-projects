/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbfactorybase_h__
#define mozilla_dom_indexeddb_idbfactorybase_h__

#include "mozilla/dom/indexedDB/IndexedDatabase.h"

#include "mozilla/dom/quota/PersistenceType.h"

namespace mozilla {
class ErrorResult;
} // namespace mozilla

BEGIN_INDEXEDDB_NAMESPACE

class IDBFactoryBase
{
public:
  const nsCString&
  GetGroup() const
  {
    return mGroup;
  }

  const nsCString&
  GetASCIIOrigin() const
  {
    return mASCIIOrigin;
  }

  mozilla::dom::quota::PersistenceType
  GetDefaultPersistenceType() const
  {
    return mDefaultPersistenceType;
  }

  int16_t
  Cmp(JSContext* aCx, JS::Handle<JS::Value> aFirst,
      JS::Handle<JS::Value> aSecond, ErrorResult& aRv);

protected:
  IDBFactoryBase()
  : mDefaultPersistenceType(mozilla::dom::quota::PERSISTENCE_TYPE_TEMPORARY)
  { }

  virtual ~IDBFactoryBase()
  { }

  nsCString mGroup;
  nsCString mASCIIOrigin;
  mozilla::dom::quota::PersistenceType mDefaultPersistenceType;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_idbfactorybase_h__
