/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_workerpoolchild_h__
#define mozilla_dom_workers_workerpoolchild_h__

#include "mozilla/dom/workers/Workers.h"

#include "mozilla/dom/workers/PWorkerPoolChild.h"

BEGIN_WORKERS_NAMESPACE

class WorkerChild;

class WorkerPoolChild : public PWorkerPoolChild
{
public:
  NS_INLINE_DECL_REFCOUNTING(WorkerPoolChild)

  static already_AddRefed<WorkerPoolChild>
  CreateSingleton();

  static WorkerPoolChild*
  GetSingleton();

  WorkerChild*
  GetActorForSerial(uint64_t aSerial);

private:
  virtual PWorkerChild*
  AllocPWorkerChild(const uint64_t& aSerial) MOZ_OVERRIDE;

  virtual bool
  DeallocPWorkerChild(PWorkerChild* aActor) MOZ_OVERRIDE;
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_workerpoolchild_h__
