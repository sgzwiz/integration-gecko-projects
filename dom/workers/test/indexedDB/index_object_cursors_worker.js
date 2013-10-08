/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;

  const objectStoreData = [
    { name: "", options: { keyPath: "id", autoIncrement: true } },
    { name: null, options: { keyPath: "ss" } },
    { name: undefined, options: { } },
    { name: "4", options: { autoIncrement: true } }
  ];

  const indexData = [
    { name: "", keyPath: "name", options: { unique: true } },
    { name: null, keyPath: "height", options: { } }
  ];

  const data = [
    { ss: "237-23-7732", name: "Ann", height: 60 },
    { ss: "237-23-7733", name: "Bob", height: 65 }
  ];

  var db = indexedDBSync.open(name, 1, function(trans, oldVersion) {
    for (var objectStoreIndex in objectStoreData) {
      const objectStoreInfo = objectStoreData[objectStoreIndex];
      var objectStore = trans.db.createObjectStore(objectStoreInfo.name,
                                                   objectStoreInfo.options);
      for (var indexIndex in indexData) {
        const indexInfo = indexData[indexIndex];
        var index = objectStore.createIndex(indexInfo.name,
                                            indexInfo.keyPath,
                                            indexInfo.options);
      }
    }
  });

  info("Initial setup");

  for (var objectStoreIndex in objectStoreData) {
    const item = objectStoreData[objectStoreIndex];

    for (var indexIndex in indexData) {
      const objectStoreName = objectStoreData[objectStoreIndex].name;
      const indexName = indexData[indexIndex].name;

      db.transaction(objectStoreName, function(trans) {
        var objectStore = trans.objectStore(objectStoreName);
        info("Got objectStore " + objectStoreName);

        for (var dataIndex in data) {
          const obj1 = data[dataIndex];
          var key;
          if (!item.options.keyPath && !item.options.autoIncrement) {
            key = obj1.ss;
          }
          objectStore.add(obj1, key);
        }

        var index = objectStore.index(indexName);
        info("Got index " + indexName);

        var keyIndex = 0;

        var cursor = index.openCursor();
        if (cursor) {
          do {
            is(cursor.key, data[keyIndex][indexData[indexIndex].keyPath],
               "Good key");
            is(cursor.value.ss, data[keyIndex].ss, "Correct ss");
            is(cursor.value.name, data[keyIndex].name, "Correct name");
            is(cursor.value.height, data[keyIndex].height, "Correct height");

            if (!keyIndex) {
              var obj = cursor.value;
              obj.updated = true;

              cursor.update(obj);
              info("Object updated");
              keyIndex++;
              continue;
            }

            cursor.delete();
            info("Object deleted");
            keyIndex++;
          } while(cursor.continue());
        }
        is(keyIndex, 2, "Saw all the items");
      },"readwrite");

      db.transaction(objectStoreName, function(trans) {
        var objectStore = trans.objectStore(objectStoreName);
        var keyIndex = 0;
        var cursor = objectStore.openCursor();
        if (cursor) {
          do {
            is(cursor.value.ss, data[keyIndex].ss, "Correct ss");
            is(cursor.value.name, data[keyIndex].name, "Correct name");
            is(cursor.value.height, data[keyIndex].height, "Correct height");
            is(cursor.value.updated, true, "Correct updated flag");
            keyIndex++;
          } while(cursor.continue());
        }
        is(keyIndex, 1, "Saw all the items");
      });


      db.transaction(objectStoreName, function(trans) {
        var objectStore = trans.objectStore(objectStoreName);
        objectStore.clear();
      },"readwrite");
    }
  }

  info("Test successfully completed");
  postMessage(undefined);
};
