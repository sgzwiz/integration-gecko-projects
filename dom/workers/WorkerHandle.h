/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_workerhandle_h__
#define mozilla_dom_workers_workerhandle_h__

#include "mozilla/dom/workers/Workers.h"

BEGIN_WORKERS_NAMESPACE

struct WorkerHandle
{
  WorkerHandle()
  : mId(0)
  { }

  WorkerHandle(uint64_t aId)
  : mId(aId)
  { }

  uint64_t mId;
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_workerhandle_h__
