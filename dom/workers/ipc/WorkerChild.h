/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_workerchild_h__
#define mozilla_dom_workers_workerchild_h__

#include "mozilla/dom/workers/Workers.h"

#include "mozilla/dom/workers/PWorkerChild.h"

BEGIN_WORKERS_NAMESPACE

class WorkerChild : public PWorkerChild
{
public:
  WorkerChild(uint64_t aSerial);
  virtual ~WorkerChild();

  NS_INLINE_DECL_REFCOUNTING(WorkerChild)

  uint64_t
  Serial() const
  {
    return mSerial;
  }

private:
  virtual PIndexedDBChild*
  AllocPIndexedDBChild(const nsCString& aGroup,
                       const nsCString& aASCIIOrigin) MOZ_OVERRIDE;

  virtual bool
  DeallocPIndexedDBChild(PIndexedDBChild* aActor) MOZ_OVERRIDE;

  uint64_t mSerial;
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_workerchild_h__
