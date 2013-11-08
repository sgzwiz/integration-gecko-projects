/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerParent.h"

#include "mozilla/dom/Element.h"
#include "mozilla/dom/indexedDB/IDBFactory.h"
#include "mozilla/dom/indexedDB/IndexedDBParent.h"
#include "mozilla/dom/TabParent.h"

#include "RuntimeService.h"
#include "WorkerPrivate.h"

USING_WORKERS_NAMESPACE
using namespace mozilla::dom;
using namespace mozilla::dom::indexedDB;

WorkerParent::WorkerParent(WorkerPoolParent* aWorkerPoolParent)
: mManagerWorkerPool(aWorkerPoolParent), mManagerTab(nullptr),
  mWorkerPrivate(nullptr)
{
  MOZ_COUNT_CTOR(WorkerParent);
}

WorkerParent::WorkerParent(TabParent* aTabParent)
: mManagerWorkerPool(nullptr), mManagerTab(aTabParent), mWorkerPrivate(NULL)
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

  aWorkerPrivate->SetActor(this);
  mWorkerPrivate = aWorkerPrivate;
}

void
WorkerParent::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mWorkerPrivate) {
    mWorkerPrivate->SetActor(static_cast<WorkerParent*>(NULL));
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

  if (!RuntimeService::IsMainProcess()) {
    NS_RUNTIMEABORT("Not supported yet!");
  }

  // XXXjanv security checks go here

  nsresult rv;

  nsRefPtr<IDBFactory> factory;
  if (mManagerWorkerPool) {
    if (mWorkerPrivate->IsSharedWorker()) {
      rv = IDBFactory::Create(aGroup, aASCIIOrigin, mManagerWorkerPool,
                              getter_AddRefs(factory));
    }
    else {
      nsCOMPtr<nsPIDOMWindow> window = mWorkerPrivate->GetWindow();
      if (!window) {
        NS_WARNING("Failed to get window!");
        return aActor->SendResponse(false);
      }

      rv = IDBFactory::Create(window, aGroup, aASCIIOrigin, mManagerWorkerPool,
                              getter_AddRefs(factory));
    }
  }
  else {
    nsCOMPtr<nsINode> node =
      do_QueryInterface(mManagerTab->GetOwnerElement());
    if (!node) {
      NS_WARNING("Failed to get owner node!");
      return aActor->SendResponse(false);
    }

    nsIDocument* doc = node->GetOwnerDocument();
    if (!doc) {
      NS_WARNING("Failed to get owner document!");
      return aActor->SendResponse(false);
    }

    nsCOMPtr<nsPIDOMWindow> window = doc->GetInnerWindow();
    if (!doc) {
      NS_WARNING("Failed to get inner window!");
      return aActor->SendResponse(false);
    }

    // Let's do a current inner check to see if the inner is active or is in
    // bf cache, and bail out if it's not active.
    nsCOMPtr<nsPIDOMWindow> outer = doc->GetWindow();
    if (!outer || outer->GetCurrentInnerWindow() != window) {
      return aActor->SendResponse(false);
    }

    ContentParent* contentParent = mManagerTab->Manager();
    MOZ_ASSERT(contentParent, "Null manager of manager?!");

    rv = IDBFactory::Create(window, aGroup, aASCIIOrigin, contentParent,
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
