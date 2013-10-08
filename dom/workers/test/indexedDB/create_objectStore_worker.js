/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;
  const objectStoreInfo = [
    { name: "1", options: { keyPath: null } },
    { name: "2", options: { keyPath: null, autoIncrement: true } },
    { name: "3", options: { keyPath: null, autoIncrement: false } },
    { name: "4", options: { keyPath: null } },
    { name: "5", options: { keyPath: "foo" } },
    { name: "6" },
    { name: "7", options: null },
    { name: "8", options: { autoIncrement: true } },
    { name: "9", options: { autoIncrement: false } },
    { name: "10", options: { keyPath: "foo", autoIncrement: false } },
    { name: "11", options: { keyPath: "foo", autoIncrement: true } },
    { name: "" },
    { name: null },
    { name: undefined }
  ];

  var db = indexedDBSync.open(name, 1, function(trans, oldVersion) {
    var count = trans.db.objectStoreNames.length;
    is(count, 0, "Correct objectStoreNames length");

    try {
      trans.db.createObjectStore("foo", "bar");
      ok(false, "CreateObjectStore with bad options should have thrown");
    }
    catch(e) {
      ok(true, "CreateObjectStore with bad options threw");
    }

    try {
       trans.db.createObjectStore("foo", { foo: "" });
       ok(true, "createObjectStore with unknown options should not throw");
       is(trans.db.objectStoreNames.length, 1, "Correct objectStoreNames list");
       trans.db.deleteObjectStore("foo");
       is(trans.db.objectStoreNames.length, 0, "Correct objectStoreNames list");
    }
    catch(e) {
      ok(false, "createObjectStore with unknown options");
    }

    for (var index in objectStoreInfo) {
      index = parseInt(index);
      const osInfo = objectStoreInfo[index];

      var objectStore = osInfo.hasOwnProperty("options") ?
                        trans.db.createObjectStore(osInfo.name, osInfo.options) :
                        trans.db.createObjectStore(osInfo.name);

      is(trans.db.objectStoreNames.length, index + 1,
         "Updated objectStoreNames list");

      var name = osInfo.name;
      if (name === null) {
        name = "null";
      }
      else if (name === undefined) {
        name = "undefined";
      }

      var found = false;
      for (var i = 0; i <= index; i++) {
        if (trans.db.objectStoreNames.item(i) == name) {
          found = true;
          break;
        }
      }
      is(found, true, "objectStoreNames contains name");

      is(objectStore.name, name, "Correct store name");
      is(objectStore.keyPath, osInfo.options && osInfo.options.keyPath ?
                              osInfo.options.keyPath : null,
         "Bad keyPath");
      is(objectStore.indexNames.length, 0, "Correct indexNames list");

      is(trans.mode, "versionchange",
         "Transaction has the correct mode");
      is(trans.objectStoreNames.length, index + 1,
         "Transaction has correct objectStoreNames list");
      found = false;
      for (var j = 0; j < trans.objectStoreNames.length; j++) {
        if (trans.objectStoreNames.item(j) == name) {
          found = true;
          break;
        }
      }
      is(found, true, "Transaction has correct objectStoreNames list");
    }

    // Can't handle autoincrement and empty keypath
    expectException( function() {
      trans.db.createObjectStore("storefail", { keyPath: "", autoIncrement: true });
      ok(false, "createObjectStore with empty keyPath and autoIncrement should throw");
    }, "InvalidAccessError", DOMException.INVALID_ACCESS_ERR);

    // Can't handle autoincrement and array keypath
    expectException( function() {
      trans.db.createObjectStore("storefail", { keyPath: ["a"], autoIncrement: true });
      ok(false, "createObjectStore with array keyPath and autoIncrement should throw");
    }, "InvalidAccessError", DOMException.INVALID_ACCESS_ERR);

  });

  info("Test successfully completed");
  postMessage(undefined);
};
