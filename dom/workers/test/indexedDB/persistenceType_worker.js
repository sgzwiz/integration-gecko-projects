/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;
  const version = 1;

  const objectStoreName = "foo";
  const data = { key: 1, value: "bar" };

  try {
    indexedDBSync.open(name, { version: version, storage: "unknown" });
    ok(false, "Should have thrown!");
  }
  catch (e) {
    ok(e instanceof TypeError, "Got TypeError.");
    is(e.name, "TypeError", "Good error name.");
  }

  var upgradeneededcalled = false;

  var db = indexedDBSync.open(name, { version: version, storage: "persistent" },
                              function(trans, oldVersion) {
    upgradeneededcalled = true;

    var objectStore = trans.db.createObjectStore(objectStoreName, { });
  });

  ok(upgradeneededcalled, "Expected upgradeneeded callback");
  is(db.name, name, "Correct name");
  is(db.version, version, "Correct version");
  is(db.storage, "persistent", "Correct persistence type");

  db.transaction(objectStoreName,  function(trans) {
    var objectStore = trans.objectStore(objectStoreName);

    var result = objectStore.get(data.key);
    is(result, null, "Got no data");

    result = objectStore.add(data.value, data.key);
    is(result, data.key, "Got correct key");
  }, "readwrite");

  upgradeneededcalled = false;

  db = indexedDBSync.open(name, { version: version, storage: "temporary" },
                          function(trans, oldVersion) {
    upgradeneededcalled = true;

    var objectStore = trans.db.createObjectStore(objectStoreName, { });
  });

  ok(upgradeneededcalled, "Expected upgradeneeded callback");
  is(db.name, name, "Correct name");
  is(db.version, version, "Correct version");
  is(db.storage, "temporary", "Correct persistence type");

  db.transaction(objectStoreName,  function(trans) {
    var objectStore = trans.objectStore(objectStoreName);

    var result = objectStore.get(data.key);
    is(result, null, "Got no data");
  });

  info("Test successfully completed");
  postMessage(undefined);
};
