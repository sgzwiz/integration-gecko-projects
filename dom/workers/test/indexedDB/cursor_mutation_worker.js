/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;
  const objectStoreData = [
    // This one will be removed.
    { ss: "237-23-7732", name: "Bob" },

    // These will always be included.
    { ss: "237-23-7733", name: "Ann" },
    { ss: "237-23-7734", name: "Ron" },
    { ss: "237-23-7735", name: "Sue" },
    { ss: "237-23-7736", name: "Joe" },

    // This one will be added.
    { ss: "237-23-7737", name: "Pat" }
  ];

  // Post-add and post-remove data ordered by name.
  const objectStoreDataNameSort = [ 1, 4, 5, 2, 3 ];

  var db = indexedDBSync.open(name, 1, function(trans, oldVersion) {
    var objectStore = trans.db.createObjectStore("foo", { keyPath: "ss" });
    objectStore.createIndex("name", "name", { unique: true });

    for (var i = 0; i < objectStoreData.length - 1; i++) {
      objectStore.add(objectStoreData[i]);
    }
  });

  var count = 0;
  var sawAdded = false;
  var sawRemoved = false;

  db.transaction("foo", function(trans) {
    var objectStore =trans.objectStore("foo");
    var cursor = objectStore.openCursor();
      if (cursor) {
        do {
          if (cursor.value.name == objectStoreData[0].name) {
            sawRemoved = true;
          }
          if (cursor.value.name ==
              objectStoreData[objectStoreData.length - 1].name) {
            sawAdded = true;
          }
          count++;
        } while(cursor.continue());
      }
    });

  is(count, objectStoreData.length - 1, "Good initial count");
  is(sawAdded, false, "Didn't see item that is about to be added");
  is(sawRemoved, true, "Saw item that is about to be removed");

  count = 0;
  sawAdded = false;
  sawRemoved = false;

  db.transaction("foo", function(trans) {
    var objectStore =trans.objectStore("foo");
    var index = objectStore.index("name");
    var cursor = index.openCursor();
    if (cursor) {
      var run = true;
      do {
        if (cursor.value.name == objectStoreData[0].name) {
          sawRemoved = true;
        }
        if (cursor.value.name ==
            objectStoreData[objectStoreData.length - 1].name) {
          sawAdded = true;
        }

        is(cursor.value.name,
           objectStoreData[objectStoreDataNameSort[count++]].name,
           "Correct name");

        if (count == 1) {
          var objectStore = trans.objectStore("foo");
          objectStore.delete(objectStoreData[0].ss);
          objectStore.add(objectStoreData[objectStoreData.length - 1]);
        }
      } while(cursor.continue());
    }
  },"readwrite");

  is(count, objectStoreData.length - 1, "Good final count");
  is(sawAdded, true, "Saw item that was added");
  is(sawRemoved, false, "Didn't see item that was removed");

  info("Test successfully completed");
  postMessage(undefined);
};
