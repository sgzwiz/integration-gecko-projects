/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_workerpoolparent_h__
#define mozilla_dom_workers_workerpoolparent_h__

#include "mozilla/dom/workers/Workers.h"

#include "mozilla/dom/workers/PWorkerPoolParent.h"

BEGIN_WORKERS_NAMESPACE

class WorkerPoolParent : public PWorkerPoolParent
{
public:
  NS_INLINE_DECL_REFCOUNTING(WorkerPoolParent)

  static already_AddRefed<WorkerPoolParent>
  CreateSingleton();

  static WorkerPoolParent*
  GetSingleton();

private:
  virtual PWorkerParent*
  AllocPWorkerParent(const uint64_t& aSerial) MOZ_OVERRIDE;

  virtual bool
  DeallocPWorkerParent(PWorkerParent* aActor) MOZ_OVERRIDE;
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_workerpoolparent_h__
