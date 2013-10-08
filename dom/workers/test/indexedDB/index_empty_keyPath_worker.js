/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;

  const objectStoreData = [
    { key: "1", value: "foo" },
    { key: "2", value: "bar" },
    { key: "3", value: "baz" }
  ];

  var db = indexedDBSync.open(name, 1, function(trans, oldVersion) {
    var objectStore = trans.db.createObjectStore("data", { keyPath: null });

    // First, add all our data to the object store.
    for (var i in objectStoreData) {
      var id = objectStore.add(objectStoreData[i].value, objectStoreData[i].key);
    }

    // Now create the index.
    objectStore.createIndex("set", "", { unique: true });
  });

  db.transaction("data", function(trans) {
    var objectStore = trans.objectStore("data");
    var index = objectStore.index("set");
    var value = index.get("bar");
    is(value, "bar", "Got correct result");

    var id = objectStore.add("foopy", 4);

    value = index.get("foopy");
    is(value, "foopy", "Got correct result");

    expectException(function() {
      objectStore.add("foopy", 5);
    }, "ConstraintError");

  },"readwrite");

  info("Test successfully completed");
  postMessage(undefined);
};
