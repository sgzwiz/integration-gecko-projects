/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  var data = event.data;
  var objectStoreData = data.objectStoreData;

  var db = indexedDBSync.open(data.name, 1, function(trans, oldVersion) {
    ok(false, "Unexpected upgradeneeded callback ");
  });

  db.transaction(data.objectStoreName, function(trans) {
    var store = trans.objectStore(data.objectStoreName);

    ok(store.get(objectStoreData.key).name == objectStoreData.value.name,
       "Correct data");

    var index = store.index(data.indexData.name);
    ok(index.get(objectStoreData.value.name).name == objectStoreData.value.name,
       "Correct data");

  });

  postMessage(undefined);
};
