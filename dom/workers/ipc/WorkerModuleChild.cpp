/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerModuleChild.h"

#include "mozilla/dom/workers/PWorkerChild.h"
#include "mozilla/dom/indexedDB/PIndexedDBChild.h"
#include "WorkerChild.h"

USING_WORKERS_NAMESPACE
using namespace mozilla::dom::indexedDB;

PWorkerChild*
WorkerModuleChild::AllocPWorkerChild(const WorkerHandle& aHandle)
{
  NS_NOTREACHED("Should never get here!");
  return NULL;
}

bool
WorkerModuleChild::DeallocPWorkerChild(PWorkerChild* aActor)
{
  delete aActor;
  return true;
}
