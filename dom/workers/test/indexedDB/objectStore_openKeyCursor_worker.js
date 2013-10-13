/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;
  const dbVersion = 1;
  const objectStoreName = "foo";
  const keyCount = 100;

  var db = indexedDBSync.open(name, dbVersion, function(trans, oldVersion) {
    info("Creating database");
    var objectStore = trans.db.createObjectStore(objectStoreName);

    for (var i = 0; i < keyCount; i++) {
      objectStore.add(true, i);
    }
  });

  db.transaction(objectStoreName, function(trans) {
    var objectStore = trans.objectStore(objectStoreName);

    info("Getting all keys");
    const allKeys = objectStore.getAllKeys();
    is(allKeys instanceof Array, true, "Got an array result");
    is(allKeys.length, keyCount, "Correct length");

    info("Opening normal key cursor");
    var seenKeys = [];
    var cursor = objectStore.openKeyCursor();
    if (cursor) {
      do {
        ok(cursor.source == objectStore, "Correct source");
        is(cursor.direction, "next", "Correct direction");

        var exception = null;
        try {
          cursor.update(10);
        } catch(e) {
          exception = e;
        }
        ok(!!exception, "update() throws for key cursor");

        exception = null;
        try {
          cursor.delete();
        } catch(e) {
          exception = e;
        }
        ok(!!exception, "delete() throws for key cursor");

        is(cursor.key, cursor.primaryKey, "key and primaryKey match");
        ok(!("value" in cursor), "No 'value' property on key cursor");

        seenKeys.push(cursor.key);
      } while (cursor.continue());
    }

    is(seenKeys.length, allKeys.length, "Saw the right number of keys");

    var match = true;
    for (var i = 0; i < seenKeys.length; i++) {
      if (seenKeys[i] !== allKeys[i]) {
        match = false;
        break;
      }
    }
    ok(match, "All keys matched");

    info("Opening key cursor with keyRange");
    var keyRange = IDBKeyRange.bound(10,20,false,true);

    seenKeys = [];
    var cursor = objectStore.openKeyCursor(keyRange);
    if (cursor) {
      do {
        ok(cursor.source == objectStore, "Correct source");
        is(cursor.direction, "next", "Correct direction");

        var exception = null;
        try {
          cursor.update(10);
        } catch(e) {
          exception = e;
        }
        ok(!!exception, "update() throws for key cursor");

        exception = null;
        try {
          cursor.delete();
        } catch(e) {
          exception = e;
        }
        ok(!!exception, "delete() throws for key cursor");

        is(cursor.key, cursor.primaryKey, "key and primaryKey match");
        ok(!("value" in cursor), "No 'value' property on key cursor");

        seenKeys.push(cursor.key);
      } while (cursor.continue());
    }
    is(seenKeys.length, 10, "Saw the right number of keys");

    var match = true;
    for (var i = 0; i < seenKeys.length; i++) {
      if (seenKeys[i] !== allKeys[i+10]) {
        match = false;
        break;
      }
    }
    ok(match, "All keys matched");

    info("Opening key cursor with unmatched keyRange");

    keyRange = IDBKeyRange.bound(10000, 200000);

    seenKeys = [];
    var cursor = objectStore.openKeyCursor(keyRange);
    if (cursor) {
      do {
        ok(false, "Shouldn't have any keys here");
      } while (cursor.continue());
    }
    is(seenKeys.length, 0, "Saw the right number of keys");

    info("Opening reverse key cursor");
    seenKeys = [];
    var cursor = objectStore.openKeyCursor(null, "prev");
    if (cursor) {
      do {
        ok(cursor.source == objectStore, "Correct source");
        is(cursor.direction, "prev", "Correct direction");

        var exception = null;
        try {
          cursor.update(10);
        } catch(e) {
          exception = e;
        }
        ok(!!exception, "update() throws for key cursor");

        exception = null;
        try {
          cursor.delete();
        } catch(e) {
          exception = e;
        }
        ok(!!exception, "delete() throws for key cursor");

        is(cursor.key, cursor.primaryKey, "key and primaryKey match");
        ok(!("value" in cursor), "No 'value' property on key cursor");

        seenKeys.push(cursor.key);
      } while (cursor.continue());
    }

    is(seenKeys.length, allKeys.length, "Saw the right number of keys");
    seenKeys.reverse();

    var match = true;
    for (var i = 0; i < seenKeys.length; i++) {
      if (seenKeys[i] !== allKeys[i]) {
        match = false;
        break;
      }
    }
    ok(match, "All keys matched");

    info("Opening reverse key cursor with key range");

    keyRange = IDBKeyRange.bound(10, 20, false, true);
    seenKeys = [];
    var cursor = objectStore.openKeyCursor(keyRange, "prev");
    if (cursor) {
      do {
        ok(cursor.source == objectStore, "Correct source");
        is(cursor.direction, "prev", "Correct direction");

        var exception = null;
        try {
          cursor.update(10);
        } catch(e) {
          exception = e;
        }
        ok(!!exception, "update() throws for key cursor");

        exception = null;
        try {
          cursor.delete();
        } catch(e) {
          exception = e;
        }
        ok(!!exception, "delete() throws for key cursor");

        is(cursor.key, cursor.primaryKey, "key and primaryKey match");
        ok(!("value" in cursor), "No 'value' property on key cursor");

        seenKeys.push(cursor.key);
      } while (cursor.continue());
    }

    is(seenKeys.length, 10, "Saw the right number of keys");
    seenKeys.reverse();

    var match = true;
    for (var i = 0; i < seenKeys.length; i++) {
      if (seenKeys[i] !== allKeys[i+10]) {
        match = false;
        break;
      }
    }
    ok(match, "All keys matched");

    info("Opening reverse key cursor with unmatched key range");

    keyRange = IDBKeyRange.bound(10000, 20000);
    seenKeys = [];
    var cursor = objectStore.openKeyCursor(keyRange, "prev");
    if (cursor) {
      do {
        ok(false, "Shouldn't have any keys here");
      } while (cursor.continue());
    }

    is(seenKeys.length, 0, "Saw the right number of keys");

    info("Opening key cursor with advance");

    var run = true;
    seenKeys = [];
    var cursor = objectStore.openKeyCursor();
    if (cursor) {
      do {
        ok(cursor.source == objectStore, "Correct source");
        is(cursor.direction, "next", "Correct direction");

        var exception = null;
        try {
          cursor.update(10);
        } catch(e) {
          exception = e;
        }
        ok(!!exception, "update() throws for key cursor");

        exception = null;
        try {
          cursor.delete();
        } catch(e) {
          exception = e;
        }
        ok(!!exception, "delete() throws for key cursor");

        is(cursor.key, cursor.primaryKey, "key and primaryKey match");
        ok(!("value" in cursor), "No 'value' property on key cursor");

        seenKeys.push(cursor.key);
        if (seenKeys.length == 1) {
          run = cursor.advance(10);
        } else {
          run = cursor.continue();
        }
      } while (run);
    }

    is(seenKeys.length, allKeys.length - 9, "Saw the right number of keys");

    var match = true;
    for (var i = 0, j = 0; i < seenKeys.length; i++) {
      if (seenKeys[i] !== allKeys[i+j]) {
        match = false;
        break;
      }
      if (i == 0) {
        j = 9;
      }
    }
    ok(match, "All keys matched");

    info("Opening key cursor with continue-to-key");

    var run = true;
    seenKeys = [];
    var cursor = objectStore.openKeyCursor();
    if (cursor) {
      do {
        ok(cursor.source == objectStore, "Correct source");
        is(cursor.direction, "next", "Correct direction");

        var exception = null;
        try {
          cursor.update(10);
        } catch(e) {
          exception = e;
        }
        ok(!!exception, "update() throws for key cursor");

        exception = null;
        try {
          cursor.delete();
        } catch(e) {
          exception = e;
        }
        ok(!!exception, "delete() throws for key cursor");

        is(cursor.key, cursor.primaryKey, "key and primaryKey match");
        ok(!("value" in cursor), "No 'value' property on key cursor");

        seenKeys.push(cursor.key);

        if (seenKeys.length == 1) {
          run = cursor.continue(10);
        } else {
          run = cursor.continue();
        }
      } while (run);
    }

    is(seenKeys.length, allKeys.length - 9, "Saw the right number of keys");

    var match = true;
    for (var i = 0, j = 0; i < seenKeys.length; i++) {
      if (seenKeys[i] !== allKeys[i+j]) {
        match = false;
        break;
      }
      if (i == 0) {
        j = 9;
      }
    }
    ok(match, "All keys matched");

  }, "readwrite");

  info("Test successfully completed");
  postMessage(undefined);
};
