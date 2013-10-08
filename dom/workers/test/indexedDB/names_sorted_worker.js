/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;
  const objectStoreInfo = [
    { name: "foo", options: { keyPath: "id" }, location: 1 },
    { name: "bar", options: { keyPath: "id" }, location: 0 }
  ];
  const indexInfo = [
    { name: "foo", keyPath: "value", location: 1 },
    { name: "bar", keyPath: "value", location: 0 }
  ];

  var item;
  var i;
  var j;

  var db = indexedDBSync.open(name, 1, function(trans, oldVersion) {


    for (i = 0; i < objectStoreInfo.length; i++) {
      item = objectStoreInfo[i];
      var objectStore = item.hasOwnProperty("options") ?
                        trans.db.createObjectStore(item.name, item.options) :
                        trans.db.createObjectStore(item.name);

      // Test index creation, and that it ends up in indexNames.
      var objectStoreName = item.name;
      for (j = 0; j < indexInfo.length; j++) {
        item = indexInfo[j];
        is(objectStore.indexNames.length, j, "Correct indexNames length");
        var index = item.hasOwnProperty("options") ?
                    objectStore.createIndex(item.name, item.keyPath,
                                            item.options) :
                    objectStore.createIndex(item.name, item.keyPath);
      }
      is(objectStore.indexNames.length, 2, "Correct indexNames length");
    }
  });

  var objectStoreNames = [];

  for (i = 0; i < objectStoreInfo.length; i++) {
    item = objectStoreInfo[i];
    objectStoreNames.push(item.name);

    is(db.objectStoreNames.item(item.location), item.name,
       "Got objectStore name in the right location");

    db.transaction(item.name, function(trans) {
      var objectStore = trans.objectStore(item.name);
      for (j = 0; j < indexInfo.length; j++) {
        item = indexInfo[j];
        is(objectStore.indexNames.item(item.location), item.name,
           "Got index name in the right location");
      }
    });
  }

  db.transaction(objectStoreNames, function(trans) {

    for (i = 0; i < objectStoreInfo.length; i++) {
      item = objectStoreInfo[i];

      is(trans.objectStoreNames.item(item.location), item.name,
         "Got objectStore name in the right location");
    }
  });

  db.close();

  var db2 = indexedDBSync.open(name, 1, function(trans, oldVersion) {});

  objectStoreNames = [];

  for (i = 0; i < objectStoreInfo.length; i++) {
    item = objectStoreInfo[i];
    objectStoreNames.push(item.name);

    is(db2.objectStoreNames.item(item.location), item.name,
       "Got objectStore name in the right location");

    db2.transaction(item.name, function(trans) {
      item = objectStoreInfo[i];
      var objectStore = trans.objectStore(item.name);

      for (var j = 0; j < indexInfo.length; j++) {
        item = indexInfo[j];
        is(objectStore.indexNames.item(item.location), item.name,
           "Got index name in the right location");
      }
    });
  }

  db2.transaction(objectStoreNames, function(trans) {
    for (var i = 0; i < objectStoreInfo.length; i++) {
      item = objectStoreInfo[i];

      is(trans.objectStoreNames.item(item.location), item.name,
         "Got objectStore name in the right location");
    }
  });

  db2.close();

  info("Test successfully completed");
  postMessage(undefined);
};
