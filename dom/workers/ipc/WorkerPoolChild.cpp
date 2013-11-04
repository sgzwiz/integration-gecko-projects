/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerPoolChild.h"

#include "mozilla/dom/workers/PWorkerChild.h"
#include "mozilla/dom/indexedDB/PIndexedDBChild.h"

#include "WorkerChild.h"

USING_WORKERS_NAMESPACE

namespace {

WorkerPoolChild* gInstance = nullptr;

} // anonynous namespace

//  static
already_AddRefed<WorkerPoolChild>
WorkerPoolChild::CreateSingleton()
{
  MOZ_ASSERT(!gInstance);

  nsRefPtr<WorkerPoolChild> instance = new WorkerPoolChild();

  gInstance = instance;

  return instance.forget();
}

// static
WorkerPoolChild*
WorkerPoolChild::GetSingleton()
{
  return gInstance;
}

PWorkerChild*
WorkerPoolChild::AllocPWorkerChild(const uint64_t& aSerial)
{
  return new WorkerChild(aSerial);
}

bool
WorkerPoolChild::DeallocPWorkerChild(PWorkerChild* aActor)
{
  delete aActor;
  return true;
}

WorkerChild*
WorkerPoolChild::GetActorForSerial(uint64_t aSerial)
{
  const InfallibleTArray<PWorkerChild*>& workers =
    ManagedPWorkerChild();

  for (uint32_t i = 0; i < workers.Length(); i++) {
    WorkerChild* workerChild = static_cast<WorkerChild*>(workers[i]);
    if (workerChild->Serial() == aSerial) {
      return workerChild;
    }
  }

  return nullptr;
}
