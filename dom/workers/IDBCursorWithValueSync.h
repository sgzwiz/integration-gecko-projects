/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_idbcursorwithvaluesync_h__
#define mozilla_dom_workers_idbcursorwithvaluesync_h__

#include "Workers.h"

#include "mozilla/dom/BindingUtils.h"

#include "IDBCursorSync.h"

BEGIN_WORKERS_NAMESPACE

class OpenHelperWithValue;

class IDBCursorWithValueSync MOZ_FINAL : public IDBCursorSync
{
  friend class IDBIndexSync;
  friend class IDBObjectStoreSync;
  friend class OpenHelperWithValue;

public:
  NS_DECL_ISUPPORTS_INHERITED

  // For INDEXOBJECT cursors.
  static IDBCursorWithValueSync*
  Create(JSContext* aCx, IDBIndexSync* aIndex, Direction aDirection);

  // For OBJECTSTORE cursors.
  static IDBCursorWithValueSync*
  Create(JSContext* aCx, IDBObjectStoreSync* aObjectStore,
         Direction aDirection);

  virtual void
  _trace(JSTracer* aTrc) MOZ_OVERRIDE;

  virtual void
  _finalize(JSFreeOp* aFop) MOZ_OVERRIDE;

  // WebIDL
  JS::Value
  GetValue(JSContext* aCx, ErrorResult& aRv);

private:
  IDBCursorWithValueSync(JSContext* aCx, WorkerPrivate* aWorkerPrivate);
  ~IDBCursorWithValueSync();

  bool
  Open(JSContext* aCx, IDBKeyRange* aKeyRange);
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_idbcursorwithvaluesync_h__
