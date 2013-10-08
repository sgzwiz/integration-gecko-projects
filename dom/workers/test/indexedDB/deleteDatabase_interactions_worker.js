/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;

  var upgradeneededcalled = false;

  var db = indexedDBSync.open(name, 10, function(trans, oldVersion) {
    upgradeneededcalled = true;
    trans.db.createObjectStore("stuff");
  });

  ok(upgradeneededcalled, "Expected upgradeneeded callback");
  ok(db instanceof IDBDatabaseSync, "Result should be a database");
  is(db.objectStoreNames.length, 1, "Expect an objectStore here");

  db.close();

  indexedDBSync.deleteDatabase(name);

  db = indexedDBSync.open(name, 1);

  is(db.version, 1, "DB has proper version");
  is(db.objectStoreNames.length, 0, "DB should have no object stores");

  info("Test successfully completed");
  postMessage(undefined);
};
