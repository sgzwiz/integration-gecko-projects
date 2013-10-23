/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerParent.h"

#include "mozilla/dom/indexedDB/IDBFactory.h"
#include "mozilla/dom/indexedDB/IndexedDatabaseManager.h"
#include "mozilla/dom/indexedDB/IndexedDBParent.h"

#include "WorkerPrivate.h"

USING_WORKERS_NAMESPACE
using namespace mozilla::dom::indexedDB;

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

PIndexedDBParent*
WorkerParent::AllocPIndexedDBParent(const nsCString& aGroup,
                                    const nsCString& aASCIIOrigin)
{
  return new IndexedDBParent(this);
}

bool
WorkerParent::DeallocPIndexedDBParent(PIndexedDBParent* aActor)
{
  delete aActor;
  return true;
}

bool
WorkerParent::RecvPIndexedDBConstructor(PIndexedDBParent* aActor,
                                        const nsCString& aGroup,
                                        const nsCString& aASCIIOrigin)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsRefPtr<IndexedDatabaseManager> mgr = IndexedDatabaseManager::GetOrCreate();
  if (!mgr) {
    NS_WARNING("Failed to get manager!");
    return aActor->SendResponse(false);
  }

  if (!IndexedDatabaseManager::IsMainProcess()) {
    NS_RUNTIMEABORT("Not supported yet!");
  }

  // XXXjanv security checks go here

  nsresult rv;

  nsRefPtr<IDBFactory> factory;
  if (mWorkerPrivate->IsSharedWorker()) {
    rv = IDBFactory::Create(aGroup, aASCIIOrigin, nullptr,
                            getter_AddRefs(factory));
  }
  else {
    nsCOMPtr<nsPIDOMWindow> window = mWorkerPrivate->GetWindow();
    if (!window) {
      NS_WARNING("Failed to get window!");
      return aActor->SendResponse(false);
    }

    rv = IDBFactory::Create(window, aGroup, aASCIIOrigin, nullptr,
                            getter_AddRefs(factory));
  }

  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to create factory!");
    return aActor->SendResponse(false);
  }

  if (!factory) {
    return aActor->SendResponse(false);
  }

  IndexedDBParent* actor = static_cast<IndexedDBParent*>(aActor);
  actor->mFactory = factory;
  actor->mASCIIOrigin = aASCIIOrigin;

  return aActor->SendResponse(true);
}
