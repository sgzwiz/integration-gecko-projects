/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;
  const version = 1;
  const storeName = "foo";
  const keyCount = 200;

  var db = indexedDBSync.open(name, version, function(trans, oldVersion) {
    info("Creating database");
    var objectStore = trans.db.createObjectStore(storeName);
    for (var i = 0; i < keyCount; i++) {
      objectStore.add(true, i);
    }
  });

  db.transaction(storeName, function(trans) {
    var objectStore = trans.objectStore(storeName);
    info("Getting all keys");
    var result = objectStore.getAllKeys();
    is(result instanceof Array, true, "Got an array result");
    is(result.length, keyCount, "Got correct array length");

    var match = true;
    for (var i = 0; i < keyCount; i++) {
      if (result[i] != i) {
        match = false;
        break;
      }
    }
    ok(match, "Got correct keys");

    info("Getting all keys with key range");
    var keyRange = IDBKeyRange.bound(10, 20, false, true);
    result = objectStore.getAllKeys(keyRange);
    is(result instanceof Array, true, "Got an array result");
    is(result.length, 10, "Got correct array length");

    var match = true;
    for (var i = 10; i < 20; i++) {
      if (result[i-10] != i) {
        match = false;
        break;
      }
    }
    ok(match, "Got correct keys");

    info("Getting all keys with unmatched key range");
    var keyRange = IDBKeyRange.bound(10000, 20000);
    result = objectStore.getAllKeys(keyRange);
    is(result instanceof Array, true, "Got an array result");
    is(result.length, 0, "Got correct array length");

    info("Getting all keys with limit");
    result = objectStore.getAllKeys(null, 5);
    is(result instanceof Array, true, "Got an array result");
    is(result.length, 5, "Got correct array length");

    var match = true;
    for (var i = 0; i < 5; i++) {
      if (result[i] != i) {
        match = false;
        break;
      }
    }
    ok(match, "Got correct keys");

    info("Getting all keys with key range and limit");
    var keyRange = IDBKeyRange.bound(10, 20, false, true);
    result = objectStore.getAllKeys(keyRange, 5);
    is(result instanceof Array, true, "Got an array result");
    is(result.length, 5, "Got correct array length");

    var match = true;
    for (var i = 10; i < 15; i++) {
      if (result[i-10] != i) {
        match = false;
        break;
      }
    }
    ok(match, "Got correct keys");

    info("Getting all keys with unmatched key range and limit");
    var keyRange = IDBKeyRange.bound(10000, 20000);
    result = objectStore.getAllKeys(keyRange, 5);
    is(result instanceof Array, true, "Got an array result");
    is(result.length, 0, "Got correct array length");

  });

  info("Test successfully completed");
  postMessage(undefined);
};
