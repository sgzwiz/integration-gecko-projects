/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function runTest(path) {
  SimpleTest.waitForExplicitFinish();

  var worker = new Worker(path);
  worker.onmessage = function(event) {
    var result = event.data;
    if(result == undefined) {
      SimpleTest.finish();
    }
    else {
      if (result.type == "ok") {
        ok(result.condition, result.name, result.diag);
      }
      else if (result.type == "is") {
        is(result.a, result.b, result.name);
      }
      else if (result.type == "info") {        
        info(result.name);
      }
      else if (result.type == "isnot") {
        isnot(result.a, result.b, result.name);
      }
      else if (result.type == "todo") {
        todo(result.a, result.b, result.name);
      }
      else if (result.type == "todo_is") {
        todo_is(result.a, result.b, result.name);
      }
    }
  };
  worker.postMessage("foo");
}
