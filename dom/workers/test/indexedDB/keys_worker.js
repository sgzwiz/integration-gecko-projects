/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;

  // Test simple inserts
  var keys = [
    -1/0,
    -1.7e308,
    -10000,
    -2,
    -1.5,
    -1,
    -1.00001e-200,
    -1e-200,
    0,
    1e-200,
    1.00001e-200,
    1,
    2,
    10000,
    1.7e308,
    1/0,
    new Date("1750-01-02"),
    new Date("1800-12-31T12:34:56.001"),
    new Date(-1000),
    new Date(-10),
    new Date(-1),
    new Date(0),
    new Date(1),
    new Date(2),
    new Date(1000),
    new Date("1971-01-01"),
    new Date("1971-01-01T01:01:01Z"),
    new Date("1971-01-01T01:01:01.001Z"),
    new Date("1971-01-01T01:01:01.01Z"),
    new Date("1971-01-01T01:01:01.1Z"),
    new Date("1980-02-02"),
    new Date("3333-03-19T03:33:33.333"),
    "",
    "\x00",
    "\x00\x00",
    "\x00\x01",
    "\x01",
    "\x02",
    "\x03",
    "\x04",
    "\x07",
    "\x08",
    "\x0F",
    "\x10",
    "\x1F",
    "\x20",
    "01234",
    "\x3F",
    "\x40",
    "A",
    "A\x00",
    "A1",
    "ZZZZ",
    "a",
    "a\x00",
    "aa",
    "azz",
    "}",
    "\x7E",
    "\x7F",
    "\x80",
    "\xFF",
    "\u0100",
    "\u01FF",
    "\u0200",
    "\u03FF",
    "\u0400",
    "\u07FF",
    "\u0800",
    "\u0FFF",
    "\u1000",
    "\u1FFF",
    "\u2000",
    "\u3FFF",
    "\u4000",
    "\u7FFF",
    "\u8000",
    "\uD800",
    "\uD800a",
    "\uD800\uDC01",
    "\uDBFF",
    "\uDC00",
    "\uDFFF\uD800",
    "\uFFFE",
    "\uFFFF",
     "\uFFFF\x00",
    "\uFFFFZZZ",
    [],
    [-1/0],
    [-1],
    [0],
    [1],
    [1, "a"],
    [1, []],
    [1, [""]],
    [2, 3],
    [2, 3.0000000000001],
    [12, [[]]],
    [12, [[[]]]],
    [12, [[[""]]]],
    [12, [[["foo"]]]],
    [12, [[[[[3]]]]]],
    [12, [[[[[[3]]]]]]],
    [new Date(-1)],
    [new Date(1)],
    [""],
    ["", [[]]],
    ["", [[[]]]],
    ["abc"],
    ["abc", "def"],
    ["abc\x00"],
    ["abc\x00", "\x00\x01"],
    ["abc\x00", "\x00def"],
    ["abc\x00\x00def"],
    ["x", [[]]],
    ["x", [[[]]]],
    [[]],
    [[],"foo"],
    [[],[]],
    [[[]]],
    [[[]], []],
    [[[]], [[]]],
    [[[]], [[1]]],
    [[[]], [[[]]]],
    [[[1]]],
    [[[[]], []]]
    ];

  var db = indexedDBSync.open(name, 1, function(trans, oldVersion) {
    var store = trans.db.createObjectStore("store");

    for (var i = 0; i < keys.length; ++i) {
      var keyI = keys[i];
      is(indexedDBSync.cmp(keyI, keyI), 0, i + " compared to self");

      function doCompare(keyI) {
        for (var j = i-1; j >= i-10 && j >= 0; --j) {
          is(indexedDBSync.cmp(keyI, keys[j]), 1, i + " compared to " + j);
          is(indexedDBSync.cmp(keys[j], keyI), -1, j + " compared to " + i);
        }
      }

      doCompare(keyI);

      var id = store.add(i, keyI);
      is(indexedDBSync.cmp(id, keyI), 0,
         "Returned key should cmp as equal");
      ok(compareKeys(id, keyI),
         "Returned key should actually be equal");

      // Test that -0 compares the same as 0
      if (keyI === 0) {
        doCompare(-0);

        expectException(function() {
          var id = store.add(i, -0);
        }, "ConstraintError");

      }
      else if (Array.isArray(keyI) && keyI.length === 1 && keyI[0] === 0) {
        doCompare([-0]);

        expectException(function() {
          var id = store.add(i, [-0]);
        }, "ConstraintError");

      }
    }

    var cursor = store.openCursor();
    for (i = 0; i < keys.length; ++i) {
      is(indexedDBSync.cmp(cursor.key, keys[i]), 0,
         "Read back key should cmp as equal");
      ok(compareKeys(cursor.key, keys[i]),
         "Read back key should actually be equal");
      is(cursor.value, i, "Stored with right value");
      cursor.continue();
    }

    var nan = 0/0;
    var invalidKeys = [
      nan,
      undefined,
      null,
      /x/,
      {},
      new Date(NaN),
      new Date("foopy"),
      [nan],
      [undefined],
      [null],
      [/x/],
      [{}],
      [new Date(NaN)],
      [1, nan],
      [1, undefined],
      [1, null],
      [1, /x/],
      [1, {}],
      [1, [nan]],
      [1, [undefined]],
      [1, [null]],
      [1, [/x/]],
      [1, [{}]],
      ];

    for (i = 0; i < invalidKeys.length; ++i) {

      expectException( function() {
        indexedDBSync.cmp(invalidKeys[i], 1);
        ok(false, "Should have thrown");
      }, "DataError", 0);

      expectException( function() {
        indexedDBSync.cmp(1, invalidKeys[i]);
        ok(false, "Should have thrown");
      }, "DataError", 0);

      expectException( function() {
        store.put(1, invalidKeys[i]);
        ok(false, "Should have thrown");
      }, "DataError", 0);

    }
  });

  info("Test successfully completed");
  postMessage(undefined);
};
