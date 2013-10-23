/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMClipboardEvent.h"
#include "mozilla/dom/DataTransfer.h"
#include "nsIClipboard.h"
#include "mozilla/ContentEvents.h"

using namespace mozilla;
using namespace mozilla::dom;

nsDOMClipboardEvent::nsDOMClipboardEvent(EventTarget* aOwner,
                                         nsPresContext* aPresContext,
                                         InternalClipboardEvent* aEvent)
  : nsDOMEvent(aOwner, aPresContext, aEvent ? aEvent :
               new InternalClipboardEvent(false, 0))
{
  if (aEvent) {
    mEventIsInternal = false;
  } else {
    mEventIsInternal = true;
    mEvent->time = PR_Now();
  }
}

NS_INTERFACE_MAP_BEGIN(nsDOMClipboardEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMClipboardEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMPL_ADDREF_INHERITED(nsDOMClipboardEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMClipboardEvent, nsDOMEvent)

nsresult
nsDOMClipboardEvent::InitClipboardEvent(const nsAString & aType,
                                        bool aCanBubble,
                                        bool aCancelable,
                                        nsIDOMDataTransfer* aClipboardData)
{
  nsCOMPtr<DataTransfer> clipboardData = do_QueryInterface(aClipboardData);
  NS_ENSURE_ARG(clipboardData);

  ErrorResult rv;
  InitClipboardEvent(aType, aCanBubble, aCancelable, clipboardData, rv);

  return rv.ErrorCode();
}

void
nsDOMClipboardEvent::InitClipboardEvent(const nsAString& aType,
                                        bool aCanBubble,
                                        bool aCancelable,
                                        DataTransfer* aClipboardData,
                                        ErrorResult& aError)
{
  aError = nsDOMEvent::InitEvent(aType, aCanBubble, aCancelable);
  if (aError.Failed()) {
    return;
  }

  mEvent->AsClipboardEvent()->clipboardData = aClipboardData;
}

already_AddRefed<nsDOMClipboardEvent>
nsDOMClipboardEvent::Constructor(const GlobalObject& aGlobal,
                                 const nsAString& aType,
                                 const ClipboardEventInit& aParam,
                                 mozilla::ErrorResult& aRv)
{
  nsCOMPtr<EventTarget> t = do_QueryInterface(aGlobal.GetAsSupports());
  nsRefPtr<nsDOMClipboardEvent> e =
    new nsDOMClipboardEvent(t, nullptr, nullptr);
  bool trusted = e->Init(t);

  nsRefPtr<DataTransfer> clipboardData;
  if (e->mEventIsInternal) {
    InternalClipboardEvent* event = e->mEvent->AsClipboardEvent();
    if (event) {
      // Always create a clipboardData for the copy event. If this is changed to
      // support other types of events, make sure that read/write privileges are
      // checked properly within DataTransfer.
      clipboardData = new DataTransfer(NS_COPY, false, -1);
      clipboardData->SetData(aParam.mDataType, aParam.mData);
    }
  }

  e->InitClipboardEvent(aType, aParam.mBubbles, aParam.mCancelable,
                        clipboardData, aRv);
  e->SetTrusted(trusted);
  return e.forget();
}

NS_IMETHODIMP
nsDOMClipboardEvent::GetClipboardData(nsIDOMDataTransfer** aClipboardData)
{
  NS_IF_ADDREF(*aClipboardData = GetClipboardData());
  return NS_OK;
}

mozilla::dom::DataTransfer*
nsDOMClipboardEvent::GetClipboardData()
{
  InternalClipboardEvent* event = mEvent->AsClipboardEvent();

  if (!event->clipboardData) {
    if (mEventIsInternal) {
      event->clipboardData = new DataTransfer(NS_COPY, false, -1);
    } else {
      event->clipboardData =
        new DataTransfer(event->message, event->message == NS_PASTE, nsIClipboard::kGlobalClipboard);
    }
  }

  return event->clipboardData;
}

nsresult NS_NewDOMClipboardEvent(nsIDOMEvent** aInstancePtrResult,
                                 EventTarget* aOwner,
                                 nsPresContext* aPresContext,
                                 InternalClipboardEvent* aEvent)
{
  nsDOMClipboardEvent* it =
    new nsDOMClipboardEvent(aOwner, aPresContext, aEvent);
  return CallQueryInterface(it, aInstancePtrResult);
}
