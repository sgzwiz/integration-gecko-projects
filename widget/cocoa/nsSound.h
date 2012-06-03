/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSound_h_
#define nsSound_h_

#include "nsISound.h"
#include "nsIStreamLoader.h"

class nsSound : public nsISound,
                public nsIStreamLoaderObserver
{
public: 
    nsSound();
    virtual ~nsSound();

    NS_DECL_ISUPPORTS

    NS_IMETHODIMP_(JSZoneId) GetZone() { return JS_ZONE_CHROME; }

    NS_DECL_NSISOUND
    NS_DECL_NSISTREAMLOADEROBSERVER
};

#endif // nsSound_h_
