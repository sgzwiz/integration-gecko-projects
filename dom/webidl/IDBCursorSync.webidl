/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://www.w3.org/TR/2012/WD-IndexedDB-20120524/#idl-def-IDBCursorSync
 * http://www.w3.org/TR/2012/WD-IndexedDB-20120524/#idl-def-IDBCursorWithValueSync
 */

interface IDBCursorSync {
    readonly    attribute (IDBObjectStoreSync or IDBIndexSync) source;

    [Throws]
    readonly    attribute IDBCursorDirection                   direction;

    readonly    attribute any                                  key;

    readonly    attribute any                                  primaryKey;

    [Throws]
    void    update (any value);

    [Throws]
    boolean advance ([EnforceRange] unsigned long count);

    [Throws]
    boolean continue (optional any key);

    [Throws]
    boolean delete ();
};

interface IDBCursorWithValueSync : IDBCursorSync {
    [Throws]
    readonly    attribute any                value;
};
