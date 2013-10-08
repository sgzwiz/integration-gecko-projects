/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  const name = location.pathname;

  var db = indexedDBSync.open(name, 1, function(trans, oldVersion) {
    /*
    db.addEventListener("error", function(event) {
      event.preventDefault();
    }, false);
    */
    var objectStore = trans.db.createObjectStore("foo", { autoIncrement: true });

    var key1 = objectStore.add({});
    var request = objectStore.put({}, key1);
    is(request, key1, "Put gave the same key back");

    var key2 = 10;

    request = objectStore.put({}, key2);
    is(request, key2, "Put gave the same key back");

    key2 = 100;

    request = objectStore.add({}, key2);
    is(request, key2, "Add gave correct key back");

    try {
      objectStore.put({});
      ok(true, "Put with no key should not throw with autoIncrement");
    }
    catch (e) {
      ok(false, "Put with no key threw with autoIncrement");
    }

    try {
      objectStore.put({});
      ok(true, "Put with no key should not throw with autoIncrement");
    }
    catch (e) {
      ok(false, "Put with no key threw with autoIncrement");
    }

    try {
      objectStore.delete();
      ok(false, "Remove with no key should throw");
    }
    catch (e) {
      ok(true, "Remove with no key threw");
    }

    objectStore = trans.db.createObjectStore("bar");

    try {
      objectStore.add({});
      ok(false, "Add with no key should throw");
    }
    catch (e) {
      ok(true, "Add with no key threw");
    }

    try {
      objectStore.put({});
      ok(false, "Put with no key should throw");
    }
    catch (e) {
      ok(true, "Put with no key threw");
    }

    try {
      objectStore.put({});
      ok(false, "Put with no key should throw");
    }
    catch (e) {
      ok(true, "Put with no key threw");
    }

    try {
      objectStore.delete();
      ok(false, "Remove with no key should throw!");
    }
    catch (e) {
      ok(true, "Remove with no key threw");
    }

    objectStore = trans.db.createObjectStore("baz", { keyPath: "id" });

    try {
      objectStore.add({});
      ok(false, "Add with no key should throw");
    }
    catch (e) {
      ok(true, "Add with no key threw");
    }

    try {
      objectStore.add({id:5}, 5);
      ok(false, "Add with inline key and passed key should throw");
    }
    catch (e) {
      ok(true, "Add with inline key and passed key threw");
    }

    try {
      objectStore.put({});
      ok(false, "Put with no key should throw");
    }
    catch (e) {
      ok(true, "Put with no key threw");
    }

    try {
      objectStore.put({});
      ok(false, "Put with no key should throw");
    }
    catch (e) {
      ok(true, "Put with no key threw");
    }

    try {
      objectStore.delete();
      ok(false, "Remove with no key should throw");
    }
    catch (e) {
      ok(true, "Remove with no key threw");
    }

    key1 = 10;

    request = objectStore.add({id:key1});
    is(request, key1, "Add gave back the same key");

    request = objectStore.put({id:10});
    is(request, key1, "Put gave back the same key");

    request = objectStore.put({id:10});
    is(request, key1, "Put gave back the same key");

    expectException( function() {
      request = objectStore.add({id:10});
    }, "ConstraintError", 0);

    try {
      objectStore.add({}, null);
      ok(false, "Add with null key should throw");
    }
    catch (e) {
      ok(true, "Add with null key threw");
    }

    try {
      objectStore.put({}, null);
      ok(false, "Put with null key should throw");
    }
    catch (e) {
      ok(true, "Put with null key threw");
    }

    try {
      objectStore.put({}, null);
      ok(false, "Put with null key should throw");
    }
    catch (e) {
      ok(true, "Put with null key threw");
    }

    try {
      objectStore.delete(null);
      ok(false, "Remove with null key should throw");
    }
    catch (e) {
      ok(true, "Remove with null key threw");
    }

    objectStore = trans.db.createObjectStore("bazing", { keyPath: "id",
                                                         autoIncrement: true });

    key1 = objectStore.add({});
    request = objectStore.put({id:key1});
    is(request, key1, "Put gave the same key back");

    key2 = 10;

    request = objectStore.put({id:key2});
    is(request, key2, "Put gave the same key back");

    try {
      objectStore.put({});
      ok(true, "Put with no key should not throw with autoIncrement!");
    }
    catch (e) {
      ok(false, "Put with no key threw with autoIncrement");
    }

    try {
      objectStore.put({});
      ok(true, "Put with no key should not throw with autoIncrement");
    }
    catch (e) {
      ok(false, "Put with no key threw with autoIncrement");
    }

    try {
      objectStore.delete();
      ok(false, "Remove with no key should throw");
    }
    catch (e) {
      ok(true, "Remove with no key threw");
    }

    try {
      objectStore.add({id:5}, 5);
      ok(false, "Add with inline key and passed key should throw");
    }
    catch (e) {
      ok(true, "Add with inline key and passed key threw");
    }
    request = objectStore.delete(key2);
  });

  info("Test successfully completed");
  postMessage(undefined);
};
