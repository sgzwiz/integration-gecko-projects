/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
this.EXPORTED_SYMBOLS = ["OfflineAppCacheHelper"];

const Cc = Components.classes;
const Ci = Components.interfaces;

this.OfflineAppCacheHelper = {
  clear: function() {
    // TODO mayhemer: migrate to new cache api when implemete for appcache
    var cacheService = Cc["@mozilla.org/network/cache-service;1"].
                       getService(Ci.nsICacheService);
    try {
      cacheService.evictEntries(Ci.nsICache.STORE_OFFLINE);
    } catch(er) {}
  }
};
