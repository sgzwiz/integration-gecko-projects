/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;
  const objectStoreName = "Objects";

  var testString = { key: 0, value: "testString" };
  var testInt = { key: 1, value: 1002 };

  indexedDBSync.open(name, 1, function(trans, oldVersion) {
    var objectStore = trans.db.createObjectStore(objectStoreName, { });

    var key = objectStore.add(testString.value, testString.key);
    is(key, testString.key, "Got the right key");
    var value = objectStore.get(testString.key);
    is(value, testString.value, "Got the right value");

    key = objectStore.add(testInt.value, testInt.key);
    is(key, testInt.key, "Got the right key");
    value = objectStore.get(testInt.key);
    is(value, testInt.value, "Got the right value");
  });

  info("Test successfully completed");
  postMessage(undefined);
};
