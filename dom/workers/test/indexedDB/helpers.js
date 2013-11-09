/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Components = SpecialPowers.Components;

function clearAllDatabases() {
  if (!SpecialPowers.isMainProcess()) {
    return;
  }

  let comp = SpecialPowers.wrap(Components);

  let quotaManager =
    comp.classes["@mozilla.org/dom/quota/manager;1"]
        .getService(comp.interfaces.nsIQuotaManager);

  let uri = SpecialPowers.wrap(document).documentURIObject;

  quotaManager.clearStoragesForURI(uri);
}

if (!window.runTest) {
  window.runTest = function(path) {
    const indexedDBSyncPref = "dom.indexedDBSync.enabled";

    clearAllDatabases();

    SpecialPowers.pushPrefEnv({ set: [[indexedDBSyncPref, true]] }, function() {
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
          else if (result.type == "info") {
            info(result.name);
          }
          else if (result.type == "todo") {
            todo(result.a, result.b, result.name);
          }
        }
      };
      worker.postMessage("foo");
    });

    SimpleTest.waitForExplicitFinish();
  }
}
