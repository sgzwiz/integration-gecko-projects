/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerChild.h"

#include "mozilla/dom/indexedDB/PIndexedDBChild.h"

USING_WORKERS_NAMESPACE
using namespace mozilla::dom::indexedDB;

WorkerChild::WorkerChild(uint64_t aSerial)
: mSerial(aSerial)
{
  MOZ_COUNT_CTOR(WorkerChild);
}

WorkerChild::~WorkerChild()
{
  MOZ_COUNT_DTOR(WorkerChild);
}

PIndexedDBChild*
WorkerChild::AllocPIndexedDBChild(const nsCString& aGroup,
                                  const nsCString& aASCIIOrigin)
{
  NS_NOTREACHED("Should never get here!");
  return NULL;
}

bool
WorkerChild::DeallocPIndexedDBChild(PIndexedDBChild* aActor)
{
  delete aActor;
  return true;
}
