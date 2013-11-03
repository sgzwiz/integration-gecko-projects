/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function repr(val) {
  return typeof val == "string" ? '"' + val + '"' : val;
}

function ok(condition, name, diag) {
  var rtnObj = new Object();
  rtnObj.type = "ok";
  rtnObj.condition = !!condition;
  rtnObj.name = name;
  rtnObj.diag = diag;
  postMessage(rtnObj);
}

function is(a, b, name) {
  var pass = (a == b);
  var diag = pass ? "" : "got " + repr(a) + ", expected " + repr(b);
  ok(pass, name, diag);
}

function isnot(a, b, name) {
  var pass = (a != b);
  var diag = pass ? "" : "didn't expect " + repr(a) + ", but got it";
  ok(pass, name, diag);
}

function todo(condition, name, diag) {
  var rtnObj = new Object();
  rtnObj.type = "todo";
  rtnObj.condition = !!condition;
  rtnObj.name = name;
  rtnObj.diag = diag;
  postMessage(rtnObj);
}

function todo_is(a, b, name) {
  var pass = (a == b);
  var diag = pass ? repr(a) + " should equal " + repr(b)
                  : "got " + repr(a) + ", expected " + repr(b);
  todo(pass, name, diag);
}

function info(name) {
  var rtnObj = new Object();
  rtnObj.type = "info";
  rtnObj.name = name;
  postMessage(rtnObj);
}

function unexpectedEventHandler()
{
  ok(false, "Got event, but did not expect it!");
  postMessage(undefined);
}

function unexpectedCallback()
{
  ok(false, "Callback called, but did not expect it!");
  postMessage(undefined);
}

function expectException(callback, error, errorCode) {
  errorCode = typeof errorCode !== 'undefined' ? errorCode : 0;
  try {
    callback();
    ok(false, "Should have thrown " + error + "!");
  }
  catch (ex) {
    ok(ex instanceof DOMException, "Got a DOMException");
    is(ex.name, error, "Correct exception name: " + error);
    is(ex.code, errorCode, "Correct exception code: " + errorCode);
  }
}

function compareKeys(k1, k2) {
  var t = typeof k1;
  if (t != typeof k2)
    return false;

  if (t !== "object")
    return k1 === k2;

  if (k1 instanceof Date) {
    return (k2 instanceof Date) &&
      k1.getTime() === k2.getTime();
  }

  if (k1 instanceof Array) {
    if (!(k2 instanceof Array) ||
        k1.length != k2.length)
      return false;

    for (var i = 0; i < k1.length; ++i) {
      if (!compareKeys(k1[i], k2[i]))
        return false;
    }

    return true;
  }

  return false;
}
