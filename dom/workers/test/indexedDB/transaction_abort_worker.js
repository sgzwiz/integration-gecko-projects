/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;

  var upgradeneededcalled = false;

  var transaction;
  var objectStore;
  var index;

  var db = indexedDBSync.open(name, 1, function(trans, oldVersion) {
    upgradeneededcalled = true;

    transaction = trans;
    objectStore = trans.db.createObjectStore("foo", { autoIncrement: true });
    index = objectStore.createIndex("fooindex", "indexKey", { unique: true });

    is(transaction.mode, "versionchange", "Correct mode");
    is(transaction.objectStoreNames.length, 1, "Correct names length");
    is(transaction.objectStoreNames.item(0), "foo", "Correct name");
    ok(transaction.objectStore("foo") === objectStore, "Can get stores");

    is(objectStore.name, "foo", "Correct name");
    is(objectStore.keyPath, null, "Correct keyPath");

    is(objectStore.indexNames.length, 1, "Correct indexNames length");
    is(objectStore.indexNames.item(0), "fooindex", "Correct indexNames name");
    ok(objectStore.index("fooindex") == index, "Can get index");
  });

  ok(upgradeneededcalled, "Expected upgradeneeded callback")
  ok(transaction.db == db, "Correct database");
  is(transaction.mode, "versionchange", "Correct mode");
  is(transaction.objectStoreNames.length, 1, "Correct names length");
  is(transaction.objectStoreNames.item(0), "foo", "Correct name");

  try {
    is(transaction.objectStore("foo").name, "foo", "Can't get stores");
    ok(false, "Should have thrown");
  }
  catch (e) {
    ok(true, "Out of scope transaction can't make stores");
  }

  is(objectStore.name, "foo", "Correct name");
  is(objectStore.keyPath, null, "Correct keyPath");

  is(objectStore.indexNames.length, 1, "Correct indexNames length");
  is(objectStore.indexNames[0], "fooindex", "Correct indexNames name");

  try {
    objectStore.add({});
    ok(false, "Should have thrown");
  }
  catch (e) {
    ok(true, "Add threw");
  }

  try {
    objectStore.put({}, 1);
    ok(false, "Should have thrown");
  }
  catch (e) {
    ok(true, "Put threw");
  }

  try {
    objectStore.put({}, 1);
    ok(false, "Should have thrown");
  }
  catch (e) {
    ok(true, "Put threw");
  }

  try {
    objectStore.delete(1);
    ok(false, "Should have thrown");
  }
  catch (e) {
    ok(true, "Remove threw");
  }

  try {
    objectStore.get(1);
    ok(false, "Should have thrown");
  }
  catch (e) {
    ok(true, "Get threw");
  }

  try {
    objectStore.getAll(null);
    ok(false, "Should have thrown");
  }
  catch (e) {
    ok(true, "GetAll threw");
  }

  try {
    objectStore.openCursor();
    ok(false, "Should have thrown");
  }
  catch (e) {
    ok(true, "OpenCursor threw");
  }

  try {
    objectStore.createIndex("bar", "id");
    ok(false, "Should have thrown");
  }
  catch (e) {
    ok(true, "CreateIndex threw");
  }

  try {
    objectStore.index("bar");
    ok(false, "Should have thrown");
  }
  catch (e) {
    ok(true, "Index threw");
  }

  try {
    objectStore.deleteIndex("bar");
    ok(false, "Should have thrown");
  }
  catch (e) {
    ok(true, "RemoveIndex threw");
  }

  var key;
  db.transaction("foo", function(trans) {
    var objectStore = trans.objectStore("foo");
    key = objectStore.add({});
    trans.abort();
  }, "readwrite");

  db.transaction("foo", function(trans) {
    var objectStore = trans.objectStore("foo");
    var result = objectStore.get(key);
    is(result, undefined, "Object was removed");
  });

  var keys = [];
  var abortExceptionCount = 0;
  db.transaction("foo", function(trans) {
    var objectStore = trans.objectStore("foo");
    for (var i = 0; i < 10; i++) {
      try {
        key = objectStore.add({});
        keys.push(key);
        if (keys.length == 5) {
          trans.abort();
        }
      }
      catch (ex)
      {
        abortExceptionCount++;
      }
    }
  }, "readwrite");

  is(keys.length, 5, "Added 5 items in this transaction");
  is(abortExceptionCount, 5, "Got 5 abort exceptions")

  for (var i in keys) {
    db.transaction("foo", function(trans) {
      var result = trans.objectStore("foo").get(keys[i]);
      is(result, undefined, "Object was removed by abort");
    });
  }

  db.transaction("foo", function(trans) {
    trans.abort();
    try {
      trans.abort();
      ok(false, "Second abort should have thrown an error");
    }
    catch (ex) {
      ok(true, "Second abort threw an error");
    }
  });

  ok(true, "Test successfully completed");
  postMessage(undefined);
};
