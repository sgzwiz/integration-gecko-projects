function run_test()
{
  do_get_profile();

  var storage = getCacheStorage("disk");
  storage.asyncDoomURI(createURI("http://a/"), "",
    new EvictionCallback(true, function() {
      finish_cache2_test();
    })
  );

  do_test_pending();
}
