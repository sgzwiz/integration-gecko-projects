/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerParent.h"

#include "WorkerPrivate.h"

USING_WORKERS_NAMESPACE

WorkerParent::WorkerParent()
: mWorkerPrivate(NULL)
{
  MOZ_COUNT_CTOR(WorkerParent);
}

WorkerParent::~WorkerParent()
{
  MOZ_COUNT_DTOR(WorkerParent);
}

void
WorkerParent::SetWorkerPrivate(WorkerPrivate* aWorkerPrivate)
{
  MOZ_ASSERT(aWorkerPrivate);

  aWorkerPrivate->SetWorkerParent(this);
  mWorkerPrivate= aWorkerPrivate;
}

void
WorkerParent::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mWorkerPrivate) {
    mWorkerPrivate->SetWorkerParent(static_cast<WorkerParent*>(NULL));
#ifdef DEBUG
    mWorkerPrivate = NULL;
#endif
  }
}
