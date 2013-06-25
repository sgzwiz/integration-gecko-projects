function run_test()
{
  do_get_profile();

  // Open for write, write
  asyncOpenCacheEntry("http://v/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
    new OpenCallback(NEW, "v1m", "v1d", function(entry) {
      // Open for rewrite (don't validate), write different meta and data
      asyncOpenCacheEntry("http://v/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
        new OpenCallback(NOTVALID|RECREATE, "v2m", "v2d", function(entry) {
          // And check...
          asyncOpenCacheEntry("http://v/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
            new OpenCallback(NORMAL, "v2m", "v2d", function(entry) {
              finish_cache2_test();
            })
          );
        })
      );
    })
  );

  do_test_pending();
}
