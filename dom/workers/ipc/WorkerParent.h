/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_workerparent_h__
#define mozilla_dom_workers_workerparent_h__

#include "mozilla/dom/workers/Workers.h"

#include "mozilla/dom/workers/PWorkerParent.h"

BEGIN_WORKERS_NAMESPACE

class WorkerPrivate;

class WorkerParent : public PWorkerParent
{
public:
  WorkerParent();
  virtual ~WorkerParent();

  NS_INLINE_DECL_REFCOUNTING(WorkerParent)

  void
  SetWorkerPrivate(WorkerPrivate* aWorker);

private:
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  WorkerPrivate* mWorkerPrivate;
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_workerparent_h__
