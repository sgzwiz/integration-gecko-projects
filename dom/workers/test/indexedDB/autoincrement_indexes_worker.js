/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;

  var db = indexedDBSync.open(name, 1, function(trans, oldVersion) {
    var objectStore = trans.db.createObjectStore("foo", { keyPath: "id",
                                                  autoIncrement: true });
    objectStore.createIndex("first","first");
    objectStore.createIndex("second","second");
    objectStore.createIndex("third","third");

    var data = { first: "foo", second: "foo", third: "foo" };

    var key = objectStore.add(data);
    is(key, 1, "Correct key");
  });

  db.transaction("foo", function(trans) {
    var objectStore = trans.objectStore("foo");

    var first = objectStore.index("first");
    var second = objectStore.index("second");
    var third = objectStore.index("third");

    var result = first.get("foo");
    is(result.id, 1, "Correct entry in first");

    result = second.get("foo");
    is(result.id, 1, "Correct entry in second");

    result = third.get("foo");
    is(result.id, 1, "Correct entry in third");
  });

  info("Test successfully completed");
  postMessage(undefined);
};
