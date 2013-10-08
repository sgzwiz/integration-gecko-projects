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
    { key: "237-23-7737", value: { name: "Pat", height: 65 } }
  ];

  const indexData = [
    { name: "name", keyPath: "name", options: { unique: true } },
    { name: "height", keyPath: "height", options: { } },
    { name: "weight", keyPath: "weight", options: { unique: false } }
  ];

  const objectStoreDataNameSort = [
    { key: "237-23-7733", value: { name: "Ann", height: 52, weight: 110 } },
    { key: "237-23-7732", value: { name: "Bob", height: 60, weight: 120 } },
    { key: "237-23-7736", value: { name: "Joe", height: 65, weight: 150 } },
    { key: "237-23-7737", value: { name: "Pat", height: 65 } },
    { key: "237-23-7734", value: { name: "Ron", height: 73, weight: 180 } },
    { key: "237-23-7735", value: { name: "Sue", height: 58, weight: 130 } }
  ];

  const objectStoreDataWeightSort = [
    { key: "237-23-7733", value: { name: "Ann", height: 52, weight: 110 } },
    { key: "237-23-7732", value: { name: "Bob", height: 60, weight: 120 } },
    { key: "237-23-7735", value: { name: "Sue", height: 58, weight: 130 } },
    { key: "237-23-7736", value: { name: "Joe", height: 65, weight: 150 } },
    { key: "237-23-7734", value: { name: "Ron", height: 73, weight: 180 } }
  ];

  const objectStoreDataHeightSort = [
    { key: "237-23-7733", value: { name: "Ann", height: 52, weight: 110 } },
    { key: "237-23-7735", value: { name: "Sue", height: 58, weight: 130 } },
    { key: "237-23-7732", value: { name: "Bob", height: 60, weight: 120 } },
    { key: "237-23-7736", value: { name: "Joe", height: 65, weight: 150 } },
    { key: "237-23-7737", value: { name: "Pat", height: 65 } },
    { key: "237-23-7734", value: { name: "Ron", height: 73, weight: 180 } }
  ];

  var db = indexedDBSync.open(name, 1, function(trans, oldVersion) {
    var objectStore = trans.db.createObjectStore(objectStoreName, { keyPath: null });

    // First, add all our data to the object store.
    for (var i in objectStoreData) {
      objectStore.add(objectStoreData[i].value, objectStoreData[i].key);
    }

    // Now create the indexes.
    for (var i in indexData) {
      objectStore.createIndex(indexData[i].name, indexData[i].keyPath,
                              indexData[i].options);
    }
    ok(objectStore.indexNames.length == indexData.length, "Good index count");
  });


  db.transaction(objectStoreName, function(trans) {
    var objectStore = trans.objectStore(objectStoreName);

    // Check global properties to make sure they are correct.
    is(objectStore.indexNames.length, indexData.length, "Good index count");
    for (var i in indexData) {
      var found = false;
      for (var j = 0; j < objectStore.indexNames.length; j++) {
        if (objectStore.indexNames.item(j) == indexData[i].name) {
          found = true;
          break;
        }
      }
      is(found, true, "objectStore has our index");
      var index = objectStore.index(indexData[i].name);
      is(index.name, indexData[i].name, "Correct name");
      is(index.storeName, objectStore.name, "Correct store name");
      is(index.keyPath, indexData[i].keyPath, "Correct keyPath");
      is(index.unique, indexData[i].options.unique ? true : false,
         "Correct unique value");
    }

    var index = objectStore.index("name");
    var result = index.getKey("Bob");
    is(result, "237-23-7732", "Correct key returned");

    result = index.get("Bob");
    is(result.name, "Bob", "Correct name returned");
    is(result.height, 60, "Correct height returned");
    is(result.weight, 120, "Correct weight returned");

    info("Test group 1");

    var keyIndex = 0;

    var cursor = index.openKeyCursor();
    if (cursor) {
      do {
        is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
           "Correct key");
        is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
           "Correct primary key");
        ok(!("value" in cursor), "No value");

        keyIndex++;
      } while (cursor.continue());
    }

    is(keyIndex, objectStoreData.length, "Saw all the expected keys");

    info("Test group 2");

    keyIndex = 0;

    var index = objectStore.index("weight");
    cursor = index.openKeyCursor(null, "next");
    if (cursor) {
      do {
        is(cursor.key, objectStoreDataWeightSort[keyIndex].value.weight,
           "Correct key");
        is(cursor.primaryKey, objectStoreDataWeightSort[keyIndex].key,
           "Correct value");

        keyIndex++;
      } while (cursor.continue());

    }

    is(keyIndex, objectStoreData.length - 1, "Saw all the expected keys");
  });


  db.transaction(objectStoreName, function(trans) {
    var objectStore = trans.objectStore(objectStoreName);

    // Check that the name index enforces its unique constraint.
    expectException(function() {
      objectStore.add({ name: "Bob", height: 62, weight: 170 }, "237-23-7738");
    }, "ConstraintError");

    info("Test group 3");

    var nameIndex = objectStore.index("name");
    var heightIndex = objectStore.index("height");

    var keyIndex = objectStoreDataNameSort.length - 1;

    var cursor = nameIndex.openKeyCursor(null, "prev");
    if (cursor) {
      do {
        is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
           "Correct key");
        is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
           "Correct value");

        keyIndex--;
      } while (cursor.continue());
    }

    is(keyIndex, -1, "Saw all the expected keys");

    info("Test group 4");

    keyIndex = 1;
    var keyRange = IDBKeyRange.bound("Bob", "Ron");

    cursor = nameIndex.openKeyCursor(keyRange);
    if (cursor) {
      do {
        is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
           "Correct key");
        is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
           "Correct value");

        keyIndex++;
      } while (cursor.continue());
    }

    is(keyIndex, 5, "Saw all the expected keys");

    info("Test group 5");

    keyIndex = 2;
    var keyRange = IDBKeyRange.bound("Bob", "Ron", true);

    cursor = nameIndex.openKeyCursor(keyRange);
    if (cursor) {
      do {
        is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
           "Correct key");
        is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
           "Correct value");

        keyIndex++;
      } while (cursor.continue());
    }

    is(keyIndex, 5, "Saw all the expected keys");

    info("Test group 6");

    keyIndex = 1;
    var keyRange = IDBKeyRange.bound("Bob", "Ron", false, true);

    cursor = nameIndex.openKeyCursor(keyRange);
    if (cursor) {
      do {
        is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
           "Correct key");
        is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
           "Correct value");

        keyIndex++;
      } while (cursor.continue());
    }

    is(keyIndex, 4, "Saw all the expected keys");

    info("Test group 7");

    keyIndex = 2;
    keyRange = IDBKeyRange.bound("Bob", "Ron", true, true);

    cursor = nameIndex.openKeyCursor(keyRange);
    if (cursor) {
      do {
        is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
           "Correct key");
        is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
           "Correct value");

        keyIndex++;
      } while (cursor.continue());
    }

    is(keyIndex, 4, "Saw all the expected keys");

    info("Test group 8");

    keyIndex = 1;
    keyRange = IDBKeyRange.lowerBound("Bob");

    cursor = nameIndex.openKeyCursor(keyRange);
    if (cursor) {
      do {
        is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
           "Correct key");
        is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
           "Correct value");

        keyIndex++;
      } while (cursor.continue());
    }

    is(keyIndex, objectStoreDataNameSort.length, "Saw all the expected keys");

    info("Test group 9");

    keyIndex = 2;
    keyRange = IDBKeyRange.lowerBound("Bob", true);

    cursor = nameIndex.openKeyCursor(keyRange);
    if (cursor) {
      do {
        is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
           "Correct key");
        is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
           "Correct value");

        keyIndex++;
      } while (cursor.continue());
    }

    is(keyIndex, objectStoreDataNameSort.length, "Saw all the expected keys");

    info("Test group 10");

    keyIndex = 0;
    keyRange = IDBKeyRange.upperBound("Joe");

    cursor = nameIndex.openKeyCursor(keyRange);
    if (cursor) {
      do {
        is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
           "Correct key");
        is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
           "Correct value");

        keyIndex++;
      } while (cursor.continue());
    }

    is(keyIndex, 3, "Saw all the expected keys");

    info("Test group 11");

    keyIndex = 0;
    keyRange = IDBKeyRange.upperBound("Joe", true);

    cursor = nameIndex.openKeyCursor(keyRange);
    if (cursor) {
      do {
        is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
           "Correct key");
        is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
           "Correct value");

        keyIndex++;
      } while (cursor.continue());
    }

    is(keyIndex, 2, "Saw all the expected keys");

    info("Test group 12");

    keyIndex = 3;
    keyRange = IDBKeyRange.only("Pat");

    cursor = nameIndex.openKeyCursor(keyRange);
    if (cursor) {
      do {
        is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
           "Correct key");
        is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
           "Correct value");

        keyIndex++;
      } while (cursor.continue());
    }

    is(keyIndex, 4, "Saw all the expected keys");

    info("Test group 13");

    keyIndex = 0;

    cursor = nameIndex.openCursor();
    if (cursor) {
      do {
        is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
           "Correct key");
        is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
           "Correct primary key");
        is(cursor.value.name, objectStoreDataNameSort[keyIndex].value.name,
           "Correct name");
        is(cursor.value.height,
           objectStoreDataNameSort[keyIndex].value.height,
           "Correct height");
        if ("weight" in cursor.value) {
          is(cursor.value.weight,
             objectStoreDataNameSort[keyIndex].value.weight,
             "Correct weight");
        }

        keyIndex++;
      } while (cursor.continue());
    }

    is(keyIndex, objectStoreDataNameSort.length, "Saw all the expected keys");

    info("Test group 14");

    keyIndex = objectStoreDataNameSort.length - 1;

    cursor = nameIndex.openCursor(null, "prev");
    if (cursor) {
      do {
        is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
           "Correct key");
        is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
           "Correct primary key");
        is(cursor.value.name, objectStoreDataNameSort[keyIndex].value.name,
           "Correct name");
        is(cursor.value.height,
           objectStoreDataNameSort[keyIndex].value.height,
           "Correct height");
        if ("weight" in cursor.value) {
          is(cursor.value.weight,
             objectStoreDataNameSort[keyIndex].value.weight,
             "Correct weight");
        }

        keyIndex--;
      } while (cursor.continue());
    }

    is(keyIndex, -1, "Saw all the expected keys");

    info("Test group 15");

    keyIndex = 1;
    keyRange = IDBKeyRange.bound("Bob", "Ron");

    cursor = nameIndex.openCursor(keyRange);
    if (cursor) {
      do {
        is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
           "Correct key");
        is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
           "Correct primary key");
        is(cursor.value.name, objectStoreDataNameSort[keyIndex].value.name,
           "Correct name");
        is(cursor.value.height,
           objectStoreDataNameSort[keyIndex].value.height,
           "Correct height");
        if ("weight" in cursor.value) {
          is(cursor.value.weight,
             objectStoreDataNameSort[keyIndex].value.weight,
             "Correct weight");
        }

        keyIndex++;
      } while (cursor.continue());
    }

    is(keyIndex, 5, "Saw all the expected keys");

    info("Test group 16");

    keyIndex = 2;
    keyRange = IDBKeyRange.bound("Bob", "Ron", true);

    cursor = nameIndex.openCursor(keyRange);
    if (cursor) {
      do {
        is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
           "Correct key");
        is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
           "Correct primary key");
        is(cursor.value.name, objectStoreDataNameSort[keyIndex].value.name,
           "Correct name");
        is(cursor.value.height,
           objectStoreDataNameSort[keyIndex].value.height,
           "Correct height");
        if ("weight" in cursor.value) {
          is(cursor.value.weight,
             objectStoreDataNameSort[keyIndex].value.weight,
             "Correct weight");
        }

        keyIndex++;
      } while (cursor.continue());
    }

    is(keyIndex, 5, "Saw all the expected keys");

    info("Test group 17");

    keyIndex = 1;
    keyRange = IDBKeyRange.bound("Bob", "Ron", false, true);

    cursor = nameIndex.openCursor(keyRange);
    if (cursor) {
      do {
        is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
           "Correct key");
        is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
           "Correct primary key");
        is(cursor.value.name, objectStoreDataNameSort[keyIndex].value.name,
           "Correct name");
        is(cursor.value.height,
           objectStoreDataNameSort[keyIndex].value.height,
           "Correct height");
        if ("weight" in cursor.value) {
          is(cursor.value.weight,
             objectStoreDataNameSort[keyIndex].value.weight,
             "Correct weight");
        }

        keyIndex++;
      } while (cursor.continue());
    }

    is(keyIndex, 4, "Saw all the expected keys");

    info("Test group 18");

    keyIndex = 2;
    keyRange = IDBKeyRange.bound("Bob", "Ron", true, true);

    cursor = nameIndex.openCursor(keyRange);
    if (cursor) {
      do {
        is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
           "Correct key");
        is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
           "Correct primary key");
        is(cursor.value.name, objectStoreDataNameSort[keyIndex].value.name,
           "Correct name");
        is(cursor.value.height,
           objectStoreDataNameSort[keyIndex].value.height,
           "Correct height");
        if ("weight" in cursor.value) {
          is(cursor.value.weight,
             objectStoreDataNameSort[keyIndex].value.weight,
             "Correct weight");
        }

        keyIndex++;
      } while (cursor.continue());
    }

    is(keyIndex, 4, "Saw all the expected keys");

    info("Test group 19");

    keyIndex = 4;
    keyRange = IDBKeyRange.bound("Bob", "Ron");

    cursor = nameIndex.openCursor(keyRange, "prev");
    if (cursor) {
      do {
        is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
           "Correct key");
        is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
           "Correct primary key");
        is(cursor.value.name, objectStoreDataNameSort[keyIndex].value.name,
           "Correct name");
        is(cursor.value.height,
           objectStoreDataNameSort[keyIndex].value.height,
           "Correct height");
        if ("weight" in cursor.value) {
          is(cursor.value.weight,
             objectStoreDataNameSort[keyIndex].value.weight,
             "Correct weight");
        }

        keyIndex--;
      } while (cursor.continue());
    }

    is(keyIndex, 0, "Saw all the expected keys");

    info("Test group 20");

    // Test "nextunique"
    keyIndex = 3;
    keyRange = IDBKeyRange.only(65);

    cursor = heightIndex.openKeyCursor(keyRange, "next");
    if (cursor) {
      do {
        is(cursor.key, objectStoreDataHeightSort[keyIndex].value.height,
           "Correct key");
        is(cursor.primaryKey, objectStoreDataHeightSort[keyIndex].key,
           "Correct value");

        keyIndex++;
      } while (cursor.continue());
    }

    is(keyIndex, 5, "Saw all the expected keys");

    info("Test group 21");

    keyIndex = 3;
    keyRange = IDBKeyRange.only(65);

    cursor = heightIndex.openKeyCursor(keyRange, "nextunique");
    if (cursor) {
      do {
        is(cursor.key, objectStoreDataHeightSort[keyIndex].value.height,
           "Correct key");
        is(cursor.primaryKey, objectStoreDataHeightSort[keyIndex].key,
           "Correct value");

        keyIndex++;
      } while (cursor.continue());
    }

    is(keyIndex, 4, "Saw all the expected keys");

    info("Test group 21.5");

    keyIndex = 5;
    cursor = heightIndex.openKeyCursor(null, "prev");
    if (cursor) {
      do {
        is(cursor.key, objectStoreDataHeightSort[keyIndex].value.height,
           "Correct key");
        is(cursor.primaryKey, objectStoreDataHeightSort[keyIndex].key,
           "Correct value");

        keyIndex--;
      } while (cursor.continue());
    }

    is(keyIndex, -1, "Saw all the expected keys");

    info("Test group 22");

    keyIndex = 5;

    cursor = heightIndex.openKeyCursor(null, "prevunique");
    if (cursor) {
      do {
        is(cursor.key, objectStoreDataHeightSort[keyIndex].value.height,
           "Correct key");
        is(cursor.primaryKey, objectStoreDataHeightSort[keyIndex].key,
           "Correct value");

        if (keyIndex == 5) {
          keyIndex--;
        }
        keyIndex--;
      } while (cursor.continue());
    }

    is(keyIndex, -1, "Saw all the expected keys");

    info("Test group 23");

    keyIndex = 3;
    keyRange = IDBKeyRange.only(65);

    cursor = heightIndex.openCursor(keyRange, "next");
    if (cursor) {
      do {
        is(cursor.key, objectStoreDataHeightSort[keyIndex].value.height,
           "Correct key");
        is(cursor.primaryKey, objectStoreDataHeightSort[keyIndex].key,
           "Correct primary key");
        is(cursor.value.name, objectStoreDataHeightSort[keyIndex].value.name,
           "Correct name");
        is(cursor.value.height,
           objectStoreDataHeightSort[keyIndex].value.height,
           "Correct height");
        if ("weight" in cursor.value) {
          is(cursor.value.weight,
             objectStoreDataHeightSort[keyIndex].value.weight,
             "Correct weight");
        }

        keyIndex++;
      } while (cursor.continue());
    }

    is(keyIndex, 5, "Saw all the expected keys");

    info("Test group 24");

    keyIndex = 3;
    keyRange = IDBKeyRange.only(65);

    cursor = heightIndex.openCursor(keyRange, "nextunique");
    if (cursor) {
      do {
        is(cursor.key, objectStoreDataHeightSort[keyIndex].value.height,
           "Correct key");
        is(cursor.primaryKey, objectStoreDataHeightSort[keyIndex].key,
           "Correct primary key");
        is(cursor.value.name, objectStoreDataHeightSort[keyIndex].value.name,
           "Correct name");
        is(cursor.value.height,
           objectStoreDataHeightSort[keyIndex].value.height,
           "Correct height");
        if ("weight" in cursor.value) {
          is(cursor.value.weight,
             objectStoreDataHeightSort[keyIndex].value.weight,
             "Correct weight");
        }

        keyIndex++;
      } while (cursor.continue());
    }

    is(keyIndex, 4, "Saw all the expected keys");

    info("Test group 24.5");

    keyIndex = 5;

    cursor = heightIndex.openCursor(null, "prev");
    if (cursor) {
      do {
        is(cursor.key, objectStoreDataHeightSort[keyIndex].value.height,
           "Correct key");
        is(cursor.primaryKey, objectStoreDataHeightSort[keyIndex].key,
           "Correct primary key");
        is(cursor.value.name, objectStoreDataHeightSort[keyIndex].value.name,
           "Correct name");
        is(cursor.value.height,
           objectStoreDataHeightSort[keyIndex].value.height,
           "Correct height");
        if ("weight" in cursor.value) {
          is(cursor.value.weight,
             objectStoreDataHeightSort[keyIndex].value.weight,
             "Correct weight");
        }

        keyIndex--;
      } while (cursor.continue());
    }

    is(keyIndex, -1, "Saw all the expected keys");

    info("Test group 25");

    keyIndex = 5;

    cursor = heightIndex.openCursor(null, "prevunique");
    if (cursor) {
      do {
        is(cursor.key, objectStoreDataHeightSort[keyIndex].value.height,
           "Correct key");
        is(cursor.primaryKey, objectStoreDataHeightSort[keyIndex].key,
           "Correct primary key");
        is(cursor.value.name, objectStoreDataHeightSort[keyIndex].value.name,
           "Correct name");
        is(cursor.value.height,
           objectStoreDataHeightSort[keyIndex].value.height,
           "Correct height");
        if ("weight" in cursor.value) {
          is(cursor.value.weight,
             objectStoreDataHeightSort[keyIndex].value.weight,
             "Correct weight");
        }

        if (keyIndex == 5) {
          keyIndex--;
        }
        keyIndex--;
      } while (cursor.continue());
    }

    is(keyIndex, -1, "Saw all the expected keys");

    info("Test group 26");

    keyIndex = 0;

    cursor = nameIndex.openKeyCursor();
    if (cursor) {
      do {
        is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
           "Correct key");
        is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
           "Correct value");

        var nextKey = !keyIndex ? "Pat" : undefined;

        if (!keyIndex) {
          keyIndex = 3;
        }
        else {
          keyIndex++;
        }
      } while (cursor.continue(nextKey));
    }

    is(keyIndex, objectStoreData.length, "Saw all the expected keys");

    info("Test group 27");

    keyIndex = 0;

    cursor = nameIndex.openKeyCursor();
    if (cursor) {
      do {
        is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
           "Correct key");
        is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
           "Correct value");

        var nextKey = !keyIndex ? "Flo" : undefined;

        keyIndex += keyIndex ? 1 : 2;
      } while (cursor.continue(nextKey));
    }

    is(keyIndex, objectStoreData.length, "Saw all the expected keys");

    info("Test group 28");

    keyIndex = 0;

    cursor = nameIndex.openCursor();
    if (cursor) {
      do {
        is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
           "Correct key");
        is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
           "Correct primary key");
        is(cursor.value.name, objectStoreDataNameSort[keyIndex].value.name,
           "Correct name");
        is(cursor.value.height,
           objectStoreDataNameSort[keyIndex].value.height,
           "Correct height");
        if ("weight" in cursor.value) {
          is(cursor.value.weight,
             objectStoreDataNameSort[keyIndex].value.weight,
             "Correct weight");
        }

        var nextKey = !keyIndex ? "Pat" : undefined;

        if (!keyIndex) {
          keyIndex = 3;
        }
        else {
          keyIndex++;
        }
      } while (cursor.continue(nextKey));

    }

    is(keyIndex, objectStoreDataNameSort.length, "Saw all the expected keys");

    info("Test group 29");

    keyIndex = 0;

    cursor = nameIndex.openCursor();
    if (cursor) {
      do {
        is(cursor.key, objectStoreDataNameSort[keyIndex].value.name,
           "Correct key");
        is(cursor.primaryKey, objectStoreDataNameSort[keyIndex].key,
           "Correct primary key");
        is(cursor.value.name, objectStoreDataNameSort[keyIndex].value.name,
           "Correct name");
        is(cursor.value.height,
           objectStoreDataNameSort[keyIndex].value.height,
           "Correct height");
        if ("weight" in cursor.value) {
          is(cursor.value.weight,
             objectStoreDataNameSort[keyIndex].value.weight,
             "Correct weight");
        }

        var nextKey = !keyIndex ? "Flo" : undefined;

        keyIndex += keyIndex ? 1 : 2;
      } while (cursor.continue(nextKey));
    }

    is(keyIndex, objectStoreDataNameSort.length, "Saw all the expected keys");
  }, "readwrite");

  info("Test successfully completed");
  postMessage(undefined);
};
