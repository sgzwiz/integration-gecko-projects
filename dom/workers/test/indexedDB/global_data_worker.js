/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;
  const objectStore =  { name: "Objects",
                         options: { keyPath: "id", autoIncrement: true } };

  var db1 = indexedDBSync.open(name, 1, function(trans, oldVersion) {
    is(trans.db.objectStoreNames.length, 0, "No objectStores in db1");
    var store = trans.db.createObjectStore(objectStore.name,
                                           objectStore.options);
  });

  var db2 = indexedDBSync.open(name, 1);

  ok(db1 !== db2, "Databases are not the same object");
  is(db1.objectStoreNames.length, 1, "One objectStore in db1");
  is(db1.objectStoreNames.item(0), objectStore.name, "Correct name");

  is(db2.objectStoreNames.length, 1, "One objectStore in db2");
  is(db2.objectStoreNames.item(0), objectStore.name, "Correct name");

  var objectStore1;
  db1.transaction(objectStore.name, function(trans) {
    objectStore1 = trans.objectStore(objectStore.name);

    is(objectStore1.name, objectStore.name, "Same name");
    is(objectStore1.keyPath, objectStore.options.keyPath, "Same keyPath");
  });

  db2.transaction(objectStore.name, function(trans) {
    var objectStore2 = trans.objectStore(objectStore.name);

    ok(objectStore1 !== objectStore2, "Different objectStores");
    is(objectStore1.name, objectStore2.name, "Same name");
    is(objectStore1.keyPath, objectStore2.keyPath, "Same keyPath");
  });

  info("Test successfully completed");
  postMessage(undefined);
};
