function run_test()
{
  do_get_profile();

  var storage = getCacheStorage("disk");
  var mc = new MultipleCallbacks(4, function() {
    storage.asyncVisitStorage(
      // Previous tests should store 4 entries
      new VisitCallback(4, 40, ["http://a/", "http://b/", "http://c/", "http://d/"], function() {
        storage.asyncVisitStorage(
          // Previous tests should store 4 entries, now don't walk them
          new VisitCallback(4, 40, null, function() {
            finish_cache2_test();
          }),
        false);
      }),
    true);
  });

  asyncOpenCacheEntry("http://a/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
    new OpenCallback(NEW, "a1m", "a1d", function(entry) {
      mc.fired();
    })
  );

  asyncOpenCacheEntry("http://b/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
    new OpenCallback(NEW, "b1m", "b1d", function(entry) {
      mc.fired();
    })
  );

  asyncOpenCacheEntry("http://c/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
    new OpenCallback(NEW, "c1m", "c1d", function(entry) {
      mc.fired();
    })
  );

  asyncOpenCacheEntry("http://d/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
    new OpenCallback(NEW, "d1m", "d1d", function(entry) {
      mc.fired();
    })
  );

  do_test_pending();
}
