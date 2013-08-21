/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;
  const START_DATA = "hi";
  const END_DATA = "bye";
  const objectStoreInfo = [
    { name: "1", options: { keyPath: null }, key: 1,
      entry: { data: START_DATA } },
    { name: "2", options: { keyPath: "foo" },
      entry: { foo: 1, data: START_DATA } },
    { name: "3", options: { keyPath: null, autoIncrement: true },
      entry: { data: START_DATA } },
    { name: "4", options: { keyPath: "foo", autoIncrement: true },
      entry: { data: START_DATA } },
  ];

  for (var i = 0; i < objectStoreInfo.length; i++) {
    // Create our object stores.
    var info = objectStoreInfo[i];

    var db = indexedDBSync.open(name, i + 1, function(trans, oldVersion) {
      var objectStore = info.hasOwnProperty("options") ?
                        trans.db.createObjectStore(info.name, info.options) :
                        trans.db.createObjectStore(info.name);

      // Create the indexes on 'data' on the object store.
      var index = objectStore.createIndex("data_index", "data",
                                          { unique: false });
      var uniqueIndex = objectStore.createIndex("unique_data_index", "data",
                                                { unique: true });

      ok(true, "objectStore, index and unique index created");

      // Populate the object store with one entry of data.
      var request = info.hasOwnProperty("key") ?
                    objectStore.add(info.entry, info.key) :
                    objectStore.add(info.entry);


      // Use a cursor to update 'data' to END_DATA.
      var cursor = objectStore.openCursor();
      if (cursor) {
        ok(true, "4");
        var obj = cursor.value;
        obj.data = END_DATA;
        cursor.update(obj);
        ok(true, "5");
      }

      // Check both indexes to make sure that they were updated.
      var request = index.get(END_DATA);
      is(request.data, obj.data, "Non-unique index was properly updated.");

      request = uniqueIndex.get(END_DATA);
      is(request.data, obj.data, "Unique index was properly updated.");
    });

    db.close();
  }

  ok(true, "Test successfully completed");
  postMessage(undefined);
};
