/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;
  var autoIncrement = [true, false];

  var db = indexedDBSync.open(name, 1, function(trans, oldVersion) {
    for (var incrIndex in autoIncrement) {
      var objectStore = trans.db.createObjectStore(autoIncrement[incrIndex],
                                   { keyPath: "id",
                                     autoIncrement: autoIncrement[incrIndex] });
      objectStore.createIndex("", "index", { unique: true });

      for (var i = 0; i < 10; i++) {
        objectStore.add({ id: i, index: i });
      }
    }
  });

  for (var incrIndex in autoIncrement) {
    db.transaction(autoIncrement[incrIndex], function(trans) {
      var objectStore = trans.objectStore(autoIncrement[incrIndex]);

      expectException( function () {
        objectStore.put({ id: 5, index: 6 });
      }, "ConstraintError");

      var keyRange = IDBKeyRange.only(5);

      var cursor = objectStore.index("").openCursor(keyRange);
      if(cursor) {
        info("Got cursor here");
        is(cursor.value.index, 5, "Correct index value");
        cursor.value.index = 6;
        expectException( function () {
          cursor.update(cursor.value);
        }, "ConstraintError");
      }
      else {
        ok(false, "Should have had cursor here");
      }
    },"readwrite");
  }

  info("Test successfully completed");
  postMessage(undefined);
};
