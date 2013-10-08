/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;

  var db = indexedDBSync.open(name, 1, function(trans, oldVersion) {});

  try {
    db.mozCreateFileHandle("random.bin", "binary/random");
    ok(false, "Should have thrown!");
  }
  catch (ex) {
    ok(true, "MozCreateFileHandle threw");
  }

  info("Test successfully completed");
  postMessage(undefined);
};
