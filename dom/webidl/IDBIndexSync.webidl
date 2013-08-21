/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is http://www.w3.org/TR/IndexedDB/
 */

interface IDBIndexSync {
  readonly attribute DOMString name;

  readonly attribute IDBObjectStoreSync objectStore;

  [Throws]
  readonly attribute any keyPath;

  readonly attribute boolean multiEntry;

  readonly attribute boolean unique;

  [Throws]
  IDBCursorWithValueSync?
  openCursor(optional any range,
             optional IDBCursorDirection direction = "next");

  [Throws]
  IDBCursorSync?
  openKeyCursor(optional any range,
                optional IDBCursorDirection direction = "next");

  [Throws]
  any
  get(any key);

  [Throws]
  any
  getKey(any key);

  [Throws]
  unsigned long long
  count(optional any key);
};

partial interface IDBIndexSync {
  readonly attribute DOMString storeName;

  [Throws]
  any
  mozGetAll(optional any key,
            optional unsigned long limit);

  [Throws]
  any
  mozGetAllKeys(optional any key,
                optional unsigned long limit);
};
