/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

if (!SpecialPowers.isMainProcess()) {
  window.runTest = function() {
    todo(false, "Test disabled in child processes, for now");
    SimpleTest.finish();
  }
}
