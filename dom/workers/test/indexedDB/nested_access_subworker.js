/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;
  const objectStoreName = "Objects";

  var testString = { value: "testString" };
  var testInt = { value: 1002 };

  indexedDBSync.open(name, 1, function(trans, oldVersion) {
    var objectStore = trans.db.createObjectStore(objectStoreName,
                                                 { autoIncrement: 1 });

    var key = objectStore.add(testString.value);
    var value = objectStore.get(key);
    is(value, testString.value, "Got the right value");

    key = objectStore.add(testInt.value);
    value = objectStore.get(key);
    is(value, testInt.value, "Got the right value");
  });

  info("Test successfully completed");
  postMessage(undefined);
};
