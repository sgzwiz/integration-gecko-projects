/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_workerparent_h__
#define mozilla_dom_workers_workerparent_h__

#include "mozilla/dom/workers/Workers.h"

#include "mozilla/dom/workers/PWorkerParent.h"

namespace mozilla {
namespace dom {
class TabParent;
} // namespace dom
} // namespace mozilla

BEGIN_WORKERS_NAMESPACE

class WorkerPoolParent;
class WorkerPrivate;

class WorkerParent : public PWorkerParent
{
public:
  WorkerParent(WorkerPoolParent* aWorkerPoolParent);

  WorkerParent(TabParent* aTabParent);

  virtual ~WorkerParent();

  WorkerPoolParent*
  ManagerWorkerPool() const
  {
    return mManagerWorkerPool;
  }

  TabParent*
  ManagerTab() const
  {
    return mManagerTab;
  }

  void
  SetWorkerPrivate(WorkerPrivate* aWorker);

private:
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual PIndexedDBParent*
  AllocPIndexedDBParent(const nsCString& aGroup,
                        const nsCString& aASCIIOrigin) MOZ_OVERRIDE;

  virtual bool
  DeallocPIndexedDBParent(PIndexedDBParent* aActor) MOZ_OVERRIDE;

  virtual bool
  RecvPIndexedDBConstructor(PIndexedDBParent* aActor,
                            const nsCString& aGroup,
                            const nsCString& aASCIIOrigin) MOZ_OVERRIDE;

  WorkerPoolParent* mManagerWorkerPool;
  TabParent* mManagerTab;

  WorkerPrivate* mWorkerPrivate;
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_workerparent_h__
