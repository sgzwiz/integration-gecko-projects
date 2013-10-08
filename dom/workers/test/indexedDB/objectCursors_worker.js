/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;

  const objectStores = [
    { name: "a", autoIncrement: false },
    { name: "b", autoIncrement: true }
  ];

  const indexes = [
    { name: "a", options: { } },
    { name: "b", options: { unique: true } }
  ];

  var j = 0;
  for (var i in objectStores) {
    var db = indexedDBSync.open(name, ++j, function(trans, oldVersion) {
      var objectStore =
        trans.db.createObjectStore(objectStores[i].name,
                                   { keyPath: "id",
                                    autoIncrement: objectStores[i].autoIncrement });

      for (var j in indexes) {
        objectStore.createIndex(indexes[j].name, "name", indexes[j].options);
      }

      var data = { name: "Ben" };
      if (!objectStores[i].autoIncrement) {
        data.id = 1;
      }

      var result = objectStore.add(data);
      ok(result == 1 || result == 2, "Correct id");
    });
    db.close();
  }

  var db = indexedDBSync.open(name, j, function(trans, oldVersion) {});
  for (var i in objectStores) {
    for (var j in indexes) {

      db.transaction(objectStores[i].name, function(trans) {
        var objectStore = trans.objectStore(objectStores[i].name);
        var index = objectStore.index(indexes[j].name);

        var cursor = index.openCursor();
        if (cursor) {
          is(cursor.value.name, "Ben", "Correct object");
        }
      });

    }
  }

  info("Test successfully completed");
  postMessage(undefined);
};
