/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;
  const objectStoreName = "foo";
  const entryCount = 1000;

  var firstKey;

  var db = indexedDBSync.open(name, 1, function(trans, oldVersion) {
    var objectStore = trans.db.createObjectStore(objectStoreName,
                                                 { autoIncrement: true });

    for (var i = 0; i < entryCount; i++) {
      var key = objectStore.add({});
      if (!i) {
        firstKey = key;
      }
    }

    isnot(firstKey, undefined, "Got first key");
  });

  var seenEntryCount = 0;

  db.transaction("foo", function(trans) {
    var objectStore = trans.objectStore("foo");
    var cursor = objectStore.openCursor();
    if (cursor) {
      do {
        seenEntryCount++;
      } while (cursor.continue());
    }
    is(seenEntryCount, entryCount, "Correct entry count");

    try {
      objectStore.clear();
      ok(false, "clear should throw on READ_ONLY transactions");
    }
    catch (e) {
      ok(true, "clear should throw on READ_ONLY transactions");
    }
  });

  db.transaction("foo", function(trans) {
    var objectStore = trans.objectStore("foo");
    var result = objectStore.clear();
    ok(result === undefined, "Correct result");
  }, "readwrite");

  db.transaction("foo", function(trans) {
    var objectStore = trans.objectStore("foo");
    var cursor = objectStore.openCursor();
    if (cursor) {
      ok(false, "Shouldn't have any entries");
    }
  });

  db.transaction("foo", function(trans) {
    var objectStore = trans.objectStore("foo");
    var key = objectStore.add({});
    isnot(key, firstKey, "Got a different key");
  }, "readwrite");

  info("Test successfully completed");
  postMessage(undefined);
};
