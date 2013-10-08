/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;

  indexedDBSync.open(name, 1, function(trans, oldVersion) {
    var test = {
      name: "inline key; key generator",
      autoIncrement: true,
      storedObject: {name: "Lincoln"},
      keyName: "id",
    };

    var objectStore = trans.db.createObjectStore(
                                         test.name,
                                         { keyPath: test.keyName,
                                           autoIncrement: test.autoIncrement });

    var id = objectStore.add(test.storedObject);
    var value = objectStore.get(id);

    // Sanity check!
    is(value.name, test.storedObject.name, "The correct object was stored.");

    // Ensure that the id was also stored on the object.
    is(value.id, id, "The object had the id stored on it.");
  });

  info("Test successfully completed");
  postMessage(undefined);
};
