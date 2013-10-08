/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;

  var data = [
    { name: "inline key; key generator",
      autoIncrement: true,
      storedObject: {name: "Lincoln"},
      keyName: "id",
      keyValue: undefined,
    },
    { name: "inline key; no key generator",
      autoIncrement: false,
      storedObject: {id: 1, name: "Lincoln"},
      keyName: "id",
      keyValue: undefined,
    },
    { name: "out of line key; key generator",
      autoIncrement: true,
      storedObject: {name: "Lincoln"},
      keyName: undefined,
      keyValue: undefined,
    },
    { name: "out of line key; no key generator",
      autoIncrement: false,
      storedObject: {name: "Lincoln"},
      keyName: null,
      keyValue: 1,
    }
  ];

  for (var i = 0; i < data.length; i++) {
    var test = data[i];

    var db = indexedDBSync.open(name, i+1, function(trans, oldVersion) {
      var objectStore = trans.db.createObjectStore(
                                         test.name,
                                         { keyPath: test.keyName,
                                           autoIncrement: test.autoIncrement });

      var id = objectStore.add(test.storedObject, test.keyValue);
      var value = objectStore.get(id);

      // Sanity check!
      is(value.name, test.storedObject.name, "The correct object was stored.");

      var request = objectStore.delete(id);

      // Make sure it was removed.
      value = objectStore.get(id);
      ok(value === undefined, "Object was deleted");
    });
    db.close();
  }

  info("Test successfully completed");
  postMessage(undefined);
};
