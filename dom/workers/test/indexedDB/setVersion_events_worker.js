/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;

  // Open a datbase for the first time.
  var db = indexedDBSync.open(name, 1);

  var versionChangeEventCount = 0;
  var db1, db2, db3;

  db1 = db;
  db1.addEventListener("versionchange", function(event) {
    ok(true, "Got version change event");
    ok(event instanceof IDBVersionChangeEvent, "Event is of the right type");
    is(event.target.source, null, "Correct source");
    is(event.target, db1, "Correct target");
    is(event.target.version, 1, "Correct db version");
    is(event.oldVersion, 1, "Correct event oldVersion");
    is(event.newVersion, 2, "Correct event newVersion");
    is(versionChangeEventCount++, 0, "Correct count");
    db1.close();
  }, false);

  // Open the database again and trigger an upgrade that should succeed
  var upgradeneededcalled = false;
  db = indexedDBSync.open(name, 2, function(trans, oldVersion) {
    // Test the upgradeneeded.
    upgradeneededcalled = true;
    ok(trans.db instanceof IDBDatabaseSync, "Good result");
    db2 = trans.db;
    is(trans.mode, "versionchange", "Correct mode");
    is(db2.version, 2, "Correct db version");
    is(oldVersion, 1, "Correct upgrade callback oldVersion")
  }//, unexpectedCallback
  );
  todo(false, "Need to fix blocked events in child processes!");

  ok(upgradeneededcalled, "Expected upgradeneeded callback");

  db2.addEventListener("versionchange", function(event) {
    ok(true, "Got version change event");
    ok(event instanceof IDBVersionChangeEvent, "Event is of the right type");
    is(event.target.source, null, "Correct source");
    is(event.target, db2, "Correct target");
    is(event.target.version, 2, "Correct db version");
    is(event.oldVersion, 2, "Correct event oldVersion");
    is(event.newVersion, 3, "Correct event newVersion");
    is(versionChangeEventCount++, 1, "Correct count");
  }, false);

  // Test opening the existing version again
  upgradeneededcalled = false;
  db = indexedDBSync.open(name, 2, function(trans, oldVersion) {
    upgradeneededcalled = true;
  }//, unexpectedCallback
  );
  todo(false, "Need to fix blocked events in child processes!");

  ok(!upgradeneededcalled, "Didn't expect upgrade callback");

  db3 = db;

  // Test an upgrade that should fail
  var upgradeblockedcalled = false;
  upgradeneededcalled = false;
  db = indexedDBSync.open(name, 3, function(trans, oldVersion) {
    ok(upgradeblockedcalled, "Expected upgrade blocked callback at first");
    upgradeneededcalled = true;
  }, function(oldVersion) {
    ok(!upgradeneededcalled, "Expected upgrade blocked callback at first");
    upgradeblockedcalled = true;
    ok(true, "Got upgrade blocked callback");
    is(db3.version, 2, "Correct db version");
    is(oldVersion, 2, "Correct upgradeblocked callback oldVersion")
    versionChangeEventCount++;
    db2.close();
    db3.close();
  });

  ok(upgradeblockedcalled, "Expected upgrade blocked callback");
  ok(upgradeneededcalled, "Expected upgrade callback");

  db3 = db;
  db3.close();

  // Test another upgrade that should succeed.
  upgradeneededcalled = false;
  db = indexedDBSync.open(name, 4, function(trans, oldVersion) {
    upgradeneededcalled = true;
    ok(trans.db instanceof IDBDatabaseSync, "Good result");
    is(trans.mode, "versionchange", "Correct mode");
    is(oldVersion, 3, "Correct upgrade callback oldVersion")
  }//, unexpectedCallback
  );
  todo(false, "Need to fix blocked events in child processes!");

  ok(upgradeneededcalled, "Expected upgrade callback");

  ok(db instanceof IDBDatabaseSync, "Expect a database here");
  is(db.version, 4, "Right version");
  is(db3.version, 3, "After closing the version should not change!");
  is(db2.version, 2, "After closing the version should not change!");
  is(db1.version, 1, "After closing the version should not change!");

  is(versionChangeEventCount, 3, "Saw all expected events");

  var event = new IDBVersionChangeEvent("versionchange");
  ok(event, "Should be able to create an event with just passing in the type");
  event = new IDBVersionChangeEvent("versionchange", {oldVersion: 1});
  ok(event, "Should be able to create an event with just the old version");
  is(event.oldVersion, 1, "Correct old version");
  is(event.newVersion, null, "Correct new version");
  event = new IDBVersionChangeEvent("versionchange", {newVersion: 1});
  ok(event, "Should be able to create an event with just the new version");
  is(event.oldVersion, 0, "Correct old version");
  is(event.newVersion, 1, "Correct new version");
  event = new IDBVersionChangeEvent("versionchange", {oldVersion: 1, newVersion: 2});
  ok(event, "Should be able to create an event with both versions");
  is(event.oldVersion, 1, "Correct old version");
  is(event.newVersion, 2, "Correct new version");

  info("Test successfully completed");
  postMessage(undefined);
};
