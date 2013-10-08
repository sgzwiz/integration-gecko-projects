/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;
  const objectStoreName = "Objects";

  var db = indexedDBSync.open(name, 1, function(trans, oldVersion) {
    is(trans.db.objectStoreNames.length, 0, "Correct names length");

    var objectStore = trans.db.createObjectStore(objectStoreName,
                                                 { keyPath: "foo" });
  });

  is(db.objectStoreNames.length, 1, "Correct names length");
  is(db.objectStoreNames.item(0), objectStoreName, "Correct name");

  db.transaction(objectStoreName,  function(trans) {
    var objectStore = trans.objectStore(objectStoreName);
    is(objectStore.name, objectStoreName, "Correct name");
    is(objectStore.keyPath, "foo", "Correct keyPath");
    is(objectStore.indexNames.length, 0, "Correct indexNames length");
  });

  info("Test successfully completed");
  postMessage(undefined);
};
