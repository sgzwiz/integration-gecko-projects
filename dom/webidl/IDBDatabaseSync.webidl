/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is http://www.w3.org/TR/IndexedDB/
 */

interface IDBDatabaseSync : EventTarget {
  readonly attribute DOMString name;

  readonly attribute unsigned long long version;

  [Throws]
  readonly attribute DOMString mozStorage;

  [Throws]
  readonly attribute DOMStringList objectStoreNames;

  [Throws]
  IDBObjectStoreSync
  createObjectStore(DOMString name,
                    optional IDBObjectStoreParameters optionalParameters);

  [Throws]
  void
  deleteObjectStore(DOMString name);

  // This should be:
  // void
  // transaction((DOMString or sequence<DOMString>) storeNames,
  //            IDBTransactionCallback callback,
  //            optional IDBTransactionMode mode = "readonly",
  //            optional unsigned long timeout);
  // but unions are not currently supported.

  [Throws]
  void
  transaction(DOMString storeName,
              IDBTransactionCallback callback,
              optional IDBTransactionMode mode = "readonly",
              optional unsigned long timeout);
  [Throws]
  void
  transaction(sequence<DOMString> storeNames,
              IDBTransactionCallback callback,
              optional IDBTransactionMode mode = "readonly",
              optional unsigned long timeout);

  [Throws]
  /* FileHandleSync */ any
  mozCreateFileHandle(DOMString name,
                      optional DOMString type);

  [Throws]
  void
  close();

  [Throws]
  attribute EventHandler onversionchange;
};
