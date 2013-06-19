function run_test()
{
  do_get_profile();

  var storage = getCacheStorage("disk");
  storage.asyncEvictStorage(
    new EvictionCallback(true, function() {
      storage.asyncVisitStorage(
        new VisitCallback(0, 0, null, function() {
          var storage = getCacheStorage("memory");
          storage.asyncVisitStorage(
            new VisitCallback(0, 0, null, function() {
              finish_cache2_test();
            }),
          true);
        }),
      true);
    })
  );

  do_test_pending();
}
