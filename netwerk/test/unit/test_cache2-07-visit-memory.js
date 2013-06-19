function run_test()
{
  do_get_profile();

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
              finish_cache2_test();
            }),
          true);
        }),
      true);
    })
  );

  do_test_pending();
}
