/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is http://www.w3.org/TR/IndexedDB/
 */

interface IDBKeyRange {
  [Throws]
  readonly attribute any lower;

  [Throws]
  readonly attribute any upper;

  readonly attribute boolean lowerOpen;

  readonly attribute boolean upperOpen;

  [Creator, Throws]
  static IDBKeyRange
  only(any value);

  [Creator, Throws]
  static IDBKeyRange
  lowerBound(any lower,
             optional boolean open = false);

  [Creator, Throws]
  static IDBKeyRange
  upperBound(any upper,
             optional boolean open = false);

  [Creator, Throws]
  static IDBKeyRange
  bound(any lower,
        any upper,
        optional boolean lowerOpen = false,
        optional boolean upperOpen = false);
};
