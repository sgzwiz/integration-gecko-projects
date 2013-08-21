/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBCursorBase.h"

USING_INDEXEDDB_NAMESPACE

// static
IDBCursorBase::Direction
IDBCursorBase::ConvertDirection(mozilla::dom::IDBCursorDirection aDirection)
{
  switch (aDirection) {
    case mozilla::dom::IDBCursorDirection::Next:
      return NEXT;

    case mozilla::dom::IDBCursorDirection::Nextunique:
      return NEXT_UNIQUE;

    case mozilla::dom::IDBCursorDirection::Prev:
      return PREV;

    case mozilla::dom::IDBCursorDirection::Prevunique:
      return PREV_UNIQUE;

    default:
      MOZ_CRASH("Unknown direction!");
  }
}

mozilla::dom::IDBCursorDirection
IDBCursorBase::GetDirection(mozilla::ErrorResult& aRv) const
{
  switch (mDirection) {
    case NEXT:
      return mozilla::dom::IDBCursorDirection::Next;

    case NEXT_UNIQUE:
      return mozilla::dom::IDBCursorDirection::Nextunique;

    case PREV:
      return mozilla::dom::IDBCursorDirection::Prev;

    case PREV_UNIQUE:
      return mozilla::dom::IDBCursorDirection::Prevunique;

    case DIRECTION_INVALID:
    default:
      MOZ_CRASH("Unknown direction!");
      return mozilla::dom::IDBCursorDirection::Next;
  }
}
