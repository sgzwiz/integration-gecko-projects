/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_persistencetype_h__
#define mozilla_dom_quota_persistencetype_h__

#include "mozilla/dom/quota/QuotaCommon.h"

#include "mozilla/dom/StorageTypeBinding.h"

BEGIN_QUOTA_NAMESPACE

enum PersistenceType
{
  PERSISTENCE_TYPE_PERSISTENT = 0,
  PERSISTENCE_TYPE_TEMPORARY,

  // Only needed for IPC serialization helper, should never be used in code.
  PERSISTENCE_TYPE_INVALID
};

inline void
PersistenceTypeToText(PersistenceType aPersistenceType, nsAString& aText)
{
  switch (aPersistenceType) {
    case PERSISTENCE_TYPE_PERSISTENT:
      aText.AssignLiteral("persistent");
      return;
    case PERSISTENCE_TYPE_TEMPORARY:
      aText.AssignLiteral("temporary");
      return;

    case PERSISTENCE_TYPE_INVALID:
    default:
      MOZ_CRASH("Bad persistence type value!");
  }

  MOZ_CRASH("Should never get here!");
}

inline void
PersistenceTypeToText(PersistenceType aPersistenceType, nsACString& aText)
{
  switch (aPersistenceType) {
    case PERSISTENCE_TYPE_PERSISTENT:
      aText.AssignLiteral("persistent");
      return;
    case PERSISTENCE_TYPE_TEMPORARY:
      aText.AssignLiteral("temporary");
      return;

    case PERSISTENCE_TYPE_INVALID:
    default:
      MOZ_CRASH("Bad persistence type value!");
  }

  MOZ_CRASH("Should never get here!");
}

inline bool
PersistenceTypeFromText(const nsACString& aText,
                        PersistenceType& aPersistenceType)
{
  if (aText.EqualsLiteral("persistent")) {
    aPersistenceType = PERSISTENCE_TYPE_PERSISTENT;
  }
  else if (aText.EqualsLiteral("temporary")) {
    aPersistenceType = PERSISTENCE_TYPE_TEMPORARY;
  }
  else {
    return false;
  }

  return true;
}

inline bool
PersistenceTypeFromText(const char* aText, PersistenceType& aPersistenceType)
{
  return PersistenceTypeFromText(nsDependentCString(aText), aPersistenceType);
}

inline PersistenceType
PersistenceTypeFromStorage(const Optional<mozilla::dom::StorageType>& aStorage,
                           PersistenceType aDefaultPersistenceType)
{
  if (aStorage.WasPassed()) {
    static_assert(
      static_cast<uint32_t>(mozilla::dom::StorageType::Persistent) ==
      static_cast<uint32_t>(PERSISTENCE_TYPE_PERSISTENT),
      "Enum values should match.");
    static_assert(
      static_cast<uint32_t>(mozilla::dom::StorageType::Temporary) ==
      static_cast<uint32_t>(PERSISTENCE_TYPE_TEMPORARY),
      "Enum values should match.");

    return PersistenceType(static_cast<int>(aStorage.Value()));
  }

  return aDefaultPersistenceType;
}

END_QUOTA_NAMESPACE

#endif // mozilla_dom_quota_persistencetype_h__
