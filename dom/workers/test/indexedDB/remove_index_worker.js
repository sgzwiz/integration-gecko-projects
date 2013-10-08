/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;
  const indexName = "My Test Index";

  var db = indexedDBSync.open(name, 1, function(trans, oldVersion) {
    is(trans.db.objectStoreNames.length, 0, "Correct objectStoreNames list");
    var objectStore = trans.db.createObjectStore("test store", { keyPath: "foo" });
    is(trans.db.objectStoreNames.length, 1, "Correct objectStoreNames list");
    is(trans.db.objectStoreNames.item(0), objectStore.name, "Correct name");

    is(objectStore.indexNames.length, 0, "Correct indexNames list");
    var index = objectStore.createIndex(indexName, "foo");

    is(objectStore.indexNames.length, 1, "Correct indexNames list");
    is(objectStore.indexNames.item(0), indexName, "Correct name");
    ok(objectStore.index(indexName) === index, "Correct instance");

    objectStore.deleteIndex(indexName);
    is(objectStore.indexNames.length, 0, "Correct indexNames list");

    expectException( function() {
      objectStore.index(indexName);
      ok(false, "Should have thrown");
    }, "NotFoundError", DOMException.NOT_FOUND_ERR);

    var index2 = objectStore.createIndex(indexName, "foo");
    ok(index !== index2, "New instance should be created");

    is(objectStore.indexNames.length, 1, "Correct recreacted indexNames list");
    is(objectStore.indexNames.item(0), indexName, "Correct recreacted name");
    ok(objectStore.index(indexName) === index2, "Correct instance");
  });

  info("Test successfully completed");
  postMessage(undefined);
};
