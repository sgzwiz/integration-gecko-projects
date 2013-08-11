/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Check about:cache after private browsing
// This test covers MozTrap test 6047
// bug 880621

let tmp = {};

Cc["@mozilla.org/moz/jssubscript-loader;1"]
  .getService(Ci.mozIJSSubScriptLoader)
  .loadSubScript("chrome://browser/content/sanitize.js", tmp);

let Sanitizer = tmp.Sanitizer;

function test() {

  waitForExplicitFinish();

  sanitizeCache();

  let nrEntriesR1 = getStorageEntryCount("regular", function(nrEntriesR1) {
    is (nrEntriesR1, 0, "Disk cache reports 0KB and has no entries");

    get_cache_for_private_window();
  });
}

function cleanup() {
  let prefs = Services.prefs.getBranch("privacy.cpd.");

  prefs.clearUserPref("history");
  prefs.clearUserPref("downloads");
  prefs.clearUserPref("cache");
  prefs.clearUserPref("cookies");
  prefs.clearUserPref("formdata");
  prefs.clearUserPref("offlineApps");
  prefs.clearUserPref("passwords");
  prefs.clearUserPref("sessions");
  prefs.clearUserPref("siteSettings");
}

function sanitizeCache() {

  let s = new Sanitizer();
  s.ignoreTimespan = false;
  s.prefDomain = "privacy.cpd.";

  let prefs = gPrefService.getBranch(s.prefDomain);
  prefs.setBoolPref("history", false);
  prefs.setBoolPref("downloads", false);
  prefs.setBoolPref("cache", true);
  prefs.setBoolPref("cookies", false);
  prefs.setBoolPref("formdata", false);
  prefs.setBoolPref("offlineApps", false);
  prefs.setBoolPref("passwords", false);
  prefs.setBoolPref("sessions", false);
  prefs.setBoolPref("siteSettings", false);

  s.sanitize();
}

function get_cache_service() {
  return Components.classes["@mozilla.org/netwerk/cache-storage-service;1"]
                   .getService(Components.interfaces.nsICacheStorageService);
}

function getStorageEntryCount(device, goon) {
  var cs = get_cache_service();
  
  function LoadContextInfo(isprivate, anonymous, appid, inbrowser)
  {
    this.isPrivate = isprivate || false;
    this.isAnonymous = anonymous || false;
    this.appId = appid || 0;
    this.isInBrowserElement = inbrowser || false;
  }

  LoadContextInfo.prototype = {
    QueryInterface: function(iid) {
      if (iid.equals(Ci.nsILoadContextInfo))
        return this;
      throw Cr.NS_ERROR_NO_INTERFACE;
    },
    isPrivate : false,
    isAnonymous : false,
    isInBrowserElement : false,
    appId : 0
  };

  var storage;
  switch (device) {
  case "anon":
    storage = cs.diskCacheStorage(new LoadContextInfo(true), false);
    break;
  case "regular":
    storage = cs.diskCacheStorage(new LoadContextInfo(false), false);
    break;
  default:
    throw "Unknown device " + device + " at getStorageEntryCount";
  }

  var visitor = {
    entryCount: 0,
    onCacheStorageInfo: function (aEntryCount, aConsumption) {
    },
    onCacheEntryInfo: function(entry)
    {
      dump(device + ":" + entry.key + "\n");
      if (entry.key.match(/^http:\/\/example.org\//))
        ++this.entryCount;
    },
    onCacheEntryVisitCompleted: function()
    {
      goon(this.entryCount);
    }
  };

  storage.asyncVisitStorage(visitor, true);
}

function get_cache_for_private_window () {
  let win = OpenBrowserWindow({private: true});
  win.addEventListener("load", function () {
    win.removeEventListener("load", arguments.callee, false);

    executeSoon(function() {

      ok(true, "The private window got loaded");

      let tab = win.gBrowser.addTab("http://example.org");
      win.gBrowser.selectedTab = tab;
      let newTabBrowser = win.gBrowser.getBrowserForTab(tab);

      newTabBrowser.addEventListener("load", function eventHandler() {
        newTabBrowser.removeEventListener("load", eventHandler, true);

        executeSoon(function() {

          getStorageEntryCount("anon", function(nrEntriesP) {
            ok (nrEntriesP >= 1, "Memory cache reports some entries from example.org domain");

            getStorageEntryCount("regular", function(nrEntriesR2) {
              is (nrEntriesR2, 0, "Disk cache reports 0KB and has no entries");

              cleanup();

              win.close();
              finish();
            });
          });
        });
      }, true);
    });
  }, false);
}
