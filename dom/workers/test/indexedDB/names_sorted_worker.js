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

  var info;
  var i;
  var j;

  var db = indexedDBSync.open(name, 1, function(trans, oldVersion) {


    for (i = 0; i < objectStoreInfo.length; i++) {
      info = objectStoreInfo[i];
      var objectStore = info.hasOwnProperty("options") ?
                        trans.db.createObjectStore(info.name, info.options) :
                        trans.db.createObjectStore(info.name);

      // Test index creation, and that it ends up in indexNames.
      var objectStoreName = info.name;
      for (j = 0; j < indexInfo.length; j++) {
        info = indexInfo[j];
        is(objectStore.indexNames.length, j, "Correct indexNames length");
        var index = info.hasOwnProperty("options") ?
                    objectStore.createIndex(info.name, info.keyPath,
                                            info.options) :
                    objectStore.createIndex(info.name, info.keyPath);
      }
      is(objectStore.indexNames.length, 2, "Correct indexNames length");
    }
  });

  var objectStoreNames = [];

  for (i = 0; i < objectStoreInfo.length; i++) {
    info = objectStoreInfo[i];
    objectStoreNames.push(info.name);

    is(db.objectStoreNames.item(info.location), info.name,
       "Got objectStore name in the right location");

    db.transaction(info.name, function(trans) {
      var objectStore = trans.objectStore(info.name);
      for (j = 0; j < indexInfo.length; j++) {
        info = indexInfo[j];
        is(objectStore.indexNames.item(info.location), info.name,
           "Got index name in the right location");
      }
    });
  }

  db.transaction(objectStoreNames, function(trans) {

    for (i = 0; i < objectStoreInfo.length; i++) {
      info = objectStoreInfo[i];

      is(trans.objectStoreNames.item(info.location), info.name,
         "Got objectStore name in the right location");
    }
  });

  db.close();

  var db2 = indexedDBSync.open(name, 1, function(trans, oldVersion) {});

  objectStoreNames = [];

  for (i = 0; i < objectStoreInfo.length; i++) {
    info = objectStoreInfo[i];
    objectStoreNames.push(info.name);

    is(db2.objectStoreNames.item(info.location), info.name,
       "Got objectStore name in the right location");

    db2.transaction(info.name, function(trans) {
      info = objectStoreInfo[i];
      var objectStore = trans.objectStore(info.name);

      for (var j = 0; j < indexInfo.length; j++) {
        info = indexInfo[j];
        is(objectStore.indexNames.item(info.location), info.name,
           "Got index name in the right location");
      }
    });
  }

  db2.transaction(objectStoreNames, function(trans) {
    for (var i = 0; i < objectStoreInfo.length; i++) {
      info = objectStoreInfo[i];

      is(trans.objectStoreNames.item(info.location), info.name,
         "Got objectStore name in the right location");
    }
  });

  db2.close();

  ok(true, "Test successfully completed");
  postMessage(undefined);
};
