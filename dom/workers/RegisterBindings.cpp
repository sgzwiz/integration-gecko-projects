/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Workers.h"

#include "jsapi.h"
#include "js/OldDebugAPI.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/DOMExceptionBinding.h"
#include "mozilla/dom/DOMStringListBinding.h"
#include "mozilla/dom/EventBinding.h"
#include "mozilla/dom/EventHandlerBinding.h"
#include "mozilla/dom/EventTargetBinding.h"
#include "mozilla/dom/FileReaderSyncBinding.h"
#include "mozilla/dom/IDBCursorSyncBinding.h"
#include "mozilla/dom/IDBDatabaseSyncBinding.h"
#include "mozilla/dom/IDBFactorySyncBinding.h"
#include "mozilla/dom/IDBIndexSyncBinding.h"
#include "mozilla/dom/IDBKeyRangeBinding.h"
#include "mozilla/dom/IDBObjectStoreSyncBinding.h"
#include "mozilla/dom/IDBVersionChangeEventBinding.h"
#include "mozilla/dom/IDBTransactionSyncBinding.h"
#include "mozilla/dom/ImageData.h"
#include "mozilla/dom/ImageDataBinding.h"
#include "mozilla/dom/MessageEventBinding.h"
#include "mozilla/dom/MessagePortBinding.h"
#include "mozilla/dom/TextDecoderBinding.h"
#include "mozilla/dom/TextEncoderBinding.h"
#include "mozilla/dom/XMLHttpRequestBinding.h"
#include "mozilla/dom/XMLHttpRequestUploadBinding.h"
#include "mozilla/dom/URLBinding.h"
#include "mozilla/dom/WorkerBinding.h"
#include "mozilla/dom/WorkerLocationBinding.h"
#include "mozilla/dom/WorkerNavigatorBinding.h"
#include "mozilla/OSFileConstants.h"

#include "ChromeWorkerScope.h"
#include "File.h"
#include "IDBFactorySync.h"
#include "WorkerPrivate.h"

#define IDBSYNC_STR "indexedDBSync"

USING_WORKERS_NAMESPACE
using namespace mozilla::dom;

namespace {

bool
IndexedDBSyncLazyGetter(JSContext* aCx, JS::Handle<JSObject*> aObj,
                        JS::Handle<jsid> aId, JS::MutableHandle<JS::Value> aVp)
{
  NS_ASSERTION(JS_IsGlobalObject(aObj), "Not a global object!");
  NS_ASSERTION(JSID_IS_STRING(aId), "Bad id!");
  NS_ASSERTION(JS_FlatStringEqualsAscii(JSID_TO_FLAT_STRING(aId), IDBSYNC_STR),
               "Bad id!");

  IDBFactorySync* indexedDBSync = IDBFactorySync::Create(aCx, aObj);
  if (!indexedDBSync) {
    return false;
  }

  JS::Rooted<JS::Value> wrappedIndexedDBSync(aCx);
  if (!WrapNewBindingObject(aCx, aObj, indexedDBSync, &wrappedIndexedDBSync)) {
    return false;
  }

  if (!JS_DeletePropertyById(aCx, aObj, aId) ||
      !JS_DefinePropertyById(aCx, aObj, aId, wrappedIndexedDBSync, NULL, NULL,
                             JSPROP_ENUMERATE)) {
    return false;
  }

  return JS_GetPropertyById(aCx, aObj, aId, aVp);
}

inline bool
DefineIndexedDBSyncLazyGetter(JSContext* aCx, JSObject* aGlobal)
{
  JSString* indexedDBSyncStr = JS_InternString(aCx, IDBSYNC_STR);
  if (!indexedDBSyncStr) {
    return false;
  }

  jsid indexedDBSyncId = INTERNED_STRING_TO_JSID(aCx, indexedDBSyncStr);

  if (!JS_DefinePropertyById(aCx, aGlobal, indexedDBSyncId, JSVAL_VOID,
                             IndexedDBSyncLazyGetter, NULL, 0)) {
    return false;
  }

  return true;
}

} // anonymous namespace

bool
WorkerPrivate::RegisterBindings(JSContext* aCx, JS::Handle<JSObject*> aGlobal)
{
  JS::Rooted<JSObject*> eventTargetProto(aCx,
    EventTargetBinding::GetProtoObject(aCx, aGlobal));
  if (!eventTargetProto) {
    return false;
  }

  if (IsChromeWorker()) {
    if (!ChromeWorkerBinding::GetConstructorObject(aCx, aGlobal) ||
        !DefineChromeWorkerFunctions(aCx, aGlobal) ||
        !DefineOSFileConstants(aCx, aGlobal)) {
      return false;
    }
  }

  // Init other classes we care about.
  if (!file::InitClasses(aCx, aGlobal)) {
    return false;
  }

  // Init other paris-bindings.
  if (!DOMExceptionBinding::GetConstructorObject(aCx, aGlobal) ||
      !DOMStringListBinding::GetConstructorObject(aCx, global) ||
      !EventBinding::GetConstructorObject(aCx, aGlobal) ||
      !FileReaderSyncBinding_workers::GetConstructorObject(aCx, aGlobal) ||
      !IDBCursorSyncBinding_workers::GetConstructorObject(aCx, global) ||
      !IDBCursorWithValueSyncBinding_workers::GetConstructorObject(aCx, global) ||
      !IDBDatabaseSyncBinding_workers::GetConstructorObject(aCx, global) ||
      !IDBFactorySyncBinding_workers::GetConstructorObject(aCx, global) ||
      !IDBIndexSyncBinding_workers::GetConstructorObject(aCx, global) ||
      !IDBKeyRangeBinding::GetConstructorObject(aCx, global) ||
      !IDBObjectStoreSyncBinding_workers::GetConstructorObject(aCx, global) ||
      !IDBVersionChangeEventBinding_workers::GetConstructorObject(aCx, global) ||
      !IDBTransactionSyncBinding_workers::GetConstructorObject(aCx, global) ||
      !ImageDataBinding::GetConstructorObject(aCx, aGlobal) ||
      !MessageEventBinding::GetConstructorObject(aCx, aGlobal) ||
      !MessagePortBinding::GetConstructorObject(aCx, aGlobal) ||
      !TextDecoderBinding::GetConstructorObject(aCx, aGlobal) ||
      !TextEncoderBinding::GetConstructorObject(aCx, aGlobal) ||
      !XMLHttpRequestBinding_workers::GetConstructorObject(aCx, aGlobal) ||
      !XMLHttpRequestUploadBinding_workers::GetConstructorObject(aCx, aGlobal) ||
      !URLBinding_workers::GetConstructorObject(aCx, aGlobal) ||
      !WorkerBinding::GetConstructorObject(aCx, aGlobal) ||
      !WorkerLocationBinding_workers::GetConstructorObject(aCx, aGlobal) ||
      !WorkerNavigatorBinding_workers::GetConstructorObject(aCx, aGlobal)) {
    return false;
  }

  if (!DefineIndexedDBSyncLazyGetter(aCx, global)) {
    return NULL;
  }

  if (!JS_DefineProfilingFunctions(aCx, aGlobal)) {
    return false;
  }

  return true;
}
