function run_test()
{
  do_get_profile();

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
                      finish_cache2_test();
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

  do_test_pending();
}
