/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;
  const objectStoreName = "foo";

  indexedDBSync.open(name, 1, function(trans, oldVersion) {

    var objectStore =
      trans.db.createObjectStore(objectStoreName, { keyPath: null });
    var key = 10;

    var result = objectStore.add({}, key);
    is(result, key, "Correct key");

    expectException(function() {
      objectStore.add({}, key);
    }, "ConstraintError");
  });

  info("Test successfully completed");
  postMessage(undefined);
};
