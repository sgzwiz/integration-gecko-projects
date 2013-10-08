/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;
  const osName = "foo";

  var db = indexedDBSync.open(name, 1, function(trans, oldVersion) {
    is(trans.db.objectStoreNames.length, 0, "Correct objectStoreNames list");
    trans.db.createObjectStore(osName, { autoIncrement: "true" });
  });

  var key1, key2;

  db.transaction([osName], function(trans) {
    var objectStore = trans.objectStore(osName);
    key1 = objectStore.add({});
    is(trans.mode, "readwrite", "Correct mode");
  }, "readwrite");

  db.transaction(osName, function(trans) {
    var objectStore = trans.objectStore(osName);
    key2 = objectStore.add({});
    is(trans.mode, "readwrite", "Correct mode");
  }, "readwrite");

  db.transaction([osName], function(trans) {
    var objectStore = trans.objectStore(osName);
    key1 = objectStore.put({}, key1);
    is(trans.mode, "readwrite", "Correct mode");
  }, "readwrite");

    db.transaction(osName, function(trans) {
    var objectStore = trans.objectStore(osName);
    key2 = objectStore.put({}, key2);
    is(trans.mode, "readwrite", "Correct mode");
  }, "readwrite");

  db.transaction([osName], function(trans) {
    var objectStore = trans.objectStore(osName);
    key1 = objectStore.put({}, key1);
    is(trans.mode, "readwrite", "Correct mode");
  }, "readwrite");

  db.transaction(osName, function(trans) {
    var objectStore = trans.objectStore(osName);
    key1 = objectStore.put({}, key1);
    is(trans.mode, "readwrite", "Correct mode");
  }, "readwrite");

  db.transaction([osName], function(trans) {
    var objectStore = trans.objectStore(osName);
    objectStore.delete(key1);
    is(trans.mode, "readwrite", "Correct mode");
  }, "readwrite");

  db.transaction(osName, function(trans) {
    var objectStore = trans.objectStore(osName);
    objectStore.delete(key2);
    is(trans.mode, "readwrite", "Correct mode");
  }, "readwrite");


  db.transaction([osName], function(trans) {
    var objectStore = trans.objectStore(osName);
    try {
      objectStore.add({});
      ok(false, "Adding to a readonly transaction should fail");
    }
    catch(e) {
      ok(true, "Adding to a readonly transaction failed");
    }
  });

  db.transaction(osName, function(trans) {
    var objectStore = trans.objectStore(osName);
    try {
      objectStore.add({});
      ok(false, "Adding to a readonly transaction should fail");
    }
    catch(e) {
      ok(true, "Adding to a readonly transaction failed");
    }
  });

  db.transaction([osName], function(trans) {
    var objectStore = trans.objectStore(osName);
    try {
      objectStore.put({});
      ok(false, "Adding or modifying a readonly transaction should fail");
    }
    catch(e) {
      ok(true, "Adding or modifying a readonly transaction failed");
    }
  });

  db.transaction(osName, function(trans) {
    var objectStore = trans.objectStore(osName);
    try {
      objectStore.put({});
      ok(false, "Adding or modifying a readonly transaction should fail");
    }
    catch(e) {
      ok(true, "Adding or modifying a readonly transaction failed");
    }
  });

  db.transaction([osName], function(trans) {
    var objectStore = trans.objectStore(osName);
    try {
      objectStore.put({}, key1);
      ok(false, "Modifying a readonly transaction should fail");
    }
    catch(e) {
      ok(true, "Modifying a readonly transaction failed");
    }
  });

  db.transaction(osName, function(trans) {
    var objectStore = trans.objectStore(osName);
    try {
      objectStore.put({}, key1);
      ok(false, "Modifying a readonly transaction should fail");
    }
    catch(e) {
      ok(true, "Modifying a readonly transaction failed");
    }
  });

  db.transaction([osName], function(trans) {
    var objectStore = trans.objectStore(osName);
    try {
      objectStore.delete(key1);
      ok(false, "Removing from a readonly transaction should fail");
    }
    catch(e) {
      ok(true, "Removing from a readonly transaction failed");
    }
  });

  db.transaction(osName, function(trans) {
    var objectStore = trans.objectStore(osName);
    try {
      objectStore.delete(key1);
      ok(false, "Removing from a readonly transaction should fail");
    }
    catch(e) {
      ok(true, "Removing from a readonly transaction failed");
    }
  });

  info("Test successfully completed");
  postMessage(undefined);
};
