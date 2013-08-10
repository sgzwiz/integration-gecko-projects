/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_telephony_telephonycall_h__
#define mozilla_dom_telephony_telephonycall_h__

#include "TelephonyCommon.h"

#include "mozilla/dom/DOMError.h"

class nsPIDOMWindow;

BEGIN_TELEPHONY_NAMESPACE

class TelephonyCall MOZ_FINAL : public nsDOMEventTargetHelper
{
  nsRefPtr<Telephony> mTelephony;

  nsString mNumber;
  nsString mSecondNumber;
  nsString mState;
  bool mEmergency;
  nsRefPtr<mozilla::dom::DOMError> mError;

  uint32_t mCallIndex;
  uint16_t mCallState;
  bool mLive;
  bool mOutgoing;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_REALLY_FORWARD_NSIDOMEVENTTARGET(nsDOMEventTargetHelper)
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TelephonyCall,
                                           nsDOMEventTargetHelper)

  nsPIDOMWindow*
  GetParentObject() const
  {
    return GetOwner();
  }

  // WrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  // WebIDL
  void
  GetNumber(nsString& aNumber) const
  {
    aNumber.Assign(mNumber);
  }

  void
  GetSecondNumber(nsString& aSecondNumber) const
  {
    aSecondNumber.Assign(mSecondNumber);
  }

  void
  GetState(nsString& aState) const
  {
    aState.Assign(mState);
  }

  bool
  Emergency() const
  {
    return mEmergency;
  }

  already_AddRefed<DOMError>
  GetError() const;

  void
  Answer(ErrorResult& aRv);

  void
  HangUp(ErrorResult& aRv);

  void
  Hold(ErrorResult& aRv);

  void
  Resume(ErrorResult& aRv);

  IMPL_EVENT_HANDLER(statechange)
  IMPL_EVENT_HANDLER(dialing)
  IMPL_EVENT_HANDLER(alerting)
  IMPL_EVENT_HANDLER(connecting)
  IMPL_EVENT_HANDLER(connected)
  IMPL_EVENT_HANDLER(disconnecting)
  IMPL_EVENT_HANDLER(disconnected)
  IMPL_EVENT_HANDLER(holding)
  IMPL_EVENT_HANDLER(held)
  IMPL_EVENT_HANDLER(resuming)
  IMPL_EVENT_HANDLER(error)

  static already_AddRefed<TelephonyCall>
  Create(Telephony* aTelephony, const nsAString& aNumber, uint16_t aCallState,
         uint32_t aCallIndex = kOutgoingPlaceholderCallIndex,
         bool aEmergency = false);

  void
  ChangeState(uint16_t aCallState)
  {
    ChangeStateInternal(aCallState, true);
  }

  uint32_t
  CallIndex() const
  {
    return mCallIndex;
  }

  void
  UpdateCallIndex(uint32_t aCallIndex)
  {
    NS_ASSERTION(mCallIndex == kOutgoingPlaceholderCallIndex,
                 "Call index should not be set!");
    mCallIndex = aCallIndex;
  }

  uint16_t
  CallState() const
  {
    return mCallState;
  }

  void
  UpdateEmergency(bool aEmergency)
  {
    mEmergency = aEmergency;
  }

  void
  UpdateSecondNumber(const nsAString& aNumber)
  {
    mSecondNumber = aNumber;
  }

  bool
  IsOutgoing() const
  {
    return mOutgoing;
  }

  void
  NotifyError(const nsAString& aError);

private:
  TelephonyCall();

  ~TelephonyCall();

  void
  ChangeStateInternal(uint16_t aCallState, bool aFireEvents);

  nsresult
  DispatchCallEvent(const nsAString& aType,
                    TelephonyCall* aCall);
};

END_TELEPHONY_NAMESPACE

#endif // mozilla_dom_telephony_telephonycall_h__
