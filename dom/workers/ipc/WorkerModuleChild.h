/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_workermodulechild_h__
#define mozilla_dom_workers_workermodulechild_h__

#include "mozilla/dom/workers/Workers.h"

#include "mozilla/dom/workers/PWorkerModuleChild.h"

BEGIN_WORKERS_NAMESPACE

class WorkerModuleChild : public PWorkerModuleChild
{
public:
  NS_INLINE_DECL_REFCOUNTING(WorkerModuleChild)

private:
  virtual PWorkerChild*
  AllocPWorkerChild(const WorkerHandle& aHandle) MOZ_OVERRIDE;

  virtual bool
  DeallocPWorkerChild(PWorkerChild* aActor) MOZ_OVERRIDE;
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_workermodulechild_h__
