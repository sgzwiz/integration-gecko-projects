/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;

  var upgradeneededcalled = false;

  var db;
  var objectStore;
  try {
    indexedDBSync.open(name, 1, function(trans, oldVersion) {
      upgradeneededcalled = true;

      db = trans.db;

      objectStore = db.createObjectStore("foo");
      var index = objectStore.createIndex("bar", "baz");

      is(db.version, 1, "Correct version");
      is(db.objectStoreNames.length, 1, "Correct objectStoreNames length");
      is(objectStore.indexNames.length, 1, "Correct indexNames length");

      is(trans.mode, "versionchange", "Correct transaction mode");
      trans.abort();

      is(db.version, 0, "Correct version");
      is(db.objectStoreNames.length, 0, "Correct objectStoreNames length");
      is(objectStore.indexNames.length, 0, "Correct indexNames length");

    });
    ok(false, "Expect an exception");
  }
  catch (ex) {
    ok(true, "Expect an exception");
  }

  ok(upgradeneededcalled, "Expected upgradeneeded callback")
  is(db.version, 0, "Correct version");
  is(db.objectStoreNames.length, 0, "Correct objectStoreNames length");
  is(objectStore.indexNames.length, 0, "Correct indexNames length");

  // Test that the db is actually closed.
  try {
    db.transaction("", function(trans) {});
    ok(false, "Expect an exception");
  } catch (e) {
    ok(true, "Expect an exception");
    is(e.name, "InvalidStateError", "Expect an InvalidStateError");
  }

  upgradeneededcalled = false;

  var db2;
  var objectStore2;
  var result = indexedDBSync.open(name, 1, function(trans, oldVersion) {
    upgradeneededcalled = true;

    db2 = trans.db;
    ok(db != db2, "Should give a different db instance");
    is(db2.version, 1, "Correct version");
    is(db2.objectStoreNames.length, 0, "Correct objectStoreNames length");

    objectStore2 = db2.createObjectStore("foo");
    var index2 = objectStore2.createIndex("bar", "baz");
  });

  ok(upgradeneededcalled, "Expected upgradeneeded callback")
  ok(result == db2, "Correct result");
  is(db2.version, 1, "Correct version");
  is(db2.objectStoreNames.length, 1, "Correct objectStoreNames length");
  is(objectStore2.indexNames.length, 1, "Correct indexNames length");
  is(db.version, 0, "Correct version still");
  is(db.objectStoreNames.length, 0, "Correct objectStoreNames length still");
  is(objectStore.indexNames.length, 0, "Correct indexNames length still");

  info("Test successfully completed");
  postMessage(undefined);
};
