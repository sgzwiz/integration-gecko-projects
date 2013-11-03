/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;

  var versionChangeEventCount = 0;
  var db1, db2, db3; 
  
  var onversionchangecalled = false;
  var onversionchangecalled2 = false;
  
  // Open a datbase for the first time.
  db1 = indexedDBSync.open(name, 1);
  /*
  function closeDB(event) {
    onversionchangecalled = true;
    //ok(event instanceof IDBVersionChangeEvent, "Event is of the right type");
    is(event.target.source, null, "Correct source");
    is(event.target, db1, "Correct target");
    is(event.target.version, 1, "Correct db version");
    is(event.oldVersion, 1, "Correct event oldVersion");
    is(event.newVersion, 2, "Correct event newVersion");
    is(versionChangeEventCount++, 0, "Correct count");
    db1.close();
    db1.onversionchange = unexpectedEventHandler;
  };
  
  db1.onversionchange = closeDB;
  */
  
  db1.close();

  var upgradeneededcalled = false;
  // Open the database again and trigger an upgrade that should succeed
  // Test the upgradeneeded
  db2 = indexedDBSync.open(name, 2, function(trans, oldVersion) {
    is(oldVersion, 1, "Correct upgradeneeded callback oldVersion")
    upgradeneededcalled = true;
    is(trans.mode, "versionchange", "Correct mode");
  });

  ok(upgradeneededcalled, "Expected upgradeneeded callback");
  ok(db2 instanceof IDBDatabaseSync, "Good result");
  is(db2.version, 2, "Correct db version");

/*
  function closeDB2(event) {
    onversionchangecalled2 = true;
    //ok(event instanceof IDBVersionChangeEvent, "Event is of the right type");
    is(event.target.source, null, "Correct source");
    is(event.target, db2, "Correct target");
    is(event.target.version, 2, "Correct db version");
    is(event.oldVersion, 2, "Correct event oldVersion");
    is(event.newVersion, 3, "Correct event newVersion");
    is(versionChangeEventCount++, 1, "Correct count");
    db2.close();
    db2.onversionchange = unexpectedEventHandler;
  };
  
  db2.onversionchange = closeDB2;
  */
  
  db2.close();

  // Test opening the existing version again
  db3 = indexedDBSync.open(name, 3, function(trans, oldVersion) {
    is(oldVersion, 2, "Correct upgradeneeded callback oldVersion")
    is(trans.mode, "versionchange", "Correct mode");
  });

  db3.close();

  // ok(onversionchangecalled, "Expected versionchange event");
  // ok(onversionchangecalled2, "Expected versionchange event");
  
  is(db3.version, 3, "After closing the version should not change!");
  is(db2.version, 2, "After closing the version should not change!");
  is(db1.version, 1, "After closing the version should not change!");
  //is(versionChangeEventCount, 3, "Saw all expected events");


  ok(true, "Test successfully completed");
  postMessage(undefined);
};

