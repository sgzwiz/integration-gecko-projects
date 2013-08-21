/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerModuleParent.h"

#include "RuntimeService.h"
#include "WorkerParent.h"
#include "WorkerPrivate.h"

USING_WORKERS_NAMESPACE

PWorkerParent*
WorkerModuleParent::AllocPWorkerParent(const WorkerHandle& aHandle)
{
  return new WorkerParent();
}

bool
WorkerModuleParent::DeallocPWorkerParent(PWorkerParent* aActor)
{
  delete aActor;
  return true;
}

bool
WorkerModuleParent::RecvPWorkerConstructor(PWorkerParent* aActor,
                                           const WorkerHandle& aHandle)
{
  WorkerPrivate* workerPrivate = RuntimeService::GetTopLevelWorker(aHandle.mId);

  WorkerParent* actor = static_cast<WorkerParent*>(aActor);
  actor->SetWorkerPrivate(workerPrivate);

  return true;
}
