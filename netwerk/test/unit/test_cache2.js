const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

var callbacks = new Array();

const NORMAL =           0;
const NEW =         1 << 0;
const NOTVALID =    1 << 1;
const THROWAVAIL =  1 << 2;
const READONLY =    1 << 3;
const NOTFOUND =    1 << 4;
const REVAL =       1 << 5;

var log = true;
function LOG(o, m)
{
  if (!log) return;
  if (!m)
    dump("TEST-INFO | CACHE2: " + o + "\n");
  else
    dump("TEST-INFO | CACHE2: callback #" + o.order + "(" + (o.workingData || "---") + ") " + m + "\n"); 
}

function exitPB()
{
  var obsvc = Cc["@mozilla.org/observer-service;1"].
    getService(Ci.nsIObserverService);
  obsvc.notifyObservers(null, "last-pb-context-exited", null);
}

function pumpReadStream(inputStream, goon)
{
  var pump = Cc["@mozilla.org/network/input-stream-pump;1"]
             .createInstance(Ci.nsIInputStreamPump);
  pump.init(inputStream, -1, -1, 0, 0, true);
  var data = "";
  pump.asyncRead({
    onStartRequest: function (aRequest, aContext) { },
    onDataAvailable: function (aRequest, aContext, aInputStream, aOffset, aCount)
    {
      var wrapper = Cc["@mozilla.org/scriptableinputstream;1"].
                    createInstance(Ci.nsIScriptableInputStream);
      wrapper.init(aInputStream);
      var str = wrapper.read(wrapper.available());
      LOG("reading data '" + str + "'");
      data += str;
    },
    onStopRequest: function (aRequest, aContext, aStatusCode)
    {
      LOG("done reading data: " + aStatusCode);
      do_check_eq(aStatusCode, Cr.NS_OK);
      goon(data);
    },
  }, null);
}

OpenCallback.prototype =
{
  QueryInterface: function listener_qi(iid) {
    if (iid.equals(Ci.nsISupports) ||
        iid.equals(Ci.nsICacheEntryOpenCallback)) {
      return this;
    }
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },
  onCacheEntryCheck: function(entry, appCache)
  {
    LOG(this, "onCacheEntryCheck");
    do_check_true(!this.onCheckPassed);
    this.onCheckPassed = true;
     
    if (this.behavior & NOTVALID) {
      LOG(this, "onCacheEntryCheck DONE, return ENTRY_NOT_VALID");
      return Ci.nsICacheEntryOpenCallback.ENTRY_NOT_VALID;
    }

    do_check_eq(entry.getMetaDataElement("meto"), this.workingMetadata);

    if (this.behavior & REVAL) {
      LOG(this, "onCacheEntryCheck DONE, return REVAL");
      return Ci.nsICacheEntryOpenCallback.ENTRY_NEEDS_REVALIDATION;
    }

    LOG(this, "onCacheEntryCheck DONE, return ENTRY_VALID");
    return Ci.nsICacheEntryOpenCallback.ENTRY_VALID;
  },
  onCacheEntryAvailable: function(entry, isnew, appCache, status)
  {
    LOG(this, "onCacheEntryAvailable");
    do_check_true(!this.onAvailPassed);
    this.onAvailPassed = true;

    do_check_eq(isnew, !!(this.behavior & NEW));

    if (this.behavior & NOTFOUND) {
      do_check_eq(status, Cr.NS_ERROR_CACHE_KEY_NOT_FOUND);
      do_check_false(!!entry);
      if (this.behavior & THROWAVAIL)
        this.throwAndNotify(entry);
      this.goon(entry);
    }
    else if (this.behavior & NEW) {
      do_check_true(!!entry);
      if (this.behavior & THROWAVAIL)
        this.throwAndNotify(entry);

      this.goon(entry);
      
      try {
        entry.getMetaDataElement("meto");
        do_check_true(false);
      }
      catch (ex) {}

      var self = this;
      do_execute_soon(function() { // emulate network latency
        entry.setMetaDataElement("meto", self.workingMetadata);
        entry.setValid();
        do_execute_soon(function() { // emulate more network latency
          var os = entry.openOutputStream(0);
          var wrt = os.write(self.workingData, self.workingData.length);
          do_check_eq(wrt, self.workingData.length);
          os.close();
        })
      })
    }
    else /* NORMAL */ {
      do_check_true(!!entry);
      do_check_eq(entry.getMetaDataElement("meto"), this.workingMetadata);
      if (this.behavior & THROWAVAIL)
        this.throwAndNotify(entry);

      var wrapper = Cc["@mozilla.org/scriptableinputstream;1"].
                    createInstance(Ci.nsIScriptableInputStream);
      var self = this;
      pumpReadStream(entry.openInputStream(0), function(data) {
        do_check_eq(data, self.workingData);
        self.onDataCheckPassed = true;
        LOG(self, "entry read done");
        self.goon(entry);
      });
    }
  },
  selfCheck: function()
  {
    LOG(this, "selfCheck");

    do_check_true(this.onCheckPassed);
    do_check_true(this.onAvailPassed);
    do_check_true(this.onDataCheckPassed);
  },
  throwAndNotify: function(entry)
  {
    LOG(this, "Throwing");
    var self = this;
    do_execute_soon(function() {
      LOG(self, "Notifying");
      self.goon(entry);
    });
    throw Cr.NS_ERROR_FAILURE;
  }
};

function OpenCallback(behavior, workingMetadata, workingData, goon)
{
  this.behavior = behavior;
  this.workingMetadata = workingMetadata;
  this.workingData = workingData;
  this.goon = goon;
  this.onCheckPassed = (!!(behavior & NEW) || !workingMetadata) && !(behavior & NOTVALID);
  this.onAvailPassed = false;
  this.onDataCheckPassed = !!(behavior & NEW) || !workingMetadata;
  callbacks.push(this);
  this.order = callbacks.length;
}

VisitCallback.prototype =
{
  QueryInterface: function listener_qi(iid) {
    if (iid.equals(Ci.nsISupports) ||
        iid.equals(Ci.nsICacheStorageVisitor)) {
      return this;
    }
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },
  onCacheStorageInfo: function(num, consumption)
  {
    LOG(this, "onCacheStorageInfo: num=" + num + ", size=" + consumption);
    do_check_eq(this.num, num);
    do_check_eq(this.consumption, consumption);

    if (!this.entries || num == 0)
      this.notify();
  },
  onCacheEntryInfo: function(entry)
  {
    var key = entry.key;
    LOG(this, "onCacheEntryInfo: key=" + key);

    do_check_true(!!this.entries);

    var index = this.entries.indexOf(key);
    do_check_true(index > -1);

    this.entries.splice(index, 1);

    if (this.entries.length == 0) {
      this.entries = null;
      this.notify();
    }
  },
  notify: function()
  {
    do_check_true(!!this.goon);
    var goon = this.goon;
    this.goon = null;
    goon();
  },
  selfCheck: function()
  {
    do_check_true(!this.entries || !this.entries.length);
  }
};

function VisitCallback(num, consumption, entries, goon)
{
  this.num = num;
  this.consumption = consumption;
  this.entries = entries;
  this.goon = goon;
  callbacks.push(this);
  this.order = callbacks.length;
}

EvictionCallback.prototype =
{
  QueryInterface: function listener_qi(iid) {
    if (iid.equals(Ci.nsISupports) ||
        iid.equals(Ci.nsICacheEntryDoomCallback)) {
      return this;
    }
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },
  onCacheEntryDoomed: function(result)
  {
    do_check_eq(this.expectedSuccess, result == Cr.NS_OK);
    this.goon();
  },
  selfCheck: function() {}
}

function EvictionCallback(success, goon)
{
  this.expectedSuccess = success;
  this.goon = goon;
  callbacks.push(this);
  this.order = callbacks.length;
}

MultipleCallbacks.prototype =
{
  fired: function()
  {
    if (--this.pending == 0)
      this.goon();
  }
}

function MultipleCallbacks(number, goon)
{
  this.pending = number;
  this.goon = goon;
}

// ---------------------------------------------------------------------
// ------------------------------- TESTS -------------------------------
// ---------------------------------------------------------------------

function run_test_basic()
{
  // Open for write, write
  asyncOpenCacheEntry("http://a/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
    new OpenCallback(NEW, "a1m", "a1d", function(entry) {
      // Open for read and check
      asyncOpenCacheEntry("http://a/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
        new OpenCallback(NORMAL, "a1m", "a1d", function(entry) {
          // Open for rewrite (truncate), write different meta and data
          asyncOpenCacheEntry("http://a/", "disk", Ci.nsICacheStorage.OPEN_TRUNCATE, null,
            new OpenCallback(NEW, "a2m", "a2d", function(entry) {
              // Open for read and check
              asyncOpenCacheEntry("http://a/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
                new OpenCallback(NORMAL, "a2m", "a2d", function(entry) {
                  run_test_open_non_existent();
                  //finish_test();
                })
              );
            })
          );
        })
      );
    })
  );
}

function run_test_open_non_existent()
{
  // Open non-existing for read, should fail
  asyncOpenCacheEntry("http://b/", "disk", Ci.nsICacheStorage.OPEN_READONLY, null,
    new OpenCallback(NOTFOUND, null, null, function(entry) {
      // Open the same non-existing for read again, should fail second time
      asyncOpenCacheEntry("http://b/", "disk", Ci.nsICacheStorage.OPEN_READONLY, null,
        new OpenCallback(NOTFOUND, null, null, function(entry) {
          // Try it again normally, should go
          asyncOpenCacheEntry("http://b/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
            new OpenCallback(NEW, "b1m", "b1d", function(entry) {
              // ...and check
              asyncOpenCacheEntry("http://b/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
                new OpenCallback(NORMAL, "b1m", "b1d", function(entry) {
                  run_test_OCEA_throws();
                  //finish_test();
                })
              );
            })
          );
        })
      );
    })
  );
}

function run_test_OCEA_throws()
{
  // Open but let OCEA throw
  asyncOpenCacheEntry("http://c/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
    new OpenCallback(NEW|THROWAVAIL, null, null, function(entry) {
      // Try it again, should go
      asyncOpenCacheEntry("http://c/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
        new OpenCallback(NEW, "c1m", "c1d", function(entry) {
          // ...and check
          asyncOpenCacheEntry("http://c/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
            new OpenCallback(false, "c1m", "c1d", function(entry) {
              run_test_OCEA_throws2x();
              //finish_test();
            })
          );
        })
      );
    })
  );
}

function run_test_OCEA_throws2x()
{
  // Open but let OCEA throw
  asyncOpenCacheEntry("http://d/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
    new OpenCallback(NEW|THROWAVAIL, null, null, function(entry) {
      // Open but let OCEA throw ones again
      asyncOpenCacheEntry("http://d/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
        new OpenCallback(NEW|THROWAVAIL, null, null, function(entry) {
          // Try it again, should go
          asyncOpenCacheEntry("http://d/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
            new OpenCallback(NEW, "d1m", "d1d", function(entry) {
              // ...and check
              asyncOpenCacheEntry("http://d/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
                new OpenCallback(NORMAL, "d1m", "d1d", function(entry) {
                  run_test_visit_basic();
                  //finish_test();
                })
              );
            })
          );
        })
      );
    })
  );
}

function run_test_visit_basic()
{
  var storage = getCacheStorage("disk");
  storage.asyncVisitStorage(
    // Previous tests should store 4 entries
    new VisitCallback(4, 40, ["http://a/", "http://b/", "http://c/", "http://d/"], function() {
      storage.asyncVisitStorage(
        // Previous tests should store 4 entries, now don't walk them
        new VisitCallback(4, 40, null, function() {
          LOG("*** run_test_visit_basic done");
          run_test_PB_mode();
          //finish_test();
        }),
      false);
    }),
  true);
}

function run_test_PB_mode()
{
  // Store PB entry
  asyncOpenCacheEntry("http://p1/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, new LoadContextInfo(PRIVATE),
    new OpenCallback(NEW, "p1m", "p1d", function(entry) {
      // Check it's there
      var storage = getCacheStorage("disk", new LoadContextInfo(PRIVATE));
      storage.asyncVisitStorage(
        new VisitCallback(1, 10, ["http://p1/"], function() {
          // Simulate PB exit
          exitPB();
          // Check the entry is gone
          storage.asyncVisitStorage(
            new VisitCallback(0, 0, null, function() {
              LOG("*** run_test_PB_mode done");
              run_test_memory_storage_visit();
              //finish_test();
            }),
          true);
        }),
      true);
    })
  );
}

function run_test_memory_storage_visit()
{
  // Add entry to the memory storage
  asyncOpenCacheEntry("http://mem1/", "memory", Ci.nsICacheStorage.OPEN_NORMALLY, null,
    new OpenCallback(NEW, "m1m", "m1d", function(entry) {
      // Check it's there by visiting the storage
      var storage = getCacheStorage("memory");
      storage.asyncVisitStorage(
        new VisitCallback(1, 10, ["http://mem1/"], function() {
          storage = getCacheStorage("disk");
          storage.asyncVisitStorage(
            // Previous tests should store 4 disk entries + 1 memory entry
            new VisitCallback(5, 50, ["http://a/", "http://b/", "http://c/", "http://d/", "http://mem1/"], function() {
              run_test_evict_existing_disk_entry_using_memory_storage();
              //finish_test();
            }),
          true);
        }),
      true);
    })
  );
}

function run_test_evict_existing_disk_entry_using_memory_storage()
{
  var storage = getCacheStorage("memory");
  storage.asyncDoomURI(createURI("http://a/"), "",
    new EvictionCallback(false, function() {
      run_test_evict_single_entry();
      //finish_test();
    })
  );

}

function run_test_evict_single_entry()
{
  var storage = getCacheStorage("disk");
  storage.asyncDoomURI(createURI("http://a/"), "",
    new EvictionCallback(true, function() {
      run_test_evict_single_entry2();
      //finish_test();
    })
  );
}

function run_test_evict_single_entry2()
{
  asyncOpenCacheEntry("http://b/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
    new OpenCallback(NORMAL, "b1m", "b1d", function(entry) {
      entry.asyncDoom(
        new EvictionCallback(true, function() {
          run_test_evict_memory_storage();        
          //finish_test();
        })
      );
    })
  );
}

function run_test_evict_memory_storage()
{
  var storage = getCacheStorage("memory");
  storage.asyncEvictStorage(
    new EvictionCallback(true, function() {
      asyncOpenCacheEntry("http://mem1/", "memory", Ci.nsICacheStorage.OPEN_NORMALLY, null,
        new OpenCallback(NEW, "m2m", "m2d", function(entry) {
          storage.asyncEvictStorage(
            new EvictionCallback(true, function() {
              storage.asyncVisitStorage(
                new VisitCallback(0, 0, null, function() {
                  var storage = getCacheStorage("disk");
                  storage.asyncVisitStorage(
                    new VisitCallback(2, 20, ["http://c/", "http://d/"], function() {
                      run_test_evict_storage();
                      //finish_test();
                    }),
                  true);
                }),
              true);
            })
          );
        })
      );
    })
  );
}

function run_test_evict_storage()
{
  var storage = getCacheStorage("disk");
  storage.asyncEvictStorage(
    new EvictionCallback(true, function() {
      storage.asyncVisitStorage(
        new VisitCallback(0, 0, null, function() {
          var storage = getCacheStorage("memory");
          storage.asyncVisitStorage(
            new VisitCallback(0, 0, null, function() {
              run_test_dont_validate(); 
            }),
          true);
        }),
      true);
    })
  );
}

function run_test_dont_validate()
{
  // Open for write, write
  asyncOpenCacheEntry("http://v/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null, 
    new OpenCallback(NEW, "v1m", "v1d", function(entry) {
      // Open for rewrite (don't validate), write different meta and data
      asyncOpenCacheEntry("http://v/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
        new OpenCallback(NOTVALID|NEW, "v2m", "v2d", function(entry) {
          // And check...
          asyncOpenCacheEntry("http://v/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null, 
            new OpenCallback(NORMAL, "v2m", "v2d", function(entry) {
              //run_test_basic_concurent_readers();
              run_test_evict_non_existing();
            })
          );
        })
      );
    })
  );
}

function run_test_basic_concurent_readers()
{
  asyncOpenCacheEntry("http://x/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
    new OpenCallback(NEW, "x1m", "x1d", function(entry) {
      // nothing to do here, we expect concurent callbacks to get
      // all notified, then the whole test chain continues
    })
  );
  
  var mc = new MultipleCallbacks(3, run_test_needs_reval_304);
  
  asyncOpenCacheEntry("http://x/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
    new OpenCallback(NORMAL, "x1m", "x1d", function(entry) {
      mc.fired();
    })
  );
  asyncOpenCacheEntry("http://x/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
    new OpenCallback(NORMAL, "x1m", "x1d", function(entry) {
      mc.fired();
    })
  );
  asyncOpenCacheEntry("http://x/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
    new OpenCallback(NORMAL, "x1m", "x1d", function(entry) {
      mc.fired();
    })
  );
}

function run_test_needs_reval_304()
{
  // Open for write, write
  asyncOpenCacheEntry("http://304/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null, 
    new OpenCallback(NEW, "31m", "31d", function(entry) {
      // Open normally but wait for validation from the server
      asyncOpenCacheEntry("http://304/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
        new OpenCallback(REVAL, "31m", "31d", function(entry) {
          // emulate 304 from the server
          do_execute_soon(function() {
            entry.setValid(); // this will trigger OpenCallbacks bellow
          });
        })
      );
      
      var mc = new MultipleCallbacks(3, run_test_needs_reval_200);
      
      asyncOpenCacheEntry("http://304/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
        new OpenCallback(NORMAL, "31m", "31d", function(entry) {
          mc.fired();
        })
      );
      asyncOpenCacheEntry("http://304/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
        new OpenCallback(NORMAL, "31m", "31d", function(entry) {
          mc.fired();
        })
      );
      asyncOpenCacheEntry("http://304/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
        new OpenCallback(NORMAL, "31m", "31d", function(entry) {
          mc.fired();
        })
      );
    })
  );
}

function run_test_needs_reval_200()
{
  // Open for write, write
  asyncOpenCacheEntry("http://200/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null, 
    new OpenCallback(NEW, "21m", "21d", function(entry) {
      // Open normally but wait for validation from the server
      asyncOpenCacheEntry("http://200/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
        new OpenCallback(REVAL, "21m", "21d", function(entry) {
          // emulate 200 from server (new content)
          do_execute_soon(function() {
            var entry2 = entry.recreate(); 
            
            // now fill the new entry, use OpenCallback directly for it
            var callback2 = new OpenCallback(NEW, "22m", "22d", function() {});
            callback2.onCacheEntryAvailable(entry2, true, null, Cr.NS_OK);
          });
        })
      );

      var mc = new MultipleCallbacks(3, run_test_evict_non_existing);

      asyncOpenCacheEntry("http://200/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
        new OpenCallback(NORMAL, "22m", "22d", function(entry) {
          mc.fired();
        })
      );
      asyncOpenCacheEntry("http://200/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
        new OpenCallback(NORMAL, "22m", "22d", function(entry) {
          mc.fired();
        })
      );
      asyncOpenCacheEntry("http://200/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
        new OpenCallback(NORMAL, "22m", "22d", function(entry) {
          mc.fired();
        })
      );
    })
  );
}

function run_test_evict_non_existing()
{
  var storage = getCacheStorage("disk");
  storage.asyncDoomURI(createURI("http://non-existing/"), "",
    new EvictionCallback(false, function() {
      run_test_evict_all();
    })
  );
}

function run_test_evict_all()
{
  var svc = get_cache_service();
  svc.clear();

  var storage = getCacheStorage("disk");
  storage.asyncVisitStorage(
    new VisitCallback(0, 0, null, function() {
      finish_test();
    }),
  true);
}

function run_test()
{
  do_get_profile();
  run_test_basic();
  // run_test_basic_concurent_readers();
  do_test_pending();
}

function finish_test()
{
  callbacks.forEach(function(callback, index) {
    callback.selfCheck();
  });
  do_test_finished();
}
