/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// nsWeakReference.cpp

#include "mozilla/Attributes.h"

#include "nsWeakReference.h"
#include "nsThreadUtils.h"
#include "nsCOMPtr.h"

class nsWeakReference MOZ_FINAL : public nsIWeakReference
  {
    public:
    // nsISupports...
      NS_DECL_ISUPPORTS

      NS_IMETHODIMP_(JSZoneId) GetZone() { return mZone; }

    // nsIWeakReference...
      NS_DECL_NSIWEAKREFERENCE

    private:
      friend class nsSupportsWeakReference;

      nsWeakReference( nsSupportsWeakReference* referent )
          : mReferent(referent), mZone(referent->GetZone())
          // ...I can only be constructed by an |nsSupportsWeakReference|
        {
          // nothing else to do here
        }

      ~nsWeakReference()
           // ...I will only be destroyed by calling |delete| myself.
        {
          MOZ_ASSERT(NS_IsOwningThread(mZone));
          if ( mReferent )
            mReferent->NoticeProxyDestruction();
        }

      void
      NoticeReferentDestruction()
          // ...called (only) by an |nsSupportsWeakReference| from _its_ dtor.
        {
          MOZ_ASSERT(NS_IsOwningThread(mZone));
          mReferent = 0;
        }

      nsSupportsWeakReference*  mReferent;
      JSZoneId mZone;
  };

nsresult
nsQueryReferent::operator()( const nsIID& aIID, void** answer ) const
  {
    nsresult status;
    if ( mWeakPtr )
      {
        if ( NS_FAILED(status = mWeakPtr->QueryReferent(aIID, answer)) )
          *answer = 0;
      }
    else
      status = NS_ERROR_NULL_POINTER;

    if ( mErrorPtr )
      *mErrorPtr = status;
    return status;
  }

NS_COM_GLUE nsIWeakReference*  // or else |already_AddRefed<nsIWeakReference>|
NS_GetWeakReference( nsISupports* aInstancePtr, nsresult* aErrorPtr )
  {
    nsresult status;

    nsIWeakReference* result = nsnull;

    if ( aInstancePtr )
      {
        nsCOMPtr<nsISupportsWeakReference> factoryPtr = do_QueryInterface(aInstancePtr, &status);
        NS_ASSERTION(factoryPtr, "Oops!  You're asking for a weak reference to an object that doesn't support that.");
        if ( factoryPtr )
          {
            status = factoryPtr->GetWeakReference(&result);
          }
        // else, |status| has already been set by |do_QueryInterface|
      }
    else
      status = NS_ERROR_NULL_POINTER;

    if ( aErrorPtr )
      *aErrorPtr = status;
    return result;
  }

extern void
NS_DumpBacktrace(const char *str, bool flush);

NS_COM_GLUE nsresult
nsSupportsWeakReference::GetWeakReference( nsIWeakReference** aInstancePtr )
  {
    if ( !aInstancePtr )
      return NS_ERROR_NULL_POINTER;

    if ( !mProxy )
      mProxy = new nsWeakReference(this);
    *aInstancePtr = mProxy;

    nsresult status;
    if ( !*aInstancePtr )
      status = NS_ERROR_OUT_OF_MEMORY;
    else
      {
        NS_ADDREF(*aInstancePtr);
        status = NS_OK;
      }

    return status;
  }

NS_IMPL_ISUPPORTS1(nsWeakReference, nsIWeakReference)

NS_IMETHODIMP
nsWeakReference::QueryReferent( const nsIID& aIID, void** aInstancePtr )
  {
    MOZ_ASSERT(NS_IsOwningThread(mZone));
    return mReferent ? mReferent->QueryInterface(aIID, aInstancePtr) : NS_ERROR_NULL_POINTER;
  }

void
nsSupportsWeakReference::ClearWeakReferences()
	{
		if ( mProxy )
			{
				mProxy->NoticeReferentDestruction();
				mProxy = 0;
			}
	}

void
nsSupportsWeakReference::UpdateWeakReferencesZone()
{
  // For use when an object's zone changes after creation (its actual zone was
  // not known at the point of creation) and weak references to it may exist.
  if (mProxy)
    mProxy->mZone = GetZone();
}
