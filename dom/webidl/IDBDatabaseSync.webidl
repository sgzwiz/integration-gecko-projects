/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://www.w3.org/TR/2012/WD-IndexedDB-20120524/#idl-def-IDBDatabaseSync
 */

interface IDBDatabaseSync : EventTarget {
    readonly    attribute DOMString          name;

    readonly    attribute unsigned long long version;

    [Throws]
    readonly    attribute DOMStringList      objectStoreNames;

    [Throws]
    IDBObjectStoreSync createObjectStore (DOMString name, optional IDBObjectStoreParameters optionalParameters);

    [Throws]
    void               deleteObjectStore (DOMString name);

    // Bug 899972 Unions are currently not supported.
    // void transaction ((DOMString or sequence<DOMString>) storeNames, IDBTransactionCallback callback, optional IDBTransactionMode mode = "readonly", optional unsigned long timeout);

    [Throws]
    void               transaction (DOMString storeName, IDBTransactionCallback callback, optional IDBTransactionMode mode = "readonly", optional unsigned long timeout);

    [Throws]
    void               transaction (sequence<DOMString> storeNames, IDBTransactionCallback callback, optional IDBTransactionMode mode = "readonly", optional unsigned long timeout);

    [Throws]
    void               close ();

                attribute EventHandler       onversionchange;
};

partial interface IDBDatabaseSync {
    //Bug 917182 [Pref="dom.indexedDB.experimental"]
    readonly    attribute StorageType        storage;

    [Throws]
    /* FileHandleSync */
    any                mozCreateFileHandle (DOMString name, optional DOMString type);
};
