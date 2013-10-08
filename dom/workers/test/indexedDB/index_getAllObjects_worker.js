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
    { name: "height", keyPath: "height", options: { unique: false } },
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
    var objectStore = trans.db.createObjectStore(objectStoreName, { });

   // First, add all our data to the object store.
    var addedData = 0;
    for (var i in objectStoreData) {
      objectStore.add(objectStoreData[i].value, objectStoreData[i].key);
      addedData++;
    }
    is(addedData, objectStoreData.length, "Correct length.");

    // Now create the indexes.
    for (var i in indexData) {
      objectStore.createIndex(indexData[i].name, indexData[i].keyPath,
                              indexData[i].options);
    }
    is(objectStore.indexNames.length, indexData.length, "Good index count");
  });

  db.transaction(objectStoreName, function(trans) {
    var objectStore = trans.objectStore(objectStoreName);
    var index = objectStore.index("height");

    var result = index.mozGetAll(65);
    is(result instanceof Array, true, "Got an array object");
    is(result.length, 2, "Correct length");

    for (var i in result) {
      var object = result[i];
      var testObj = objectStoreDataHeightSort[parseInt(i) + 3].value;

      is(object.name, testObj.name, "Correct name");
      is(object.height, testObj.height, "Correct height");

      if (testObj.hasOwnProperty("weight")) {
        is(object.weight, testObj.weight, "Correct weight");
      }
    }

    result = index.mozGetAll(65, 0);
    is(result instanceof Array, true, "Got an array object");
    is(result.length, 2, "Correct length");

    for (var i in result) {
      var object = result[i];
      var testObj = objectStoreDataHeightSort[parseInt(i) + 3].value;

      is(object.name, testObj.name, "Correct name");
      is(object.height, testObj.height, "Correct height");

      if (testObj.hasOwnProperty("weight")) {
        is(object.weight, testObj.weight, "Correct weight");
      }
    }

    result = index.mozGetAll(65, null);
    is(result instanceof Array, true, "Got an array object");
    is(result.length, 2, "Correct length");

    for (var i in result) {
      var object = result[i];
      var testObj = objectStoreDataHeightSort[parseInt(i) + 3].value;

      is(object.name, testObj.name, "Correct name");
      is(object.height, testObj.height, "Correct height");

      if (testObj.hasOwnProperty("weight")) {
        is(object.weight, testObj.weight, "Correct weight");
      }
    }

    result = index.mozGetAll(65, undefined);
    is(result instanceof Array, true, "Got an array object");
    is(result.length, 2, "Correct length");

    for (var i in result) {
      var object = result[i];
      var testObj = objectStoreDataHeightSort[parseInt(i) + 3].value;

      is(object.name, testObj.name, "Correct name");
      is(object.height, testObj.height, "Correct height");

      if (testObj.hasOwnProperty("weight")) {
        is(object.weight, testObj.weight, "Correct weight");
      }
    }

    result = index.mozGetAll();
    is(result instanceof Array, true, "Got an array object");
    is(result.length, objectStoreDataHeightSort.length,
       "Correct length");

    for (var i in result) {
      var object = result[i];
      var testObj = objectStoreDataHeightSort[i].value;

      is(object.name, testObj.name, "Correct name");
      is(object.height, testObj.height, "Correct height");

      if (testObj.hasOwnProperty("weight")) {
        is(object.weight, testObj.weight, "Correct weight");
      }
    }

    result = index.mozGetAll(null, 4);
    is(result instanceof Array, true, "Got an array object");
    is(result.length, 4, "Correct length");

    for (var i in result) {
      var object = result[i];
      var testObj = objectStoreDataHeightSort[i].value;

      is(object.name, testObj.name, "Correct name");
      is(object.height, testObj.height, "Correct height");

      if (testObj.hasOwnProperty("weight")) {
        is(object.weight, testObj.weight, "Correct weight");
      }
    }

    result = index.mozGetAll(65, 1);
    is(result instanceof Array, true, "Got an array object");
    is(result.length, 1, "Correct length");

    for (var i in result) {
      var object = result[i];
      var testObj = objectStoreDataHeightSort[parseInt(i) + 3].value;

      is(object.name, testObj.name, "Correct name");
      is(object.height, testObj.height, "Correct height");

      if (testObj.hasOwnProperty("weight")) {
        is(object.weight, testObj.weight, "Correct weight");
      }
    }
  });

  info("Test successfully completed");
  postMessage(undefined);
};
