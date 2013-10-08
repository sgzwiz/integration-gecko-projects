/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;
  const osName = "people";
  const indexName = "weight";

  const data = [
    { ssn: "237-23-7732", name: "Bob", height: 60, weight: 120 },
    { ssn: "237-23-7733", name: "Ann", height: 52, weight: 110 },
    { ssn: "237-23-7734", name: "Ron", height: 73, weight: 180 },
    { ssn: "237-23-7735", name: "Sue", height: 58, weight: 130 },
    { ssn: "237-23-7736", name: "Joe", height: 65, weight: 150 },
    { ssn: "237-23-7737", name: "Pat", height: 65 },
    { ssn: "237-23-7738", name: "Mel", height: 66, weight: {} },
    { ssn: "237-23-7739", name: "Tom", height: 62, weight: 130 }
  ];

  const weightSort = [1, 0, 3, 7, 4, 2];

  var db = indexedDBSync.open(name, 1, function(trans, oldVersion) {
    var objectStore = trans.db.createObjectStore(osName, { keyPath: "ssn" });
    objectStore.createIndex(indexName, "weight", { unique: false });

    for (var i in data) {
      objectStore.add(data[i]);
    }

    expectException( function() {
      IDBKeyRange.bound(1, -1);
      ok(false, "Bound keyRange with backwards args should throw!");
    }, "DataError", 0);

    try {
      IDBKeyRange.bound(1, 1);
      ok(true, "Bound keyRange with same arg should be ok");
    }
    catch (e) {
      ok(false, "Bound keyRange with same arg should have been ok");
    }

    expectException( function() {
      IDBKeyRange.bound(1, 1, true);
      ok(false, "Bound keyRange with same arg and open should throw");
    }, "DataError", 0);

     expectException( function() {
      IDBKeyRange.bound(1, 1, true, true);
      ok(false, "Bound keyRange with same arg and open should throw");
    }, "DataError", 0);
  });

  db.transaction(osName, function(trans) {
    var objectStore = trans.objectStore(osName);

    try {
      objectStore.get();
      ok(false, "Get with unspecified arg should have thrown");
    }
    catch(e) {
      ok(true, "Get with unspecified arg threw");
    }

    try {
      objectStore.get(undefined);
      ok(false, "Get with undefined should have threwn");
    }
    catch(e) {
      ok(true, "Get with undefined arg threw");
    }

    expectException( function() {
      objectStore.get(null);
      ok(false, "Get with null should have thrown");
    }, "DataError", 0);

    var request = objectStore.get(data[2].ssn);

    is(request.name, data[2].name, "Correct data");

    var keyRange = IDBKeyRange.only(data[2].ssn);

    request = objectStore.get(keyRange);

    is(request.name, data[2].name, "Correct data");

    keyRange = IDBKeyRange.lowerBound(data[2].ssn);

    request = objectStore.get(keyRange);
    is(request.name, data[2].name, "Correct data");

    keyRange = IDBKeyRange.lowerBound(data[2].ssn, true);

    request = objectStore.get(keyRange);
    is(request.name, data[3].name, "Correct data");

    keyRange = IDBKeyRange.upperBound(data[2].ssn);

    request = objectStore.get(keyRange)
    is(request.name, data[0].name, "Correct data");

    keyRange = IDBKeyRange.bound(data[2].ssn, data[4].ssn);

    request = objectStore.get(keyRange);
    is(request.name, data[2].name, "Correct data");

    keyRange = IDBKeyRange.bound(data[2].ssn, data[4].ssn, true);

    request = objectStore.get(keyRange);
    is(request.name, data[3].name, "Correct data");
  });



  db.transaction(osName, function(trans) {
    var objectStore = trans.objectStore(osName);

    try {
      objectStore.delete();
      ok(false, "Delete with unspecified arg should have thrown");
    }
    catch(e) {
      ok(true, "Delete with unspecified arg threw");
    }

    try {
      objectStore.delete(undefined);
      ok(false, "Delete with undefined should have thrown");
    }
    catch(e) {
      ok(true, "Delete with undefined threw");
    }

    expectException( function() {
      objectStore.delete(null);
      ok(false, "Delete with null should have thrown");
    }, "DataError", 0);

    request = objectStore.count();
    is(request, data.length, "Correct count");

    request = objectStore.delete(data[2].ssn);
    ok(request == undefined, "Correct result");

    request = objectStore.count();
    is(request, data.length - 1, "Correct count");

    keyRange = IDBKeyRange.bound(data[3].ssn, data[5].ssn);
    request = objectStore.delete(keyRange);
    ok(request == undefined, "Correct result");

    request = objectStore.count();
    is(request, data.length - 4, "Correct count");

    keyRange = IDBKeyRange.lowerBound(10);
    request = objectStore.delete(keyRange);
    ok(request == undefined, "Correct result");

    request = objectStore.count();
    is(request, 0, "Correct count");

    for (var i in data) {
      objectStore.add(data[i]);
    }
  },"readwrite");



  db.transaction(osName, function(trans) {
    var objectStore = trans.objectStore(osName);
    var request = objectStore.count();
    is(request, data.length, "Correct count");

    var count = 0;
    var cursor = objectStore.openCursor();
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, data.length, "Correct count for no arg to openCursor");

    count = 0;
    cursor = objectStore.openCursor(null);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, data.length, "Correct count for null arg to openCursor");

    count = 0;
    cursor = objectStore.openCursor(undefined);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, data.length, "Correct count for undefined arg to openCursor");

    count = 0;
    cursor = objectStore.openCursor(data[2].ssn);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, 1, "Correct count for single key arg to openCursor");

    count = 0;
    cursor = objectStore.openCursor("foo");
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, 0,
       "Correct count for non-existent single key arg to openCursor");

    count = 0;
    keyRange = IDBKeyRange.only(data[2].ssn);
    cursor = objectStore.openCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, 1, "Correct count for only keyRange arg to openCursor");

    count = 0;
    keyRange = IDBKeyRange.lowerBound(data[2].ssn);
    cursor = objectStore.openCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, data.length - 2,
       "Correct count for lowerBound arg to openCursor");

    count = 0;
    keyRange = IDBKeyRange.lowerBound(data[2].ssn, true);
    cursor = objectStore.openCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, data.length - 3,
       "Correct count for lowerBound arg to openCursor");

    count = 0;
    keyRange = IDBKeyRange.lowerBound("foo");
    cursor = objectStore.openCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, 0,
       "Correct count for non-existent lowerBound arg to openCursor");

    count = 0;
    keyRange = IDBKeyRange.bound(data[2].ssn, data[3].ssn);
    cursor = objectStore.openCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, 2, "Correct count for bound arg to openCursor");

    count = 0;
    keyRange = IDBKeyRange.bound(data[2].ssn, data[3].ssn, true);
    cursor = objectStore.openCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, 1, "Correct count for bound arg to openCursor");

    count = 0;
    keyRange = IDBKeyRange.bound(data[2].ssn, data[3].ssn, true, true);
    cursor = objectStore.openCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, 0, "Correct count for bound arg to openCursor");

    var index = objectStore.index(indexName);

    count = 0;
    cursor = index.openKeyCursor();
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, weightSort.length,
       "Correct count for unspecified arg to index.openKeyCursor");

    count = 0;
    cursor = index.openKeyCursor(null);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, weightSort.length,
       "Correct count for null arg to index.openKeyCursor");

    count = 0;
    cursor = index.openKeyCursor(undefined);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, weightSort.length,
       "Correct count for undefined arg to index.openKeyCursor");

    count = 0;
    cursor = index.openKeyCursor(data[0].weight);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, 1, "Correct count for single key arg to index.openKeyCursor");

    count = 0;
    cursor = index.openKeyCursor("foo");
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, 0,
       "Correct count for non-existent key arg to index.openKeyCursor");

    count = 0;
    keyRange = IDBKeyRange.only("foo");
    cursor = index.openKeyCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, 0,
     "Correct count for non-existent keyRange arg to index.openKeyCursor");

    count = 0;
    keyRange = IDBKeyRange.only(data[0].weight);
    cursor = index.openKeyCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, 1,
       "Correct count for only keyRange arg to index.openKeyCursor");

    count = 0;
    keyRange = IDBKeyRange.lowerBound(data[weightSort[0]].weight);
    cursor = index.openKeyCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, weightSort.length,
       "Correct count for lowerBound keyRange arg to index.openKeyCursor");

    count = 0;
    keyRange = IDBKeyRange.lowerBound(data[weightSort[0]].weight, true);
    cursor = index.openKeyCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, weightSort.length - 1,
       "Correct count for lowerBound keyRange arg to index.openKeyCursor");

    count = 0;
    keyRange = IDBKeyRange.lowerBound("foo");
    cursor = index.openKeyCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, 0,
       "Correct count for lowerBound keyRange arg to index.openKeyCursor");

    count = 0;
    keyRange = IDBKeyRange.upperBound(data[weightSort[0]].weight);
    cursor = index.openKeyCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, 1,
       "Correct count for upperBound keyRange arg to index.openKeyCursor");

    count = 0;
    keyRange = IDBKeyRange.upperBound(data[weightSort[0]].weight, true);
    cursor = index.openKeyCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, 0,
       "Correct count for upperBound keyRange arg to index.openKeyCursor");

    count = 0;
    keyRange = IDBKeyRange.upperBound(data[weightSort[weightSort.length - 1]].weight);
    cursor = index.openKeyCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, weightSort.length,
       "Correct count for upperBound keyRange arg to index.openKeyCursor");

    count = 0;
    keyRange = IDBKeyRange.upperBound(data[weightSort[weightSort.length - 1]].weight,
                                      true);

    cursor = index.openKeyCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, weightSort.length - 1,
       "Correct count for upperBound keyRange arg to index.openKeyCursor");

    count = 0;
    keyRange = IDBKeyRange.upperBound("foo");
    cursor = index.openKeyCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, weightSort.length,
       "Correct count for upperBound keyRange arg to index.openKeyCursor");

    count = 0;
    keyRange = IDBKeyRange.upperBound(0);
    cursor = index.openKeyCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, 0,
       "Correct count for upperBound keyRange arg to index.openKeyCursor");

    count = 0;
    keyRange = IDBKeyRange.bound(data[weightSort[0]].weight,
                                 data[weightSort[weightSort.length - 1]].weight);
    cursor = index.openKeyCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, weightSort.length,
       "Correct count for bound keyRange arg to index.openKeyCursor");

    count = 0;
    keyRange = IDBKeyRange.bound(data[weightSort[0]].weight,
                                 data[weightSort[weightSort.length - 1]].weight,
                                 true);
    cursor = index.openKeyCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, weightSort.length - 1,
       "Correct count for bound keyRange arg to index.openKeyCursor");

    count = 0;
    keyRange = IDBKeyRange.bound(data[weightSort[0]].weight,
                                 data[weightSort[weightSort.length - 1]].weight,
                                 true, true);
    cursor = index.openKeyCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, weightSort.length - 2,
       "Correct count for bound keyRange arg to index.openKeyCursor");

    count = 0;
    keyRange = IDBKeyRange.bound(data[weightSort[0]].weight - 1,
                                 data[weightSort[weightSort.length - 1]].weight + 1);
    cursor = index.openKeyCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, weightSort.length,
       "Correct count for bound keyRange arg to index.openKeyCursor");

    count = 0;
    keyRange = IDBKeyRange.bound(data[weightSort[0]].weight - 2,
                                 data[weightSort[0]].weight - 1);
    cursor = index.openKeyCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, 0,
       "Correct count for bound keyRange arg to index.openKeyCursor");

    count = 0;
    keyRange = IDBKeyRange.bound(data[weightSort[1]].weight,
                                 data[weightSort[2]].weight);
    cursor = index.openKeyCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, 3,
       "Correct count for bound keyRange arg to index.openKeyCursor");

    count = 0;
    cursor = index.openCursor();
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, weightSort.length,
       "Correct count for unspecified arg to index.openCursor");

    count = 0;
    cursor = index.openCursor(null);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, weightSort.length,
       "Correct count for null arg to index.openCursor");

    count = 0;
    cursor = index.openCursor(undefined);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, weightSort.length,
       "Correct count for undefined arg to index.openCursor");

    count = 0;
    cursor = index.openCursor(data[0].weight);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, 1, "Correct count for single key arg to index.openCursor");

    count = 0;
    cursor = index.openCursor("foo");
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, 0,
       "Correct count for non-existent key arg to index.openCursor");

    count = 0;
    keyRange = IDBKeyRange.only("foo");
    cursor = index.openCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, 0,
       "Correct count for non-existent keyRange arg to index.openCursor");

    count = 0;
    keyRange = IDBKeyRange.only(data[0].weight);
    cursor = index.openCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, 1,
       "Correct count for only keyRange arg to index.openCursor");

    count = 0;
    keyRange = IDBKeyRange.lowerBound(data[weightSort[0]].weight);
    cursor = index.openCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, weightSort.length,
       "Correct count for lowerBound keyRange arg to index.openCursor");

    count = 0;
    keyRange = IDBKeyRange.lowerBound(data[weightSort[0]].weight, true);
    cursor = index.openCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, weightSort.length - 1,
       "Correct count for lowerBound keyRange arg to index.openCursor");

    count = 0;
    keyRange = IDBKeyRange.lowerBound("foo");
    cursor = index.openCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, 0,
       "Correct count for lowerBound keyRange arg to index.openCursor");

    count = 0;
    keyRange = IDBKeyRange.upperBound(data[weightSort[0]].weight);
    cursor = index.openCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, 1,
       "Correct count for upperBound keyRange arg to index.openCursor");

    count = 0;
    keyRange = IDBKeyRange.upperBound(data[weightSort[0]].weight, true);
    cursor = index.openCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, 0,
       "Correct count for upperBound keyRange arg to index.openCursor");

    count = 0;
    keyRange = IDBKeyRange.upperBound(data[weightSort[weightSort.length - 1]].weight);
    cursor = index.openCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, weightSort.length,
       "Correct count for upperBound keyRange arg to index.openCursor");

    count = 0;
    keyRange = IDBKeyRange.upperBound(data[weightSort[weightSort.length - 1]].weight,
                                      true);
    cursor = index.openCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, weightSort.length - 1,
       "Correct count for upperBound keyRange arg to index.openCursor");

    count = 0;
    keyRange = IDBKeyRange.upperBound("foo");
    cursor = index.openCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, weightSort.length,
       "Correct count for upperBound keyRange arg to index.openCursor");

    count = 0;
    keyRange = IDBKeyRange.upperBound(0);
    cursor = index.openCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, 0,
       "Correct count for upperBound keyRange arg to index.openCursor");

    count = 0;
    keyRange = IDBKeyRange.bound(data[weightSort[0]].weight,
                                 data[weightSort[weightSort.length - 1]].weight);
    cursor = index.openCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, weightSort.length,
       "Correct count for bound keyRange arg to index.openCursor");

    count = 0;
    keyRange = IDBKeyRange.bound(data[weightSort[0]].weight,
                                 data[weightSort[weightSort.length - 1]].weight,
                                 true);
    cursor = index.openCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, weightSort.length - 1,
       "Correct count for bound keyRange arg to index.openCursor");

    count = 0;
    keyRange = IDBKeyRange.bound(data[weightSort[0]].weight,
                                 data[weightSort[weightSort.length - 1]].weight,
                                 true, true);
    cursor = index.openCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, weightSort.length - 2,
       "Correct count for bound keyRange arg to index.openCursor");

    count = 0;
    keyRange = IDBKeyRange.bound(data[weightSort[0]].weight - 1,
                                 data[weightSort[weightSort.length - 1]].weight + 1);
    cursor = index.openCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, weightSort.length,
       "Correct count for bound keyRange arg to index.openCursor");

    count = 0;
    keyRange = IDBKeyRange.bound(data[weightSort[0]].weight - 2,
                                 data[weightSort[0]].weight - 1);
    cursor = index.openCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, 0,
       "Correct count for bound keyRange arg to index.openCursor");

    count = 0;
    keyRange = IDBKeyRange.bound(data[weightSort[1]].weight,
                                 data[weightSort[2]].weight);
    cursor = index.openCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, 3,
       "Correct count for bound keyRange arg to index.openCursor");

    try {
      index.get();
      ok(false, "Get with unspecified arg should have thrown");
    }
    catch(e) {
      ok(true, "Get with unspecified arg threw");
    }

    try {
      index.get(undefined);
      ok(false, "Get with undefined should have thrown");
    }
    catch(e) {
      ok(true, "Get with undefined arg threw");
    }

    expectException( function() {
      index.get(null);
      ok(false, "Get with null should have thrown");
    }, "DataError", 0);

    var request = index.get(data[0].weight);
    is(request.weight, data[0].weight, "Got correct result");

    keyRange = IDBKeyRange.only(data[0].weight);
    request = index.get(keyRange);
    is(request.weight, data[0].weight, "Got correct result");

    keyRange = IDBKeyRange.lowerBound(data[weightSort[0]].weight);
    request = index.get(keyRange);
    is(request.weight, data[weightSort[0]].weight,
       "Got correct result");

    keyRange = IDBKeyRange.lowerBound(data[weightSort[0]].weight - 1);
    request = index.get(keyRange);
    is(request.weight, data[weightSort[0]].weight,
       "Got correct result");

    keyRange = IDBKeyRange.lowerBound(data[weightSort[0]].weight + 1);
    request = index.get(keyRange);
    is(request.weight, data[weightSort[1]].weight,
       "Got correct result");

    keyRange = IDBKeyRange.lowerBound(data[weightSort[0]].weight, true);
    request = index.get(keyRange);
    is(request.weight, data[weightSort[1]].weight,
       "Got correct result");

    keyRange = IDBKeyRange.bound(data[weightSort[0]].weight,
                                 data[weightSort[1]].weight);
    request = index.get(keyRange);
    is(request.weight, data[weightSort[0]].weight,
       "Got correct result");

    keyRange = IDBKeyRange.bound(data[weightSort[0]].weight,
                                 data[weightSort[1]].weight, true);
    request = index.get(keyRange);
    is(request.weight, data[weightSort[1]].weight,
       "Got correct result");

    keyRange = IDBKeyRange.bound(data[weightSort[0]].weight,
                                 data[weightSort[1]].weight, true, true);
    request = index.get(keyRange);
    is(request, undefined, "Got correct result");

    keyRange = IDBKeyRange.upperBound(data[weightSort[5]].weight);
    request = index.get(keyRange);
    is(request.weight, data[weightSort[0]].weight,
       "Got correct result");

    keyRange = IDBKeyRange.upperBound(data[weightSort[0]].weight, true);
    request = index.get(keyRange);
    is(request, undefined, "Got correct result");

    try {
      index.getKey();
      ok(false, "Get with unspecified arg should have thrown");
    }
    catch(e) {
      ok(true, "Get with unspecified arg threw");
    }

    try {
      index.getKey(undefined);
      ok(false, "Get with undefined should have thrown");
    }
    catch(e) {
      ok(true, "Get with undefined arg threw");
    }

    expectException( function() {
      index.getKey(null);
      ok(false, "Get with null should have thrown");
    }, "DataError", 0);

    request = index.getKey(data[0].weight);
    is(request, data[0].ssn, "Got correct result");

    keyRange = IDBKeyRange.only(data[0].weight);
    request = index.getKey(keyRange);
    is(request, data[0].ssn, "Got correct result");

    keyRange = IDBKeyRange.lowerBound(data[weightSort[0]].weight);
    request = index.getKey(keyRange);
    is(request, data[weightSort[0]].ssn, "Got correct result");

    keyRange = IDBKeyRange.lowerBound(data[weightSort[0]].weight - 1);
    request = index.getKey(keyRange);
    is(request, data[weightSort[0]].ssn, "Got correct result");

    keyRange = IDBKeyRange.lowerBound(data[weightSort[0]].weight + 1);
    request = index.getKey(keyRange);
    is(request, data[weightSort[1]].ssn, "Got correct result");

    keyRange = IDBKeyRange.lowerBound(data[weightSort[0]].weight, true);
    request = index.getKey(keyRange);
    is(request, data[weightSort[1]].ssn, "Got correct result");

    keyRange = IDBKeyRange.bound(data[weightSort[0]].weight,
                                 data[weightSort[1]].weight);
    request = index.getKey(keyRange);
    is(request, data[weightSort[0]].ssn, "Got correct result");

    keyRange = IDBKeyRange.bound(data[weightSort[0]].weight,
                                 data[weightSort[1]].weight, true);
    request = index.getKey(keyRange);
    is(request, data[weightSort[1]].ssn, "Got correct result");

    keyRange = IDBKeyRange.bound(data[weightSort[0]].weight,
                                 data[weightSort[1]].weight, true, true);
    request = index.getKey(keyRange);
    is(request, undefined, "Got correct result");

    keyRange = IDBKeyRange.upperBound(data[weightSort[5]].weight);
    request = index.getKey(keyRange);
    is(request, data[weightSort[0]].ssn, "Got correct result");

    keyRange = IDBKeyRange.upperBound(data[weightSort[0]].weight, true);
    request = index.getKey(keyRange);
    is(request, undefined, "Got correct result");


    count = 0;
    cursor = index.openKeyCursor();
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, weightSort.length,
       "Correct count for no arg to index.openKeyCursor");

    count = 0;
    cursor = index.openKeyCursor(null);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, weightSort.length,
       "Correct count for null arg to index.openKeyCursor");

    count = 0;
    cursor = index.openKeyCursor(undefined);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, weightSort.length,
       "Correct count for undefined arg to index.openKeyCursor");

    count = 0;
    cursor = index.openKeyCursor(data[weightSort[0]].weight);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, 1, "Correct count for single key arg to index.openKeyCursor");

    count = 0;
    cursor = index.openKeyCursor("foo");
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, 0,
       "Correct count for non-existent single key arg to index.openKeyCursor");

    count = 0;
    keyRange = IDBKeyRange.only(data[weightSort[0]].weight);
    cursor = index.openKeyCursor(keyRange);
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, 1,
       "Correct count for only keyRange arg to index.openKeyCursor");

    request = objectStore.mozGetAll(data[1].ssn);

    is(request instanceof Array, true, "Got an array");
    is(request.length, 1, "Got correct length");
    is(request[0].ssn, data[1].ssn, "Got correct result");

    request = objectStore.mozGetAll(null);

    is(request instanceof Array, true, "Got an array");
    is(request.length, data.length, "Got correct length");
    for (var i in request) {
      is(request[i].ssn, data[i].ssn, "Got correct value");
    }

    request = objectStore.mozGetAll(undefined);

    is(request instanceof Array, true, "Got an array");
    is(request.length, data.length, "Got correct length");
    for (var i in request) {
      is(request[i].ssn, data[i].ssn, "Got correct value");
    }

    request = objectStore.mozGetAll();

    is(request instanceof Array, true, "Got an array");
    is(request.length, data.length, "Got correct length");
    for (var i in request) {
      is(request[i].ssn, data[i].ssn, "Got correct value");
    }

    keyRange = IDBKeyRange.lowerBound(0);
    request = objectStore.mozGetAll(keyRange);

    is(request instanceof Array, true, "Got an array");
    is(request.length, data.length, "Got correct length");
    for (var i in request) {
      is(request[i].ssn, data[i].ssn, "Got correct value");
    }

    request = index.mozGetAll();

    is(request instanceof Array, true, "Got an array");
    is(request.length, weightSort.length, "Got correct length");
    for (var i in request) {
      is(request[i].ssn, data[weightSort[i]].ssn,
         "Got correct value");
    }

    request = index.mozGetAll(undefined);

    is(request instanceof Array, true, "Got an array");
    is(request.length, weightSort.length, "Got correct length");
    for (var i in request) {
      is(request[i].ssn, data[weightSort[i]].ssn,
         "Got correct value");
    }

    request = index.mozGetAll(null);

    is(request instanceof Array, true, "Got an array");
    is(request.length, weightSort.length, "Got correct length");
    for (var i in request) {
      is(request[i].ssn, data[weightSort[i]].ssn,
         "Got correct value");
    }

    request = index.mozGetAll(data[weightSort[0]].weight);

    is(request instanceof Array, true, "Got an array");
    is(request.length, 1, "Got correct length");
    is(request[0].ssn, data[weightSort[0]].ssn, "Got correct result");

    keyRange = IDBKeyRange.lowerBound(0);
    request = index.mozGetAll(keyRange);

    is(request instanceof Array, true, "Got an array");
    is(request.length, weightSort.length, "Got correct length");
    for (var i in request) {
      is(request[i].ssn, data[weightSort[i]].ssn,
         "Got correct value");
    }

    request = index.mozGetAllKeys();

    is(request instanceof Array, true, "Got an array");
    is(request.length, weightSort.length, "Got correct length");
    for (var i in request) {
      is(request[i], data[weightSort[i]].ssn,
         "Got correct value");
    }

    request = index.mozGetAllKeys(undefined);

    is(request instanceof Array, true, "Got an array");
    is(request.length, weightSort.length, "Got correct length");
    for (var i in request) {
      is(request[i], data[weightSort[i]].ssn,
         "Got correct value");
    }

    request = index.mozGetAllKeys(null);

    is(request instanceof Array, true, "Got an array");
    is(request.length, weightSort.length, "Got correct length");
    for (var i in request) {
      is(request[i], data[weightSort[i]].ssn,
         "Got correct value");
    }

    request = index.mozGetAllKeys(data[weightSort[0]].weight);

    is(request instanceof Array, true, "Got an array");
    is(request.length, 1, "Got correct length");
    is(request[0], data[weightSort[0]].ssn, "Got correct result");

    keyRange = IDBKeyRange.lowerBound(0);
    request = index.mozGetAllKeys(keyRange);

    is(request instanceof Array, true, "Got an array");
    is(request.length, weightSort.length, "Got correct length");
    for (var i in request) {
      is(request[i], data[weightSort[i]].ssn,
         "Got correct value");
    }
  });

  info("Test successfully completed");
  postMessage(undefined);
};
