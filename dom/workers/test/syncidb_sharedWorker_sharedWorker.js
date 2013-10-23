onconnect = function(event) {
  event.ports[0].onmessage = function(event) {
    function is(actual, expected, message) {
      var rtnObj = new Object();
      rtnObj.actual = actual;
      rtnObj.expected = expected;
      rtnObj.message = message;
      event.target.postMessage(rtnObj);
    }

    function ok(actual, message) {
      is(actual, true, message);
    }

    var data = event.data;
    var objectStoreData = data.objectStoreData;

    var db = indexedDBSync.open(data.name);

    db.transaction(data.objectStoreName, function(trans) {
      var store = trans.objectStore(data.objectStoreName);

      ok(store.get(objectStoreData.key).name == objectStoreData.value.name,
         "Correct data");

      var index = store.index(data.indexData.name);
      ok(index.get(objectStoreData.value.name).name == objectStoreData.value.name,
         "Correct data");
    });

    event.target.postMessage(undefined);
  };
};
