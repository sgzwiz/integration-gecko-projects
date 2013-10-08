/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;
  const versions = [
    7,
    42
  ];

  var db = indexedDBSync.open(name, 1, function(trans, oldVersion) {});

  // Check default state.
  is(db.version, 1, "Correct default version for a new database.");
  db.close();

  for (var i = 0; i < versions.length; i++) {
    var version = versions[i];

    var db1 = indexedDBSync.open(name, version, function(trans, oldVersion) {
      is(trans.db.version, version, "Database version number updated correctly");
      is(trans.mode, "versionchange", "Correct mode");
    });

    db1.close();
  }

  info("Test successfully completed");
  postMessage(undefined);
};
