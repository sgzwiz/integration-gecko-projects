/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;
  const keys = [1, -1, 0, 10, 2000, "q", "z", "two", "b", "a"];
  const sortedKeys = [-1, 0, 1, 10, 2000, "a", "b", "q", "two", "z"];

  is(keys.length, sortedKeys.length, "Good key setup");

  var db = indexedDBSync.open(name, 1, function(trans, oldVersion) {
    var objectStore = trans.db.createObjectStore("autoIncrement",
                                                 { autoIncrement: true });

    var cursor = objectStore.openCursor();
    ok(!cursor, "No results");

    objectStore = trans.db.createObjectStore("autoIncrementKeyPath",
                                             { keyPath: "foo",
                                               autoIncrement: true });

    cursor = objectStore.openCursor();
    ok(!cursor, "No results");

    objectStore = trans.db.createObjectStore("keyPath", { keyPath: "foo" });
    cursor = objectStore.openCursor();
    ok(!cursor, "No results");

    objectStore = trans.db.createObjectStore("foo");
    cursor = objectStore.openCursor();
    ok(!cursor, "No results");

    var keyIndex = 0;

    for (var i in keys) {
      objectStore.add("foo", keys[i]);
      keyIndex++
    }
    ok(keyIndex == keys.length, "Correct length");

    keyIndex = 0;

    cursor = objectStore.openCursor();
    if (cursor) {
      do {
        is(cursor.key, sortedKeys[keyIndex], "Correct key");
        is(cursor.primaryKey, sortedKeys[keyIndex], "Correct primary key");
        is(cursor.value, "foo", "Correct value");
        keyIndex++;
        /*
        try {
          cursor.continue();
          cursor.continue();
          ok(false, "continue twice should throw");    // ASYNC SPECIFIC
        }
        catch (e) {
          ok(e instanceof DOMException, "got a database exception");
          is(e.name, "InvalidStateError", "correct error");
          is(e.code, DOMException.INVALID_STATE_ERR, "correct code");
        }
        */
      } while (cursor.continue());
    }

    is(keyIndex, keys.length, "Saw all added items");

    keyIndex = 4;

    var range = IDBKeyRange.bound(2000, "q");
    cursor = objectStore.openCursor(range);
    if (cursor) {
      do {
        is(cursor.key, sortedKeys[keyIndex], "Correct key");
        is(cursor.primaryKey, sortedKeys[keyIndex], "Correct primary key");
        is(cursor.value, "foo", "Correct value");
        keyIndex++;
      } while (cursor.continue());
    }
    is(keyIndex, 8, "Saw all the expected keys");

    keyIndex = 0;

    cursor = objectStore.openCursor();
    if (cursor) {
      var run = true;
      do {
        is(cursor.key, sortedKeys[keyIndex], "Correct key");
        is(cursor.primaryKey, sortedKeys[keyIndex], "Correct primary key");
        is(cursor.value, "foo", "Correct value");

        if (keyIndex) {
          run = cursor.continue();
        }
        else {
          run = cursor.continue("b");
        }
        keyIndex += keyIndex ? 1: 6;
      } while(run);
    }
    is(keyIndex, keys.length, "Saw all the expected keys");

    keyIndex = 0;

    var cursor = objectStore.openCursor();
    if (cursor) {
      var run = true;
      do {
        is(cursor.key, sortedKeys[keyIndex], "Correct key");
        is(cursor.primaryKey, sortedKeys[keyIndex], "Correct primary key");
        is(cursor.value, "foo", "Correct value");

        if (keyIndex) {
          run = cursor.continue();
        }
        else {
          run =cursor.continue(10);
        }
        keyIndex += keyIndex ? 1: 3;
      } while(run);
    }
    is(keyIndex, keys.length, "Saw all the expected keys");

    keyIndex = 0;

    cursor = objectStore.openCursor();
    if (cursor) {
      var run = true;
      do {
        is(cursor.key, sortedKeys[keyIndex], "Correct key");
        is(cursor.primaryKey, sortedKeys[keyIndex], "Correct primary key");
        is(cursor.value, "foo", "Correct value");

        if (keyIndex) {
          run = cursor.continue();
        }
        else {
          run = cursor.continue("c");
        }
        keyIndex += keyIndex ? 1 : 7;
      } while(run);
    }
    is(keyIndex, keys.length, "Saw all the expected keys");

    keyIndex = 0;

    cursor = objectStore.openCursor();
    var storedCursor = null;
    if (cursor) {
      var run = true;
      do {
        storedCursor = cursor;

        is(cursor.key, sortedKeys[keyIndex], "Correct key");
        is(cursor.primaryKey, sortedKeys[keyIndex], "Correct primary key");
        is(cursor.value, "foo", "Correct value");

        if (keyIndex == 4) {
          cursor.update("bar");
          keyIndex++;
          run = cursor.continue();
        }
        else {
          keyIndex++;
          run = cursor.continue();
        }
      } while(run);
    }
    ok(storedCursor.value === undefined, "The cursor's value should be undefined.");
    is(keyIndex, keys.length, "Saw all the expected keys");

    var request = objectStore.get(sortedKeys[4]);
    is(request, "bar", "Update succeeded");

    request = objectStore.put("foo", sortedKeys[4]);
    keyIndex = 0;

    var gotRemoveEvent = false;
    var retval = false;

    cursor = objectStore.openCursor(null, "next");
    storedCursor = null;
    if (cursor) {
      do {
        storedCursor = cursor;

        is(cursor.key, sortedKeys[keyIndex], "Correct key");
        is(cursor.primaryKey, sortedKeys[keyIndex], "Correct primary key");
        is(cursor.value, "foo", "Correct value");

        if (keyIndex == 4) {
          request = cursor.delete();
          ok(request == true, "Should be true");
          gotRemoveEvent = true;
        }

        keyIndex++;
      } while(cursor.continue());
    }
    ok(storedCursor.value === undefined, "The cursor's value should be undefined.");
    is(keyIndex, keys.length, "Saw all the expected keys");
    is(gotRemoveEvent, true, "Saw the remove event");

    request = objectStore.get(sortedKeys[4]);
    ok(request == undefined, "Entry was deleted");

    request = objectStore.add("foo", sortedKeys[4]);
    keyIndex = sortedKeys.length - 1;

    cursor = objectStore.openCursor(null, "prev");
    storedCursor = null;
    if (cursor) {
      do {
        storedCursor = cursor;
        is(cursor.key, sortedKeys[keyIndex], "Correct key");
        is(cursor.primaryKey, sortedKeys[keyIndex], "Correct primary key");
        is(cursor.value, "foo", "Correct value");
        keyIndex--;
      } while(cursor.continue());
    }
    ok(storedCursor.value === undefined, "The cursor's value should be undefined.");
    is(keyIndex, -1, "Saw all added items");
  });

  info("Test successfully completed");
  postMessage(undefined);
};
