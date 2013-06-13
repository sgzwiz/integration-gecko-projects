/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheObserver.h"

#include "CacheStorageService.h"
#include "CacheFileIOManager.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {
namespace net {

CacheObserver* CacheObserver::sSelf = nullptr;

NS_IMPL_ISUPPORTS2(CacheObserver,
                   nsIObserver,
                   nsISupportsWeakReference)

nsresult
CacheObserver::Init()
{
  if (sSelf) {
    return NS_OK;
  }

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (!obs) {
    return NS_ERROR_UNEXPECTED;
  }

  sSelf = new CacheObserver();
  NS_ADDREF(sSelf);

  obs->AddObserver(sSelf, "profile-after-change", true);
  obs->AddObserver(sSelf, "profile-before-change", true);
  obs->AddObserver(sSelf, "xpcom-shutdown", true);
  obs->AddObserver(sSelf, "last-pb-context-exited", true);

  return NS_OK;
}

nsresult
CacheObserver::Shutdown()
{
  if (!sSelf) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  NS_RELEASE(sSelf);
  return NS_OK;
}

NS_IMETHODIMP
CacheObserver::Observe(nsISupports* aSubject,
                       const char* aTopic,
                       const PRUnichar* aData)
{
  if (!strcmp(aTopic, "profile-after-change")) {
    CacheFileIOManager::Init();
    return NS_OK;
  }

  if (!strcmp(aTopic, "profile-before-change") ||
      !strcmp(aTopic, "xpcom-shutdown")) {
    CacheStorageService* service = CacheStorageService::Self();
    if (!service)
      return NS_OK;

    service->Shutdown();
    CacheFileIOManager::Shutdown();
    return NS_OK;
  }

  if (!strcmp(aTopic, "last-pb-context-exited")) {
    CacheStorageService* service = CacheStorageService::Self();
    if (!service)
      return NS_OK;

    service->DropPrivateBrowsingEntries();
    return NS_OK;
  }

  return NS_OK;
}

} // net
} // mozilla
