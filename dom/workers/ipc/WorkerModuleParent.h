/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_workermoduleparent_h__
#define mozilla_dom_workers_workermoduleparent_h__

#include "mozilla/dom/workers/Workers.h"

#include "mozilla/dom/workers/PWorkerModuleParent.h"

BEGIN_WORKERS_NAMESPACE

class WorkerModuleParent : public PWorkerModuleParent
{
public:
  NS_INLINE_DECL_REFCOUNTING(WorkerModuleParent)

private:
  virtual PWorkerParent*
  AllocPWorkerParent(const WorkerHandle& aHandle) MOZ_OVERRIDE;

  virtual bool
  DeallocPWorkerParent(PWorkerParent* aActor) MOZ_OVERRIDE;

  virtual bool
  RecvPWorkerConstructor(PWorkerParent* aActor,
                         const WorkerHandle& aHandle) MOZ_OVERRIDE;
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_workermoduleparent_h__
