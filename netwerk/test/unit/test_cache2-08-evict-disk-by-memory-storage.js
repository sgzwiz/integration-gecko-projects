function run_test()
{
  do_get_profile();

  var storage = getCacheStorage("memory");
  storage.asyncDoomURI(createURI("http://a/"), "",
    new EvictionCallback(false, function() {
      finish_cache2_test();
    })
  );

  do_test_pending();
}
