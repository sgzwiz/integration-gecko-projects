Components.utils.import('resource://gre/modules/XPCOMUtils.jsm');

var _CSvc;
function get_cache_service() {
  if (_CSvc)
    return _CSvc;

  return _CSvc = Components.classes["@mozilla.org/netwerk/cache-storage-service;1"]
                           .getService(Components.interfaces.nsICacheStorageService);
}

const PRIVATE = true;

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

function evict_cache_entries(where)
{
  var clearDisk = (!where || where == "disk" || where == "all");
  var clearMem = (!where || where == "memory" || where == "all");
  var clearAppCache = (where == "appcache");

  var svc = get_cache_service();
  var storage;

  if (clearMem) {
    storage = svc.memoryCacheStorage(new LoadContextInfo());
    storage.asyncEvictStorage();
  }

  if (clearDisk) {
    storage = svc.diskCacheStorage(new LoadContextInfo(), false);
    storage.asyncEvictStorage();
  }

  if (clearAppCache) {
    storage = svc.appCacheStorage(new LoadContextInfo(), null);
    storage.asyncEvictStorage();
  }
}

function createURI(urispec)
{
  var ioServ = Components.classes["@mozilla.org/network/io-service;1"]
                         .getService(Components.interfaces.nsIIOService);
  return ioServ.newURI(urispec, null, null);
}

function getCacheStorage(where, lci, appcache)
{
  if (!lci) lci = new LoadContextInfo();
  var svc = get_cache_service();
  switch (where) {
    case "disk": return svc.diskCacheStorage(lci, false);
    case "memory": return svc.memoryCacheStorage(lci);
    case "appcache": return svc.appCacheStorage(lci, appcache);
  }
  return null;
}

function asyncOpenCacheEntry(key, where, flags, lci, callback, appcache)
{
  key = createURI(key);

  function CacheListener() { }
  CacheListener.prototype = {
    QueryInterface: function (iid) {
      if (iid.equals(Components.interfaces.nsICacheEntryOpenCallback) ||
          iid.equals(Components.interfaces.nsISupports))
        return this;
      throw Components.results.NS_ERROR_NO_INTERFACE;
    },

    onCacheEntryCheck: function(entry, appCache) {
      if (typeof callback === "object")
        return callback.onCacheEntryCheck(entry, appCache);
    },

    onCacheEntryAvailable: function (entry, isnew, appCache, status) {
      if (typeof callback === "object")
        callback.onCacheEntryAvailable(entry, isnew, appCache, status);
      else
        callback(status, entry, appCache);
    },

    run: function () {
      var storage = getCacheStorage(where, lci, appcache);
      storage.asyncOpenURI(key, "", flags, this);
    }
  };

  (new CacheListener()).run();
}

// mayhemer: still needed?
function syncWithCacheIOThread(callback)
{
  asyncOpenCacheEntry("http://nonexistent/entry", "disk", Ci.nsICacheStorage.OPEN_READONLY,
    function(status, entry) {
      do_check_eq(status, Components.results.NS_ERROR_CACHE_KEY_NOT_FOUND);
      callback();
    });
}

// TODO - this has to be async...
function get_device_entry_count(where, continuation) {
  var storage = getCacheStorage(where);
  if (!storage) {
    continuation(-1, 0);
    return;
  }

  var visitor = {
    onCacheStorageInfo: function (entryCount, consumption) {
      continuation(entryCount, consumption);
    },
  };

  // get the device entry count
  storage.asyncVisitStorage(visitor, true);
}

function asyncCheckCacheEntryPresence(key, where, shouldExist, continuation)
{
  asyncOpenCacheEntry(key, where, Ci.nsICacheStorage.OPEN_READONLY,
    function(status, entry) {
      if (shouldExist) {
        dump("TEST-INFO | checking cache key " + key + " exists @ " + where);
        do_check_eq(status, Cr.NS_OK);
        do_check_true(!!entry);
      } else {
        dump("TEST-INFO | checking cache key " + key + " doesn't exist @ " + where);
        do_check_eq(status, Cr.NS_ERROR_CACHE_KEY_NOT_FOUND);
        do_check_null(entry);
      }
      continuation();
    });
}
