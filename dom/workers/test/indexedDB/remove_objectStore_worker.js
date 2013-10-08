/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;
  const objectStoreName = "Objects";

  var db = indexedDBSync.open(name, 1, function(trans, oldVersion) {
    is(trans.db.objectStoreNames.length, 0, "Correct objectStoreNames list");
    var objectStore = trans.db.createObjectStore(objectStoreName,
                                                 { keyPath: "foo" });

    var addedCount = 0;

    for (var i = 0; i < 100; i++) {
      objectStore.add({foo: i});
      addedCount++;
    }
    ok(addedCount == 100, "Correct length");
    is(trans.db.objectStoreNames.length, 1, "Correct objectStoreNames list");
    is(trans.db.objectStoreNames.item(0), objectStoreName, "Correct name");
  });

  db.close();

  var db = indexedDBSync.open(name, 2, function(trans, oldVersion) {
    var oldObjectStore = trans.objectStore(objectStoreName);
    ok(oldObjectStore !== null, "Correct object store prior to deleting");
    trans.db.deleteObjectStore(objectStoreName);
    is(trans.db.objectStoreNames.length, 0, "Correct objectStores list");

    expectException( function() {
      trans.objectStore(objectStoreName);
      ok(false, "Should have thrown");
    }, "NotFoundError", DOMException.NOT_FOUND_ERR);

    var objectStore = trans.db.createObjectStore(objectStoreName, { keyPath: "foo" });
    is(trans.db.objectStoreNames.length, 1, "Correct objectStoreNames list");
    is(trans.db.objectStoreNames.item(0), objectStoreName, "Correct name");
    ok(trans.objectStore(objectStoreName) === objectStore, "Correct new objectStore");
    ok(oldObjectStore !== objectStore, "Old objectStore is not new objectStore");

    var cursor = objectStore.openCursor();
    if (cursor) {
      ok(false, "Shouldn't have any entries");
    }
    ok(cursor == undefined, "ObjectStore shouldn't have any items");

    trans.db.deleteObjectStore(objectStore.name);
    is(trans.db.objectStoreNames.length, 0, "Correct objectStores list");
  });

  db.close();

  var db =  indexedDBSync.open(name, 3, function(trans, oldVersion) {
    var objectStore = trans.db.createObjectStore(objectStoreName, { keyPath: "foo" });
    is(trans.db.objectStoreNames.length, 1, "Correct objectStoreNames list");
    objectStore.add({foo:"bar"});
    trans.db.deleteObjectStore(objectStoreName);
    is(trans.db.objectStoreNames.length, 0, "Correct objectStoreNames list");
  });

  info("Test successfully completed");
  postMessage(undefined);
};
