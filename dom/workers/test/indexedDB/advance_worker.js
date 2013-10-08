/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;
  const dataCount = 30;
  const emptyName = "";

  var db = indexedDBSync.open(name, 1, function(trans, oldVersion) {
    var objectStore = trans.db.createObjectStore(emptyName, { keyPath: "key" });
    objectStore.createIndex("", "index");

    for (var i = 0; i < dataCount; i++) {
      objectStore.add({ key: i, index: i });
    }
  });

  var count = 0;
  db.transaction(emptyName, function(trans) {
    var objectStore = trans.objectStore(emptyName);
    var cursor = objectStore.openCursor();
    if (cursor) {
      do {
        count++;
      } while(cursor.continue());
    }
    is(count, dataCount, "Saw all data");
  });

  count = 0;
  var run = true;

  db.transaction(emptyName, function(trans) {
    var objectStore = trans.objectStore(emptyName);
    var cursor = objectStore.openCursor();
    if (cursor) {
      do {
        is(cursor.primaryKey, count, "Got correct object");
        if (count) {
          count++;
          run = cursor.continue();
        }
        else {
          count = 10;
          run = cursor.advance(10);
        }
      } while(run);
    }
    is(count, dataCount, "Saw all data");
  });

  count = 0;
  run = true;

  db.transaction(emptyName, function(trans) {
    var objectStore = trans.objectStore(emptyName);
    var index = objectStore.index(emptyName);
    var cursor = index.openCursor();
    if (cursor) {
      do {
        is(cursor.primaryKey, count, "Got correct object");
        if (count) {
          count++;
          run = cursor.continue();
        }
        else {
          count = 10;
          run = cursor.advance(10);
        }
      } while(run);
    }
    is(count, dataCount, "Saw all data");
  });

  count = 0;
  run = true;

  db.transaction(emptyName, function(trans) {
    var objectStore = trans.objectStore(emptyName);
    var index = objectStore.index(emptyName);
    var cursor = index.openKeyCursor();
    if (cursor) {
      do {
        is(cursor.primaryKey, count, "Got correct object");
        if (count) {
          count++;
          run = cursor.continue();
        }
        else {
          count = 10;
          run = cursor.advance(10);
        }
      } while(run);
    }
    is(count, dataCount, "Saw all data");
  });

  count = 0;
  run = true;

  db.transaction(emptyName, function(trans) {
    var objectStore = trans.objectStore(emptyName);
    var cursor = objectStore.openCursor();
    if (cursor) {
      do {
        is(cursor.primaryKey, count, "Got correct object");
        if (count == 0) {
          run = cursor.advance(dataCount + 1);
        }
        else {
          ok(false, "Should never get here!");
          run = cursor.continue();
        }
      } while(run);
    }
    is(count, 0, "Saw all data");
  });

  count = dataCount - 1;
  run = true;

  db.transaction(emptyName, function(trans) {
    var objectStore = trans.objectStore(emptyName);
    var cursor = objectStore.openCursor(null, "prev");
    if (cursor) {
      do {
        is(cursor.primaryKey, count, "Got correct object");
        count--;
        if (count == dataCount - 2) {
          run = cursor.advance(10);
          count -= 9;
        }
        else {
         run = cursor.continue();
        }
      } while(run);
    }
    is(count, -1, "Saw all data");
  });

  count = dataCount - 1;
  run = true;

  db.transaction(emptyName, function(trans) {
    var objectStore = trans.objectStore(emptyName);
    var cursor = objectStore.openCursor(null, "prev");
    if (cursor) {
      do {
        is(cursor.primaryKey, count, "Got correct object");
        if (count == dataCount - 1) {
          run = cursor.advance(dataCount + 1);
        }
        else {
          ok(false, "Should never get here!");
          run = cursor.continue();
        }
      } while(run);
    }
    is(count, dataCount - 1, "Saw all data");
  });

  info("Test successfully completed");
  postMessage(undefined);
};
