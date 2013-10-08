/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;
  var autoIncrement = [false, true];
  var unique = [false, true];

  var db = indexedDBSync.open(name, 1, function(trans, oldVersion) {
    for (var incrIndex in autoIncrement) {
      var objectStore =
        trans.db.createObjectStore(autoIncrement[incrIndex], { keyPath: "id",
                                   autoIncrement: autoIncrement[incrIndex] });

      for (var i = 0; i < 10; i++) {
        objectStore.add({ id: i, index: i });
      }

      for (var uniqIndex in unique) {
        objectStore.createIndex(unique[uniqIndex], "index",
                                { unique: unique[uniqIndex] });
      }

      for (var i = 10; i < 20; i++) {
        objectStore.add({ id: i, index: i });
      }
    }
  });



  for (var incrIndex in autoIncrement) {

    var objectStoreCount;
    var indexCount;

    db.transaction(autoIncrement[incrIndex], function(trans) {
      var objectStore = trans.objectStore(autoIncrement[incrIndex]);
      objectStoreCount = objectStore.count();
      indexCount = objectStoreCount;
      is(objectStore.count(), 20, "Correct number of entries in objectStore");
    });

    for (var uniqIndex in unique) {

      db.transaction(autoIncrement[incrIndex], function(trans) {
        var objectStore = trans.objectStore(autoIncrement[incrIndex]);
        var index = objectStore.index(unique[uniqIndex]);
        is(index.count(), indexCount, "Correct number of entries in index");

        var modifiedEntry = unique[uniqIndex] ? 5 : 10;
        var keyRange = IDBKeyRange.only(modifiedEntry);

        var sawEntry = false;
        var cursor = index.openCursor(keyRange);
        if (cursor) {
          do {
            sawEntry = true;
            is(cursor.key, modifiedEntry, "Correct key");

            cursor.value.index = unique[uniqIndex] ? 30 : 35;
            cursor.update(cursor.value);
          } while (cursor.continue());
        }

        ok(sawEntry, "Saw entry for key value " + modifiedEntry);
      }, "readwrite");


      // Recount index. Shouldn't change.
      db.transaction(autoIncrement[incrIndex], function(trans) {
        var objectStore = trans.objectStore(autoIncrement[incrIndex]);
        var index = objectStore.index(unique[uniqIndex]);
        is(index.count(), indexCount, "Correct number of entries in index");

        modifiedEntry = unique[uniqIndex] ? 30 : 35;
        keyRange = IDBKeyRange.only(modifiedEntry);

        var sawEntry = false;
        var cursor = index.openCursor(keyRange);
        if (cursor) {
          do {
            sawEntry = true;
            is(cursor.key, modifiedEntry, "Correct key");

            delete cursor.value.index;
            cursor.update(cursor.value);
            indexCount--;
           } while (cursor.continue());
        }
        is(sawEntry, true, "Saw entry for key value " + modifiedEntry);
      }, "readwrite");



      // Recount objectStore. Should be unchanged.
      db.transaction(autoIncrement[incrIndex], function(trans) {
        var objectStore = trans.objectStore(autoIncrement[incrIndex]);
        is(objectStore.count(), objectStoreCount,
           "Correct number of entries in objectStore");

        // Recount index. Should be one item less.
        index = objectStore.index(unique[uniqIndex]);
        is(index.count(), indexCount,
           "Correct number of entries in index");

        modifiedEntry = objectStoreCount - 1;

        objectStore.delete(modifiedEntry);
        objectStoreCount--;
        indexCount--;

        is(objectStore.count(), objectStoreCount,
           "Correct number of entries in objectStore");

        is(index.count(), indexCount,
           "Correct number of entries in index");
      }, "readwrite");
    }
  }

  info("Test successfully completed");
  postMessage(undefined);
};
