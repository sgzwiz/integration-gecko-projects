/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://www.w3.org/TR/2012/WD-IndexedDB-20120524/#idl-def-IDBVersionChangeCallback
 */

/*
callback interface IDBVersionChangeCallback {
    void transactionStarted (IDBTransactionSync transaction, unsigned long long oldVersion);
};
*/

callback IDBVersionChangeCallback = void (IDBTransactionSync transaction, unsigned long long oldVersion);
