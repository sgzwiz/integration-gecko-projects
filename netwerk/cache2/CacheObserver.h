/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheObserver__h__
#define CacheObserver__h__

#include "nsIObserver.h"
#include "nsWeakReference.h"

namespace mozilla {
namespace net {

class CacheObserver : public nsIObserver
                    , public nsSupportsWeakReference
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  virtual ~CacheObserver() {}

  static nsresult Init();
  static nsresult Shutdown();

private:
  static CacheObserver* sSelf;
};

} // net
} // mozilla

#endif
