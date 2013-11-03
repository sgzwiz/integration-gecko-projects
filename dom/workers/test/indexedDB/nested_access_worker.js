/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

importScripts("helpers_worker.js");

onmessage = function(event) {
  var worker = new Worker("nested_access_subworker.js");
  worker.onmessage = function(event) {
    var result = event.data;
    if(result == undefined) {
      postMessage(undefined);
    }
    else {
      if (result.type == "ok") {
        ok(result.condition, result.name, result.diag);
      }
      else if (result.type == "todo") {
        todo(result.a, result.b, result.name);
      }
    }
  };
  worker.postMessage("foo");
};
