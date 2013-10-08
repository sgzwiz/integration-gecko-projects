/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;
  const objectStoreName = "foo";
  const indexName = "bar";

  var objectStore1;
  var objectStore2;
  var index1;
  var index2;

  var db = indexedDBSync.open(name, 1, function(trans, oldVersion) {
    objectStore1 = trans.db.createObjectStore(objectStoreName);
    objectStore2 = trans.objectStore(objectStoreName);
    ok(objectStore1 === objectStore2, "Got same objectStores");

    index1 = objectStore1.createIndex(indexName, "key");
    index2 = objectStore2.index(indexName);
    ok(index1 === index2, "Got same indexes");

  });

  db.transaction(objectStoreName,  function(transaction) {
    var objectStore3 = transaction.objectStore(objectStoreName);
    var objectStore4 = transaction.objectStore(objectStoreName);

    ok(objectStore3 === objectStore4, "Got same objectStores");

    ok(objectStore3 !== objectStore1, "Different objectStores");
    ok(objectStore4 !== objectStore2, "Different objectStores");

    var index3 = objectStore3.index(indexName);
    var index4 = objectStore4.index(indexName);
    ok(index3 === index4, "Got same indexes");

    ok(index3 !== index1, "Different indexes");
    ok(index4 !== index2, "Different indexes");
  });

  info("Test successfully completed");
  postMessage(undefined);
};
