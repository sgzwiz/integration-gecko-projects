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

var log_c2 = true;
function LOG_C2(o, m)
{
  if (!log_c2) return;
  if (!m)
    dump("TEST-INFO | CACHE2: " + o + "\n");
  else
    dump("TEST-INFO | CACHE2: callback #" + o.order + "(" + (o.workingData || "---") + ") " + m + "\n");
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
      LOG_C2("reading data '" + str + "'");
      data += str;
    },
    onStopRequest: function (aRequest, aContext, aStatusCode)
    {
      LOG_C2("done reading data: " + aStatusCode);
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
    LOG_C2(this, "onCacheEntryCheck");
    do_check_true(!this.onCheckPassed);
    this.onCheckPassed = true;

    if (this.behavior & NOTVALID) {
      LOG_C2(this, "onCacheEntryCheck DONE, return ENTRY_NOT_VALID");
      return Ci.nsICacheEntryOpenCallback.ENTRY_NOT_VALID;
    }

    do_check_eq(entry.getMetaDataElement("meto"), this.workingMetadata);

    if (this.behavior & REVAL) {
      LOG_C2(this, "onCacheEntryCheck DONE, return REVAL");
      return Ci.nsICacheEntryOpenCallback.ENTRY_NEEDS_REVALIDATION;
    }

    LOG_C2(this, "onCacheEntryCheck DONE, return ENTRY_VALID");
    return Ci.nsICacheEntryOpenCallback.ENTRY_VALID;
  },
  onCacheEntryAvailable: function(entry, isnew, appCache, status)
  {
    LOG_C2(this, "onCacheEntryAvailable");
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
        LOG_C2(self, "entry read done");
        self.goon(entry);
      });
    }
  },
  selfCheck: function()
  {
    LOG_C2(this, "selfCheck");

    do_check_true(this.onCheckPassed);
    do_check_true(this.onAvailPassed);
    do_check_true(this.onDataCheckPassed);
  },
  throwAndNotify: function(entry)
  {
    LOG_C2(this, "Throwing");
    var self = this;
    do_execute_soon(function() {
      LOG_C2(self, "Notifying");
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
    LOG_C2(this, "onCacheStorageInfo: num=" + num + ", size=" + consumption);
    do_check_eq(this.num, num);
    do_check_eq(this.consumption, consumption);

    if (!this.entries || num == 0)
      this.notify();
  },
  onCacheEntryInfo: function(entry)
  {
    var key = entry.key;
    LOG_C2(this, "onCacheEntryInfo: key=" + key);

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

function finish_cache2_test()
{
  callbacks.forEach(function(callback, index) {
    callback.selfCheck();
  });
  do_test_finished();
}
