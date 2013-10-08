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
    { key: "237-23-7738", value: { name: "Mel", height: 66, weight: {} } },
    { key: "237-23-7739", value: { name: "Tom", height: 62, weight: 130 } }
  ];

  const indexData = {
    name: "weight",
    keyPath: "weight",
    options: { unique: false }
  };

  const weightSort = [1, 0, 3, 7, 4, 2];

  var db = indexedDBSync.open(name, 1, function(trans, oldVersion) {
    var objectStore = trans.db.createObjectStore(objectStoreName, { });

    objectStore.createIndex(indexData.name, indexData.keyPath,
                            indexData.options);

    for (var i in objectStoreData) {
      objectStore.add(objectStoreData[i].value, objectStoreData[i].key);
    }
  });

  db.transaction(objectStoreName, function(trans) {
    var objectStore = trans.objectStore(objectStoreName);

    var count = objectStore.count();
    is(count, objectStoreData.length,
     "Correct number of object store entries for all keys");

    count = objectStore.count(null);
    is(count, objectStoreData.length,
     "Correct number of object store entries for null key");

    count = objectStore.count(objectStoreData[2].key);
    is(count, 1,
     "Correct number of object store entries for single existing key");

    count = objectStore.count("foo");
    is(count, 0,
       "Correct number of object store entries for single non-existing key");

    var keyRange = IDBKeyRange.only(objectStoreData[2].key);
    count = objectStore.count(keyRange);
    is(count, 1,
       "Correct number of object store entries for existing only keyRange");

    keyRange = IDBKeyRange.only("foo");
    count = objectStore.count(keyRange);
    is(count, 0,
       "Correct number of object store entries for non-existing only keyRange");

    keyRange = IDBKeyRange.lowerBound(objectStoreData[2].key);
    count = objectStore.count(keyRange);
    is(count, objectStoreData.length - 2,
       "Correct number of object store entries for lowerBound keyRange");

    keyRange = IDBKeyRange.lowerBound(objectStoreData[2].key, true);
    count = objectStore.count(keyRange);
    is(count, objectStoreData.length - 3,
       "Correct number of object store entries for lowerBound keyRange");

    keyRange = IDBKeyRange.lowerBound("foo");
    count = objectStore.count(keyRange);
    is(count, 0,
       "Correct number of object store entries for lowerBound keyRange");

    keyRange = IDBKeyRange.upperBound(objectStoreData[2].key, false);
    count = objectStore.count(keyRange);
    is(count, 3,
       "Correct number of object store entries for upperBound keyRange");

    keyRange = IDBKeyRange.upperBound(objectStoreData[2].key, true);
    count = objectStore.count(keyRange);
    is(count, 2,
       "Correct number of object store entries for upperBound keyRange");

    keyRange = IDBKeyRange.upperBound("foo", true);
    count = objectStore.count(keyRange);
    is(count, objectStoreData.length,
       "Correct number of object store entries for upperBound keyRange");

    keyRange = IDBKeyRange.bound(objectStoreData[0].key,
                                 objectStoreData[objectStoreData.length - 1].key);
    count = objectStore.count(keyRange);
    is(count, objectStoreData.length,
       "Correct number of object store entries for bound keyRange");

    keyRange = IDBKeyRange.bound(objectStoreData[0].key,
                                 objectStoreData[objectStoreData.length - 1].key,
                                 true);
    count = objectStore.count(keyRange);
    is(count, objectStoreData.length - 1,
       "Correct number of object store entries for bound keyRange");

    keyRange = IDBKeyRange.bound(objectStoreData[0].key,
                                 objectStoreData[objectStoreData.length - 1].key,
                                 true, true);
    count = objectStore.count(keyRange);
    is(count, objectStoreData.length - 2,
       "Correct number of object store entries for bound keyRange");

    keyRange = IDBKeyRange.bound("foo", "foopy", true, true);
    count = objectStore.count(keyRange);
    is(count, 0,
       "Correct number of object store entries for bound keyRange");

    keyRange = IDBKeyRange.bound(objectStoreData[0].key, "foo", true, true);
    count = objectStore.count(keyRange);
    is(count, objectStoreData.length - 1,
       "Correct number of object store entries for bound keyRange");

    var index = objectStore.index(indexData.name);

    count = index.count();
    is(count, weightSort.length, "Correct number of index entries for no key");

    count = index.count(objectStoreData[7].value.weight);
    is(count, 2, "Correct number of index entries for duplicate key");

    count = index.count(objectStoreData[0].value.weight);
    is(count, 1, "Correct number of index entries for single key");

    keyRange = IDBKeyRange.only(objectStoreData[0].value.weight);
    count = index.count(keyRange);
    is(count, 1, "Correct number of index entries for only existing keyRange");

    keyRange = IDBKeyRange.only("foo");
    count = index.count(keyRange);
    is(count, 0, "Correct number of index entries for only non-existing keyRange");

    keyRange = IDBKeyRange.only(objectStoreData[7].value.weight);
    count = index.count(keyRange);
    is(count, 2, "Correct number of index entries for only duplicate keyRange");

    keyRange = IDBKeyRange.lowerBound(objectStoreData[weightSort[0]].value.weight);
    count = index.count(keyRange);
    is(count, weightSort.length,
       "Correct number of index entries for lowerBound keyRange");

    keyRange = IDBKeyRange.lowerBound(objectStoreData[weightSort[1]].value.weight);
    count = index.count(keyRange)
    is(count, weightSort.length - 1,
       "Correct number of index entries for lowerBound keyRange");

    keyRange = IDBKeyRange.lowerBound(objectStoreData[weightSort[0]].value.weight - 1);
    count = index.count(keyRange);
    is(count, weightSort.length,
       "Correct number of index entries for lowerBound keyRange");

    keyRange = IDBKeyRange.lowerBound(objectStoreData[weightSort[0]].value.weight,
                                      true);
    count = index.count(keyRange);
    is(count, weightSort.length - 1,
       "Correct number of index entries for lowerBound keyRange");

    keyRange = IDBKeyRange.lowerBound(objectStoreData[weightSort[weightSort.length - 1]].value.weight);
    count = index.count(keyRange);
    is(count, 1,
       "Correct number of index entries for lowerBound keyRange");

    keyRange = IDBKeyRange.lowerBound(objectStoreData[weightSort[weightSort.length - 1]].value.weight,
                                      true);
    count = index.count(keyRange);
    is(count, 0,
       "Correct number of index entries for lowerBound keyRange");

    keyRange = IDBKeyRange.lowerBound(objectStoreData[weightSort[weightSort.length - 1]].value.weight + 1,
                                      true);
    count = index.count(keyRange);
    is(count, 0,
       "Correct number of index entries for lowerBound keyRange");

    keyRange = IDBKeyRange.upperBound(objectStoreData[weightSort[0]].value.weight);
    count = index.count(keyRange);
    is(count, 1,
       "Correct number of index entries for upperBound keyRange");

    keyRange = IDBKeyRange.upperBound(objectStoreData[weightSort[0]].value.weight,
                                      true);
    count = index.count(keyRange);
    is(count, 0,
       "Correct number of index entries for upperBound keyRange");

    keyRange = IDBKeyRange.upperBound(objectStoreData[weightSort[weightSort.length - 1]].value.weight);
    count = index.count(keyRange);
    is(count, weightSort.length,
       "Correct number of index entries for upperBound keyRange");

    keyRange = IDBKeyRange.upperBound(objectStoreData[weightSort[weightSort.length - 1]].value.weight,
                                      true);
    count = index.count(keyRange);
    is(count, weightSort.length - 1,
       "Correct number of index entries for upperBound keyRange");

    keyRange = IDBKeyRange.upperBound(objectStoreData[weightSort[weightSort.length - 1]].value.weight,
                                      true);
    count = index.count(keyRange);
    is(count, weightSort.length - 1,
       "Correct number of index entries for upperBound keyRange");

    keyRange = IDBKeyRange.upperBound("foo");
    count = index.count(keyRange);
    is(count, weightSort.length,
       "Correct number of index entries for upperBound keyRange");

    keyRange = IDBKeyRange.bound("foo", "foopy");
    count = index.count(keyRange);
    is(count, 0,
       "Correct number of index entries for bound keyRange");

    keyRange = IDBKeyRange.bound(objectStoreData[weightSort[0]].value.weight,
                                 objectStoreData[weightSort[weightSort.length - 1]].value.weight);
    count = index.count(keyRange);
    is(count, weightSort.length,
       "Correct number of index entries for bound keyRange");

    keyRange = IDBKeyRange.bound(objectStoreData[weightSort[0]].value.weight,
                                 objectStoreData[weightSort[weightSort.length - 1]].value.weight,
                                 true);
    count = index.count(keyRange);
    is(count, weightSort.length - 1,
       "Correct number of index entries for bound keyRange");

    keyRange = IDBKeyRange.bound(objectStoreData[weightSort[0]].value.weight,
                                 objectStoreData[weightSort[weightSort.length - 1]].value.weight,
                                 true, true);
    count = index.count(keyRange);
    is(count, weightSort.length - 2,
       "Correct number of index entries for bound keyRange");
  });

  info("Test successfully completed");
  postMessage(undefined);
};
