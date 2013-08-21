/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBKeyRangeBase.h"

#include "mozilla/dom/indexedDB/PIndexedDBIndex.h"
#include "mozilla/dom/indexedDB/PIndexedDBObjectStore.h"

USING_INDEXEDDB_NAMESPACE
using namespace mozilla::dom::indexedDB::ipc;

template <class T>
void
IDBKeyRangeBase::ToSerializedKeyRange(T& aKeyRange)
{
  aKeyRange.lowerOpen() = IsLowerOpen();
  aKeyRange.upperOpen() = IsUpperOpen();
  aKeyRange.isOnly() = IsOnly();

  aKeyRange.lower() = Lower();
  if (!IsOnly()) {
    aKeyRange.upper() = Upper();
  }
}

// Explicitly instantiate for all our key range types... Grumble.
template void
IDBKeyRangeBase::ToSerializedKeyRange<KeyRange> (KeyRange& aKeyRange);
