/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  var names = [
    //"",
    null,
    undefined,
    location.pathname
  ];

  const version = 1;

  for (var i in names) {
    var name = names[i];

    var db = indexedDBSync.open(name, version, function() {
    });

    if (name === null) {
      name = "null";
    }
    else if (name === undefined) {
      name = "undefined";
    }

    is(db.name, name, "Got the right name");
    is(db.version, version, "Got the right version");
    is(db.objectStoreNames.length, 0, "Got right objectStores list");
  }

  info("Test successfully completed");
  postMessage(undefined);
};
