/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;
  const objectStoreName = "People";

  const objectStoreData = [
    { key: "237-23-7732", value: { name: "Bob", height: 60, weight: 120 } },
    { key: "237-23-7733", value: { name: "Ann", height: 52, weight: 110 } },
    { key: "237-23-7734", value: { name: "Ron", height: 73, weight: 180 } },
    { key: "237-23-7735", value: { name: "Sue", height: 58, weight: 130 } },
    { key: "237-23-7736", value: { name: "Joe", height: 65, weight: 150 } },
    { key: "237-23-7737", value: { name: "Pat", height: 65 } },
    { key: "237-23-7738", value: { name: "Mel", height: 66, weight: {} } }
  ];

  const badObjectStoreData = [
    { key: "237-23-7739", value: { name: "Rob", height: 65 } },
    { key: "237-23-7740", value: { name: "Jen", height: 66, weight: {} } }
  ];

  const indexData = [
    { name: "weight", keyPath: "weight", options: { unique: false } }
  ];

  const objectStoreDataWeightSort = [
    { key: "237-23-7733", value: { name: "Ann", height: 52, weight: 110 } },
    { key: "237-23-7732", value: { name: "Bob", height: 60, weight: 120 } },
    { key: "237-23-7735", value: { name: "Sue", height: 58, weight: 130 } },
    { key: "237-23-7736", value: { name: "Joe", height: 65, weight: 150 } },
    { key: "237-23-7734", value: { name: "Ron", height: 73, weight: 180 } }
  ];

  var db = indexedDBSync.open(name, 1, function(trans, oldVersion) {
    var objectStore = trans.db.createObjectStore(objectStoreName, { } );

    for (var i in objectStoreData) {
      objectStore.add(objectStoreData[i].value, objectStoreData[i].key);
    }

    for (var i in indexData) {
      objectStore.createIndex(indexData[i].name, indexData[i].keyPath,
                              indexData[i].options);
    }

    addedData = 0;
    for (var i in badObjectStoreData) {
      objectStore.add(badObjectStoreData[i].value, badObjectStoreData[i].key);
    }
  });

  db.transaction(objectStoreName, function(trans) {
    objectStore = trans.objectStore(objectStoreName);
    var keyIndex = 0;
    var index = objectStore.index("weight");

    var cursor = index.openKeyCursor();
    if (cursor) {
      do {
        is(cursor.key, objectStoreDataWeightSort[keyIndex].value.weight,
           "Correct key");
        is(cursor.primaryKey, objectStoreDataWeightSort[keyIndex].key,
           "Correct value");
        keyIndex++;
      } while (cursor.continue());
    }

    is(keyIndex, objectStoreDataWeightSort.length, "Saw all weights");

    keyIndex = 0;
    cursor = objectStore.openCursor();
    if (cursor) {
      do {
        keyIndex++;
      } while (cursor.continue());
    }

    is(keyIndex, objectStoreData.length + badObjectStoreData.length,
       "Saw all people");

  });

  info("Test successfully completed");
  postMessage(undefined);
};
