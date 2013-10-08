/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;
  const values = [ "a", "1", 1, "foo", 300, true, false, 4.5, null ];

  var db = indexedDBSync.open(name, 1, function(trans, oldVersion) {
    var objectStore = trans.db.createObjectStore("foo", { autoIncrement: true });
    var result = objectStore.mozGetAll();
    is(result instanceof Array, true, "Got an array object");
    is(result.length, 0, "No elements");

    var addedCount = 0;

    for (var i in values) {
      var key = objectStore.add(values[i]);
      addedCount++;
    }

    is(addedCount, values.length, "Same length");
  });

  db.transaction("foo", function(trans) {
    var objectStore = trans.objectStore("foo");
    var result = objectStore.mozGetAll();
    is(result instanceof Array, true, "Got an array object");
    is(result.length, values.length, "Same length");

    for (var i in result) {
      is(result[i], values[i], "Same value");
    }

    result = objectStore.mozGetAll(null, 5);
    is(result instanceof Array, true, "Got an array object");
    is(result.length, 5, "Correct length");

    for (var i in result) {
      is(result[i], values[i], "Same value");
    }

    var keyRange = IDBKeyRange.bound(1, 9);
    result = objectStore.mozGetAll(keyRange);

    is(result instanceof Array, true, "Got an array object");
    is(result.length, values.length, "Correct length");

    for (var i in result) {
      is(result[i], values[i], "Same value");
    }

    result = objectStore.mozGetAll(keyRange, 0);

    is(result instanceof Array, true, "Got an array object");
    is(result.length, values.length, "Correct length");

    for (var i in result) {
      is(result[i], values[i], "Same value");
    }

    result = objectStore.mozGetAll(keyRange, null);

    is(result instanceof Array, true, "Got an array object");
    is(result.length, values.length, "Correct length");

    for (var i in result) {
      is(result[i], values[i], "Same value");
    }

    result = objectStore.mozGetAll(keyRange, undefined);

    is(result instanceof Array, true, "Got an array object");
    is(result.length, values.length, "Correct length");

    for (var i in result) {
      is(result[i], values[i], "Same value");
    }

    keyRange = IDBKeyRange.bound(4, 7);

    result = objectStore.mozGetAll(keyRange);

    is(result instanceof Array, true, "Got an array object");
    is(result.length, 4, "Correct length");

    for (var i in result) {
      is(result[i], values[parseInt(i) + 3], "Same value");
    }

    // Get should take a key range also but it doesn't return an array.
    result = objectStore.get(keyRange);

    is(result instanceof Array, false, "Not an array object");
    is(result, values[3], "Correct value");

    result = objectStore.mozGetAll(keyRange, 2);

    is(result instanceof Array, true, "Got an array object");
    is(result.length, 2, "Correct length");

    for (var i in result) {
      is(result[i], values[parseInt(i) + 3], "Same value");
    }

    keyRange = IDBKeyRange.bound(4, 7);

    result = objectStore.mozGetAll(keyRange, 50);

    is(result instanceof Array, true, "Got an array object");
    is(result.length, 4, "Correct length");

    for (var i in result) {
      is(result[i], values[parseInt(i) + 3], "Same value");
    }

    keyRange = IDBKeyRange.bound(4, 7);

    result = objectStore.mozGetAll(keyRange, 0);

    is(result instanceof Array, true, "Got an array object");
    is(result.length, 4, "Correct length");

    keyRange = IDBKeyRange.bound(4, 7, true, true);

    result = objectStore.mozGetAll(keyRange);

    is(result instanceof Array, true, "Got an array object");
    is(result.length, 2, "Correct length");

    for (var i in result) {
      is(result[i], values[parseInt(i) + 4], "Same value");
    }
  });

  info("Test successfully completed");
  postMessage(undefined);
};
