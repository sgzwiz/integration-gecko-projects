/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXPathEvaluator_h__
#define nsXPathEvaluator_h__

#include "nsIDOMXPathEvaluator.h"
#include "nsIXPathEvaluatorInternal.h"
#include "nsIWeakReference.h"
#include "nsAutoPtr.h"
#include "nsString.h"
#include "txResultRecycler.h"
#include "nsAgg.h"
#include "nsTArray.h"
#include "mozilla/Attributes.h"

/**
 * A class for evaluating an XPath expression string
 */
class nsXPathEvaluator MOZ_FINAL : public nsIDOMXPathEvaluator,
                                   public nsIXPathEvaluatorInternal
{
public:
    nsXPathEvaluator(nsISupports *aOuter);

    nsresult Init();

    // nsISupports interface (support aggregation)
    NS_DECL_AGGREGATED

    NS_IMETHODIMP_(JSZoneId) GetZone() { return fOuter->GetZone(); }

    // nsIDOMXPathEvaluator interface
    NS_DECL_NSIDOMXPATHEVALUATOR

    // nsIXPathEvaluatorInternal interface
    NS_IMETHOD SetDocument(nsIDOMDocument* aDocument);
    NS_IMETHOD CreateExpression(const nsAString &aExpression, 
                                nsIDOMXPathNSResolver *aResolver,
                                nsTArray<nsString> *aNamespaceURIs,
                                nsTArray<nsCString> *aContractIDs,
                                nsCOMArray<nsISupports> *aState,
                                nsIDOMXPathExpression **aResult);

private:
    nsresult CreateExpression(const nsAString & aExpression,
                              nsIDOMXPathNSResolver *aResolver,
                              nsTArray<PRInt32> *aNamespaceIDs,
                              nsTArray<nsCString> *aContractIDs,
                              nsCOMArray<nsISupports> *aState,
                              nsIDOMXPathExpression **aResult);

    nsWeakPtr mDocument;
    nsRefPtr<txResultRecycler> mRecycler;
};

/* d0a75e02-b5e7-11d5-a7f2-df109fb8a1fc */
#define TRANSFORMIIX_XPATH_EVALUATOR_CID   \
{ 0xd0a75e02, 0xb5e7, 0x11d5, { 0xa7, 0xf2, 0xdf, 0x10, 0x9f, 0xb8, 0xa1, 0xfc } }

#endif
