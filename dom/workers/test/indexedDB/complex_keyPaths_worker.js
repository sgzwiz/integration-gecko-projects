/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;

  // Test object stores
  const keyPaths = [
    { keyPath: "id",      value: { id: 5 },                      key: 5 },
    { keyPath: "id",      value: { id: "14", iid: 12 },          key: "14" },
    { keyPath: "id",      value: { iid: "14", id: 12 },          key: 12 },
    { keyPath: "id",      value: {} },
    { keyPath: "id",      value: { id: {} } },
    { keyPath: "id",      value: { id: /x/ } },
    { keyPath: "id",      value: 2 },
    { keyPath: "id",      value: undefined },
    { keyPath: "foo.id",  value: { foo: { id: 7 } },             key: 7 },
    { keyPath: "foo.id",  value: { id: 7, foo: { id: "asdf" } }, key: "asdf" },
    { keyPath: "foo.id",  value: { foo: { id: undefined } } },
    { keyPath: "foo.id",  value: { foo: 47 } },
    { keyPath: "foo.id",  value: {} },
    { keyPath: "",        value: "foopy",                        key: "foopy" },
    { keyPath: "",        value: 2,                              key: 2 },
    { keyPath: "",        value: undefined },
    { keyPath: "",        value: { id: 12 } },
    { keyPath: "",        value: /x/ },
    { keyPath: "foo.bar", value: { baz: 1, foo: { baz2: 2, bar: "xo" } },     key: "xo" },
    { keyPath: "foo.bar.baz", value: { foo: { bar: { bazz: 16, baz: 17 } } }, key: 17 },
    { keyPath: "foo..id", exception: true },
    { keyPath: "foo.",    exception: true },
    { keyPath: "fo o",    exception: true },
    { keyPath: "foo ",    exception: true },
    { keyPath: "foo[bar]",exception: true },
    { keyPath: "foo[1]",  exception: true },
    { keyPath: "$('id').stuff", exception: true },
    { keyPath: "foo.2.bar", exception: true },
    { keyPath: "foo. .bar", exception: true },
    { keyPath: ".bar",    exception: true },
    { keyPath: [],        exception: true },

    { keyPath: ["foo", "bar"],        value: { foo: 1, bar: 2 },              key: [1, 2] },
    { keyPath: ["foo"],               value: { foo: 1, bar: 2 },              key: [1] },
    { keyPath: ["foo", "bar", "bar"], value: { foo: 1, bar: "x" },            key: [1, "x", "x"] },
    { keyPath: ["x", "y"],            value: { x: [],  y: "x" },              key: [[], "x"] },
    { keyPath: ["x", "y"],            value: { x: [[1]],  y: "x" },           key: [[[1]], "x"] },
    { keyPath: ["x", "y"],            value: { x: [[1]],  y: new Date(1) },   key: [[[1]], new Date(1)] },
    { keyPath: ["x", "y"],            value: { x: [[1]],  y: [new Date(3)] }, key: [[[1]], [new Date(3)]] },
    { keyPath: ["x", "y.bar"],        value: { x: "hi", y: { bar: "x"} },     key: ["hi", "x"] },
    { keyPath: ["x.y", "y.bar"],      value: { x: { y: "hello" }, y: { bar: "nurse"} }, key: ["hello", "nurse"] },
    { keyPath: ["", ""],              value: 5,                               key: [5, 5] },
    { keyPath: ["x", "y"],            value: { x: 1 } },
    { keyPath: ["x", "y"],            value: { y: 1 } },
    { keyPath: ["x", "y"],            value: { x: 1, y: undefined } },
    { keyPath: ["x", "y"],            value: { x: null, y: 1 } },
    { keyPath: ["x", "y.bar"],        value: { x: null, y: { bar: "x"} } },
    { keyPath: ["x", "y"],            value: { x: 1, y: false } },
    { keyPath: ["x", "y", "z"],       value: { x: 1, y: false, z: "a" } },
    { keyPath: [".x", "y", "z"],      exception: true },
    { keyPath: ["x", "y ", "z"],      exception: true }
  ];

  var db = indexedDBSync.open(name, 1, function(trans, oldVersion) {
    var stores = {};

    // Test creating object stores and inserting data
    for (var i = 0; i < keyPaths.length; i++) {
      var item = keyPaths[i];

      var test = " for objectStore test " + JSON.stringify(item);
      var indexName = JSON.stringify(item.keyPath);
      if (!stores[indexName]) {
        try {
          var objectStore = trans.db.createObjectStore(indexName,
                                                       { keyPath: item.keyPath });
          ok(!("exception" in item), "Shouldn't throw" + test);
          is(JSON.stringify(objectStore.keyPath), JSON.stringify(item.keyPath),
             "correct keyPath property" + test);
          ok(objectStore.keyPath === objectStore.keyPath,
             "object identity should be preserved");
          stores[indexName] = objectStore;
        } catch (e) {
          ok("exception" in item, "should throw" + test);
          is(e.name, "SyntaxError", "expect a SyntaxError" + test);
          ok(e instanceof DOMException, "Got a DOM Exception" + test);
          is(e.code, DOMException.SYNTAX_ERR, "expect a syntax error" + test);
          continue;
        }
      }

      var store = stores[indexName];
      var request;
      try {
        request = store.add(item.value);
        ok("key" in item, "successfully created request to insert value" + test);
      } catch (e) {
        ok(!("key" in item), "threw when attempted to insert" + test);
        ok(e instanceof DOMException, "Got a DOMException" + test);
        is(e.name, "DataError", "expect a DataError" + test);
        is(e.code, 0, "expect zero" + test);
        continue;
      }

      ok(compareKeys(request, item.key), "Found correct key" + test);
      is(indexedDBSync.cmp(request, item.key), 0,
         "Returned key compares correctly" + test);

      request = store.get(item.key);
      isnot(request, undefined, "Entry found");

      // Check that cursor.update work as expected
      var cursor = store.openCursor();
      cursor.update(item.value);
      ok(true, "Successfully updated cursor" + test);

      // Check that cursor.update throws as expected when key is changed
      var newValue = cursor.value;
      var destProp = Array.isArray(item.keyPath) ? item.keyPath[0] : item.keyPath;
      if (destProp) {
        eval("newValue." + destProp + " = 'newKeyValue'");
      }
      else {
        newValue = 'newKeyValue';
      }

      expectException( function() {
        cursor.update(newValue);
      }, "DataError", 0);

      // Clear object store to prepare for next test
      store.clear();
    }

    // Attempt to create indexes and insert data
    var store = trans.db.createObjectStore("indexStore");
    var indexes = {};
    for (var i = 0; i < keyPaths.length; i++) {
      var test = " For index test " + JSON.stringify(item);
      var item = keyPaths[i];
      var indexName = JSON.stringify(item.keyPath);
      if (!indexes[indexName]) {
        try {
          var index = store.createIndex(indexName, item.keyPath);
          ok(!("exception" in item), "Shouldn't throw" + test);
          is(JSON.stringify(index.keyPath), JSON.stringify(item.keyPath),
             "Index has correct keyPath property" + test);
          ok(index.keyPath === index.keyPath,
             "Object identity should be preserved");
          indexes[indexName] = index;
        } catch (e) {
          ok("exception" in item, "Should throw" + test);
          is(e.name, "SyntaxError", "Expect a SyntaxError" + test);
          ok(e instanceof DOMException, "Got a DOM Exception" + test);
          is(e.code, DOMException.SYNTAX_ERR, "Expect a syntax error" + test);
          continue;
        }
      }

      var index = indexes[indexName];

      var request = store.add(item.value, 1);
      if ("key" in item) {
        var req = index.getKey(item.key);
        is(req, 1, "Found value when reading" + test);
      }
      else {
        is(index.count(), 0, "Should be empty" + test);
      }

      store.clear();
    }

    // Autoincrement and complex key paths
    var aitests = [{ v: {},                           k: 1, res: { foo: { id: 1 }} },
                   { v: { value: "x" },               k: 2, res: { value: "x", foo: { id: 2 }} },
                   { v: { value: "x", foo: {} },      k: 3, res: { value: "x", foo: { id: 3 }} },
                   { v: { v: "x", foo: { x: "y" } },  k: 4, res: { v: "x", foo: { x: "y", id: 4 }} },
                   { v: { value: 2, foo: { id: 10 }}, k: 10 },
                   { v: { value: 2 },                 k: 11, res: { value: 2, foo: { id: 11 }} },
                   { v: true                          },
                   { v: { value: 2, foo: 12 }         },
                   { v: { foo: { id: true }}          },
                   { v: { foo: { x: 5, id: {} }}      },
                   { v: undefined                     },
                   { v: { foo: undefined }            },
                   { v: { foo: { id: undefined }}     },
                   { v: null                          },
                   { v: { foo: null }                 },
                   { v: { foo: { id: null }}          }
                   ];

    store = trans.db.createObjectStore("gen", { keyPath: "foo.id", autoIncrement: true });
    for (var i = 0; i < aitests.length; ++i) {
      var item = aitests[i];
      var test = " For autoIncrement test " + JSON.stringify(item);

      var preValue = JSON.stringify(item.v);
      var req;
      if ("k" in item) {
        req = store.add(item.v);
        is(JSON.stringify(item.v), preValue, "Put didn't modify value" + test);
      }
      else {
        expectException( function() {
          req = store.add(item.v);
          ok(false, "Should have thrown" + test);
        }, "DataError", 0);

        is(JSON.stringify(item.v), preValue, "Failing put didn't modify value" + test);
        continue;
      }

      is(req, item.k, "Got correct return key" + test);

      req = store.get(item.k);
      is(JSON.stringify(req), JSON.stringify(item.res || item.v),
         "Expected value stored" + test);
    }

  });

  info("Test successfully completed");
  postMessage(undefined);
};
