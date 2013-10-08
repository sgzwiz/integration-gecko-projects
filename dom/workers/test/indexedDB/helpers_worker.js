/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function ok(condition, name, diag) {
  var rtnObj = new Object();
  rtnObj.type = "ok";
  rtnObj.condition = !!condition;
  rtnObj.name = name;
  rtnObj.diag = diag;
  postMessage(rtnObj);
}

function is(a, b, name) {
  var rtnObj = new Object();
  rtnObj.type = "is";
  rtnObj.a = a;
  rtnObj.b = b;
  rtnObj.name = name;
  postMessage(rtnObj);
}

function isnot(a, b, name) {
  var rtnObj = new Object();
  rtnObj.type = "isnot";
  rtnObj.a = a;
  rtnObj.b = b;
  rtnObj.name = name;
  postMessage(rtnObj);
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
  var rtnObj = new Object();
  rtnObj.type = "todo_is";
  rtnObj.a = a;
  rtnObj.b = b;
  rtnObj.name = name;
  postMessage(rtnObj);
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
