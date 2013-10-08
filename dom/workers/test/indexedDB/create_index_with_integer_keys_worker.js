/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;
  const data = { id: new Date().getTime(),
                 num: parseInt(Math.random() * 1000) };

  var db = indexedDBSync.open(name, 1, function(trans, oldVersion) {
    var objectStore = trans.db.createObjectStore("foo", { keyPath: "id" });
    objectStore.add(data);
    is(objectStore.count(), 1, "Correct count");
  });
  db.close();

  var db2 = indexedDBSync.open(name, 2, function(trans, oldVersion) {
    var objectStore = trans.objectStore("foo");
    objectStore.createIndex("foo", "num");
  });

  // Make sure our object made it into the index.
  var seenCount = 0;

  db2.transaction("foo", function(trans) {
    var objectStore = trans.objectStore("foo");
    var index = objectStore.index("foo");

    var cursor = index.openKeyCursor();
    if (cursor) {
      do {
        is(cursor.key, data.num, "Correct key");
        is(cursor.primaryKey, data.id, "Correct primaryKey");
        seenCount++;
      } while(cursor.continue());
    }
    is(seenCount, 1, "Saw our entry");
  });

  info("Test successfully completed");
  postMessage(undefined);
};
