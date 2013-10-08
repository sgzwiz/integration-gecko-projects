/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;

 // Test object stores
  var db = indexedDBSync.open(name, 1, function(trans, oldVersion) {
    var tests =
      [{ add:     { x: 1, id: 1 },
         indexes:[{ v: 1, k: 1 }] },
       { add:     { x: [2, 3], id: 2 },
         indexes:[{ v: 1, k: 1 },
                  { v: 2, k: 2 },
                  { v: 3, k: 2 }] },
       { put:     { x: [2, 4], id: 1 },
         indexes:[{ v: 2, k: 1 },
                  { v: 2, k: 2 },
                  { v: 3, k: 2 },
                  { v: 4, k: 1 }] },
       { add:     { x: [5, 6, 5, -2, 3], id: 3 },
         indexes:[{ v:-2, k: 3 },
                  { v: 2, k: 1 },
                  { v: 2, k: 2 },
                  { v: 3, k: 2 },
                  { v: 3, k: 3 },
                  { v: 4, k: 1 },
                  { v: 5, k: 3 },
                  { v: 6, k: 3 }] },
       { delete:  IDBKeyRange.bound(1, 3),
         indexes:[] },
       { put:     { x: ["food", {}, false, undefined, /x/, [73, false]], id: 2 },
         indexes:[{ v: "food", k: 2 }] },
       { add:     { x: [{}, /x/, -12, "food", null, [false], undefined], id: 3 },
         indexes:[{ v: -12, k: 3 },
                  { v: "food", k: 2 },
                  { v: "food", k: 3 }] },
       { put:     { x: [], id: 2 },
         indexes:[{ v: -12, k: 3 },
                  { v: "food", k: 3 }] },
       { put:     { x: { y: 3 }, id: 3 },
         indexes:[] },
       { add:     { x: false, id: 7 },
         indexes:[] },
       { delete:  IDBKeyRange.lowerBound(0),
         indexes:[] }
      ];

    var store = trans.db.createObjectStore("mystore", { keyPath: "id" });
    var index = store.createIndex("myindex", "x", { multiEntry: true });
    ok(index.multiEntry, "Index created with multiEntry");

    var i;
    for (i = 0; i < tests.length; ++i) {
      var test = tests[i];
      var testName = " for " + JSON.stringify(test);
      if (test.add) {
        store.add(test.add);
      }
      else if (test.put) {
        store.put(test.put);
      }
      else if (test.delete) {
        store.delete(test.delete);
      }
      else {
        ok(false, "Borked test");
      }

      var cursor = index.openKeyCursor();
      var result;
      for (var j = 0; j < test.indexes.length; ++j) {
        is(cursor.key, test.indexes[j].v, "Found expected index key at index " + j + testName);
        is(cursor.primaryKey, test.indexes[j].k, "Found expected index primary key at index " + j + testName);
        result = cursor.continue();
      }
      is(result, false, "exhausted indexes");

      var tempIndex = store.createIndex("temp index", "x", { multiEntry: true });
      cursor = tempIndex.openKeyCursor();
      for (var j = 0; j < test.indexes.length; ++j) {
        is(cursor.key, test.indexes[j].v, "Found expected temp index key at index " + j + testName);
        is(cursor.primaryKey, test.indexes[j].k, "Found expected temp index primary key at index " + j + testName);
        result = cursor.continue();
      }
      is(result, false, "exhausted temp index");
      store.deleteIndex("temp index");
    }

    // Unique indexes
    tests =
      [{ add:     { x: 1, id: 1 },
         indexes:[{ v: 1, k: 1 }] },
       { add:     { x: [2, 3], id: 2 },
         indexes:[{ v: 1, k: 1 },
                  { v: 2, k: 2 },
                  { v: 3, k: 2 }] },
       { put:     { x: [2, 4], id: 3 },
         fail:    true },
       { put:     { x: [1, 4], id: 1 },
         indexes:[{ v: 1, k: 1 },
                  { v: 2, k: 2 },
                  { v: 3, k: 2 },
                  { v: 4, k: 1 }] },
       { add:     { x: [5, 0, 5, 5, 5], id: 3 },
         indexes:[{ v: 0, k: 3 },
                  { v: 1, k: 1 },
                  { v: 2, k: 2 },
                  { v: 3, k: 2 },
                  { v: 4, k: 1 },
                  { v: 5, k: 3 }] },
       { delete:  IDBKeyRange.bound(1, 2),
         indexes:[{ v: 0, k: 3 },
                  { v: 5, k: 3 }] },
       { add:     { x: [0, 6], id: 8 },
         fail:    true },
       { add:     { x: 5, id: 8 },
         fail:    true },
       { put:     { x: 0, id: 8 },
         fail:    true },
      ];

    store.deleteIndex("myindex");
    index = store.createIndex("myindex", "x", { multiEntry: true, unique: true });
    is(index.multiEntry, true, "Index created with multiEntry");

    var i;
    var indexes;
    for (i = 0; i < tests.length; ++i) {
      var test = tests[i];
      var testName = " for " + JSON.stringify(test);
      try {
        if (test.add) {
          store.add(test.add);
        }
        else if (test.put) {
          store.put(test.put);
        }
        else if (test.delete) {
          store.delete(test.delete);
        }
        else {
          ok(false, "Borked test");
        }
        ok(!test.fail, "Should have failed");
        indexes = test.indexes;
      }
      catch(e) {
        ok(test.fail, "Got expected error");
      }

      var e;
      var cursor = index.openKeyCursor();
      var result;
      for (var j = 0; j < indexes.length; ++j) {
        is(cursor.key, indexes[j].v, "Found expected index key at index " + j + testName);
        is(cursor.primaryKey, indexes[j].k, "Found expected index primary key at index " + j + testName);
        result = cursor.continue();
      }
      is(result, false, "exhausted indexes");

      var tempIndex = store.createIndex("temp index", "x", { multiEntry: true, unique: true });
      cursor = tempIndex.openKeyCursor();
      for (var j = 0; j < indexes.length; ++j) {
        is(cursor.key, indexes[j].v, "Found expected temp index key at index " + j + testName);
        is(cursor.primaryKey, indexes[j].k, "Found expected temp index primary key at index " + j + testName);
        result = cursor.continue();
      }
      is(result, false, "exhausted temp index");
      store.deleteIndex("temp index");
    }
  });

  db.transaction(["mystore"], function(trans) {
    var store = trans.objectStore("mystore");
    var index = store.index("myindex");
    is(index.multiEntry, true, "Index is still  multiEntry");
  }, "readwrite");

  info("Test successfully completed");
  postMessage(undefined);
};
