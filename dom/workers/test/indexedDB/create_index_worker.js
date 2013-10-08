/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;
  const objectStoreInfo = [
    { name: "a", options: { keyPath: "id", autoIncrement: true } },
    { name: "b", options: { keyPath: "id", autoIncrement: false } }
  ];
  const indexInfo = [
    { name: "1", keyPath: "unique_value", options: { unique: true } },
    { name: "2", keyPath: "value", options: { unique: false } },
    { name: "3", keyPath: "value", options: { unique: false } },
    { name: "", keyPath: "value", options: { unique: false } },
    { name: null, keyPath: "value", options: { unique: false } },
    { name: undefined, keyPath: "value", options: { unique: false } }
  ];

  var db = indexedDBSync.open(name, 1, function(trans, oldVersion) {
    for (var i = 0; i < objectStoreInfo.length; i++) {
      var osInfo = objectStoreInfo[i];
      var objectStore = osInfo.hasOwnProperty("options") ?
                        trans.db.createObjectStore(osInfo.name, osInfo.options) :
                        trans.db.createObjectStore(osInfo.name);

      try {
        objectStore.createIndex("Hola");
        ok(false, "createIndex with no keyPath should throw");
      }
      catch(e) {
        ok(true, "createIndex with no keyPath should throw");
      }

      expectException( function() {
        objectStore.createIndex("Hola", ["foo"], { multiEntry: true });
        ok(false, "createIndex with array keyPath and multiEntry should throw");
      }, "InvalidAccessError", DOMException.INVALID_ACCESS_ERR);

      try {
        objectStore.createIndex("foo", "bar", 10);
        ok(false, "createIndex with bad options should throw");
      }
      catch(e) {
        ok(true, "createIndex with bad options");
      }

      try {
        objectStore.createIndex("foo", "bar", { foo: "" });
        ok(true, "createIndex with unknown options should not throw");
        is(objectStore.indexNames.length, 1, "Correct indexNames list");
        objectStore.deleteIndex("foo");
        is(objectStore.indexNames.length, 0, "Correct indexNames list");
      }
      catch(e) {
        ok(false, "createIndex with unknown options");
      }

      // Test index creation, and that it ends up in indexNames.
      var objectStoreName = osInfo.name;
      for (var j = 0; j < indexInfo.length; j++) {
        osInfo = indexInfo[j];
        var count = objectStore.indexNames.length;
        var index = osInfo.hasOwnProperty("options") ?
                    objectStore.createIndex(osInfo.name, osInfo.keyPath,
                                            osInfo.options) :
                    objectStore.createIndex(osInfo.name, osInfo.keyPath);

        var name = osInfo.name;
        if (name === null) {
          name = "null";
        }
        else if (name === undefined) {
          name = "undefined";
        }

        is(index.name, name, "Correct name");
        is(index.keyPath, osInfo.keyPath, "Correct keyPath");
        is(index.unique, osInfo.options.unique, "Correct uniqueness");

        is(objectStore.indexNames.length, count + 1, "List indexNames grew in size");

        var found = false;
        for (var k = 0; k < objectStore.indexNames.length; k++) {
          if (objectStore.indexNames.item(k) == name) {
            found = true;
            break;
          }
        }
        ok(found, "Name is on objectStore.indexNames");

        is(trans.mode, "versionchange", "Transaction has the correct mode");
        is(trans.objectStoreNames.length, i + 1,
           "Transaction only has one object store");
        ok(trans.objectStoreNames.contains(objectStoreName),
           "Transaction has the correct object store");
      }
    }
  });

  info("Test successfully completed");
  postMessage(undefined);
};
