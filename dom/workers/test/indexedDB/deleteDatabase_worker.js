/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;

  ok(indexedDBSync.deleteDatabase, "deleteDatabase function should exist!");

  var upgradeneededcalled = false;

  var db = indexedDBSync.open(name, 10, function(trans, oldVersion) {
    upgradeneededcalled = true;
    trans.db.createObjectStore("stuff");
  });

  ok(upgradeneededcalled, "Expected upgradeneeded callback");
  ok(db instanceof IDBDatabaseSync, "Result should be a database");
  is(db.objectStoreNames.length, 1, "Expect an objectStore here");

  var db2 = indexedDBSync.open(name, 10);

  ok(db2 instanceof IDBDatabaseSync, "Result should be a database");
  is(db2.objectStoreNames.length, 1, "Expect an objectStore here");

  var onversionchangecalled = false;
  var onversionchangecalled2 = false;

  function closeDB(event) {
    onversionchangecalled = true;
    ok(event instanceof IDBVersionChangeEvent, "expect a versionchange event");
    is(event.oldVersion, 10, "oldVersion should be 10");
    ok(event.newVersion === null, "newVersion should be null");
    ok(!(event.newVersion === undefined), "newVersion should be null");
    ok(!(event.newVersion === 0), "newVersion should be null");
    db.close();
    db.onversionchange = unexpectedEventHandler;
  };
  function closeDB2(event) {
    onversionchangecalled2 = true;
    ok(event instanceof IDBVersionChangeEvent, "expect a versionchange event");
    is(event.oldVersion, 10, "oldVersion should be 10");
    ok(event.newVersion === null, "newVersion should be null");
    ok(!(event.newVersion === undefined), "newVersion should be null");
    ok(!(event.newVersion === 0), "newVersion should be null");
    db2.close();
    db2.onversionchange = unexpectedEventHandler;
  };

  db.onversionchange = closeDB;
  db2.onversionchange = closeDB2;

  indexedDBSync.deleteDatabase(name);

  ok(onversionchangecalled, "Expected versionchange event");
  ok(onversionchangecalled2, "Expected versionchange event");

  db = indexedDBSync.open(name, 1);

  is(db.version, 1, "DB has proper version");
  is(db.objectStoreNames.length, 0, "DB should have no object stores");

  try {
    indexedDBSync.deleteDatabase("thisDatabaseHadBetterNotExist");
    ok(true, "deleteDatabase on a non-existent database succeeded");
  }
  catch (ex) {
    ok(false, "deleteDatabase on a non-existent database failed");
  }

  try {
    indexedDBSync.open("thisDatabaseHadBetterNotExist");
    ok(true, "After deleting a non-existent database, open works");
  }
  catch (ex) {
    ok(false, "After deleting a non-existent database, open doesn't work");
  }

  info("Test successfully completed");
  postMessage(undefined);
};
