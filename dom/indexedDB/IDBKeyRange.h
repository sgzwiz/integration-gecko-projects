/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbkeyrange_h__
#define mozilla_dom_indexeddb_idbkeyrange_h__

#include "mozilla/dom/indexedDB/IndexedDatabase.h"

#include "nsIIDBKeyRange.h"

#include "nsCycleCollectionParticipant.h"

#include "mozilla/dom/indexedDB/IDBKeyRangeBase.h"
#include "mozilla/dom/indexedDB/Key.h"

class mozIStorageStatement;

BEGIN_INDEXEDDB_NAMESPACE

namespace ipc {
class KeyRange;
} // namespace ipc

class IDBKeyRange MOZ_FINAL : public nsIIDBKeyRange,
                              public IDBKeyRangeBase
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIIDBKEYRANGE
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(IDBKeyRange)

  static bool DefineConstructors(JSContext* aCx,
                                 JSObject* aObject);

  static nsresult FromJSVal(JSContext* aCx,
                            const jsval& aVal,
                            IDBKeyRange** aKeyRange);

  template <class T>
  static already_AddRefed<IDBKeyRange>
  FromSerializedKeyRange(const T& aKeyRange);

  IDBKeyRange(bool aLowerOpen,
              bool aUpperOpen,
              bool aIsOnly)
  : IDBKeyRangeBase(aLowerOpen, aUpperOpen, aIsOnly),
    mRooted(false)
  { }

  void GetBindingClause(const nsACString& aKeyColumnName,
                        nsACString& _retval) const
  {
    NS_NAMED_LITERAL_CSTRING(andStr, " AND ");
    NS_NAMED_LITERAL_CSTRING(spacecolon, " :");
    NS_NAMED_LITERAL_CSTRING(lowerKey, "lower_key");

    if (IsOnly()) {
      // Both keys are set and they're equal.
      _retval = andStr + aKeyColumnName + NS_LITERAL_CSTRING(" =") +
                spacecolon + lowerKey;
    }
    else {
      nsAutoCString clause;

      if (!Lower().IsUnset()) {
        // Lower key is set.
        clause.Append(andStr + aKeyColumnName);
        clause.AppendLiteral(" >");
        if (!IsLowerOpen()) {
          clause.AppendLiteral("=");
        }
        clause.Append(spacecolon + lowerKey);
      }

      if (!Upper().IsUnset()) {
        // Upper key is set.
        clause.Append(andStr + aKeyColumnName);
        clause.AppendLiteral(" <");
        if (!IsUpperOpen()) {
          clause.AppendLiteral("=");
        }
        clause.Append(spacecolon + NS_LITERAL_CSTRING("upper_key"));
      }

      _retval = clause;
    }
  }

  nsresult BindToStatement(mozIStorageStatement* aStatement) const
  {
    NS_NAMED_LITERAL_CSTRING(lowerKey, "lower_key");

    if (IsOnly()) {
      return Lower().BindToStatement(aStatement, lowerKey);
    }

    nsresult rv;

    if (!Lower().IsUnset()) {
      rv = Lower().BindToStatement(aStatement, lowerKey);
      NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    }

    if (!Upper().IsUnset()) {
      rv = Upper().BindToStatement(aStatement, NS_LITERAL_CSTRING("upper_key"));
      NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    }

    return NS_OK;
  }

  void DropJSObjects();

private:
  ~IDBKeyRange();

  bool mRooted;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_idbkeyrange_h__
