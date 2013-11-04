/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerPoolParent.h"

#include "RuntimeService.h"
#include "WorkerParent.h"
#include "WorkerPrivate.h"

USING_WORKERS_NAMESPACE

namespace {

WorkerPoolParent* gInstance = nullptr;

} // anonynous namespace

//  static
already_AddRefed<WorkerPoolParent>
WorkerPoolParent::CreateSingleton()
{
  MOZ_ASSERT(!gInstance);

  nsRefPtr<WorkerPoolParent> instance = new WorkerPoolParent();

  gInstance = instance;

  return instance.forget();
}

// static
WorkerPoolParent*
WorkerPoolParent::GetSingleton()
{
  return gInstance;
}

PWorkerParent*
WorkerPoolParent::AllocPWorkerParent(const uint64_t& aSerial)
{
  NS_NOTREACHED("Should never get here!");
  return NULL;
}

bool
WorkerPoolParent::DeallocPWorkerParent(PWorkerParent* aActor)
{
  delete aActor;
  return true;
}
