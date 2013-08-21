/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;

  try {
    indexedDBSync.open(name, 0);
    ok(false, "Should have thrown!");
  }
  catch (e) {
    ok(e instanceof TypeError, "Got TypeError.");
    is(e.name, "TypeError", "Good error name.");
  }

  try {
    indexedDBSync.open(name, -1);
    ok(false, "Should have thrown!");
  }
  catch (e) {
    ok(e instanceof TypeError, "Got TypeError.");
    is(e.name, "TypeError", "Good error name.");
  }

  postMessage(undefined);
};
