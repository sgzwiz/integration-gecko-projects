/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is http://www.w3.org/TR/IndexedDB/
 */

interface IDBObjectStoreSync {
  readonly attribute DOMString name;

  [Throws]
  readonly attribute any keyPath;

  readonly attribute DOMStringList indexNames;

  readonly attribute IDBTransactionSync transaction;

  readonly attribute boolean autoIncremenent;

  [Throws]
  any
  put(any value,
      optional any key);

  [Throws]
  any
  add(any value,
      optional any key);

  [Throws]
  void
  delete (any key);

  [Throws]
  any
  get(any key);

  [Throws]
  void
  clear();

  // IDBIndexSync
  // createIndex(DOMString name,
  //             (DOMString or sequence<DOMString>) keyPath,
  //             optional IDBIndexParameters optionalParameters);

  [Throws]
  IDBIndexSync
  createIndex(DOMString name,
              DOMString keyPath,
              optional IDBIndexParameters optionalParameters);

  [Throws]
  IDBIndexSync
  createIndex(DOMString name,
              sequence<DOMString> keyPath,
              optional IDBIndexParameters optionalParameters);

  [Throws]
  IDBIndexSync
  index(DOMString name);

  [Throws]
  void
  deleteIndex(DOMString indexName);

  [Throws]
  IDBCursorWithValueSync?
  openCursor(optional any range,
             optional IDBCursorDirection direction = "next");

  [Throws]
  unsigned long long
  count(optional any key);
};

partial interface IDBObjectStoreSync {
  [Throws]
  any
  mozGetAll(optional any key,
            optional unsigned long limit);
};
