/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Simon Bünzli <zeniko@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * Maintains a circular buffer of recent messages, and notifies
 * listeners when new messages are logged.
 */

/* Threadsafe. */

#include "nsMemory.h"
#include "nsIServiceManager.h"
#include "nsCOMArray.h"
#include "nsThreadUtils.h"

#include "nsConsoleService.h"
#include "nsConsoleMessage.h"
#include "nsIClassInfoImpl.h"

#if defined(ANDROID)
#include <android/log.h>
#endif

using namespace mozilla;

NS_IMPL_THREADSAFE_ADDREF(nsConsoleService)
NS_IMPL_THREADSAFE_RELEASE(nsConsoleService)
NS_IMPL_CLASSINFO(nsConsoleService, NULL, nsIClassInfo::THREADSAFE | nsIClassInfo::SINGLETON, NS_CONSOLESERVICE_CID)
NS_IMPL_QUERY_INTERFACE1_CI(nsConsoleService, nsIConsoleService)
NS_IMPL_CI_INTERFACE_GETTER1(nsConsoleService, nsIConsoleService)

nsConsoleService::nsConsoleService()
    : mMessages(nsnull)
    , mCurrent(0)
    , mFull(false)
    , mDeliveringMessage(false)
    , mLock("nsConsoleService.mLock")
{
    // XXX grab this from a pref!
    // hm, but worry about circularity, bc we want to be able to report
    // prefs errs...
    mBufferSize = 250;
}

nsConsoleService::~nsConsoleService()
{
    PRUint32 i = 0;
    while (i < mBufferSize && mMessages[i] != nsnull) {
        NS_RELEASE(mMessages[i]);
        i++;
    }

    if (mMessages)
        nsMemory::Free(mMessages);
}

nsresult
nsConsoleService::Init()
{
    mMessages = (nsIConsoleMessage **)
        nsMemory::Alloc(mBufferSize * sizeof(nsIConsoleMessage *));
    if (!mMessages)
        return NS_ERROR_OUT_OF_MEMORY;

    // Array elements should be 0 initially for circular buffer algorithm.
    memset(mMessages, 0, mBufferSize * sizeof(nsIConsoleMessage *));

    mListeners.Init();

    return NS_OK;
}

namespace {

class LogMessageRunnable : public nsRunnable
{
public:
    LogMessageRunnable(nsIConsoleMessage* message, nsConsoleService* service)
        : mMessage(message)
        , mService(service)
    { }

    void AddListener(nsIConsoleListener* listener) {
        mListeners.AppendObject(listener);
    }

    NS_DECL_NSIRUNNABLE

private:
    nsCOMPtr<nsIConsoleMessage> mMessage;
    nsRefPtr<nsConsoleService> mService;
    nsCOMArray<nsIConsoleListener> mListeners;
};

NS_IMETHODIMP
LogMessageRunnable::Run()
{
    MOZ_ASSERT(NS_IsChromeOwningThread());

    mService->SetIsDelivering();

    for (PRInt32 i = 0; i < mListeners.Count(); ++i)
        mListeners[i]->Observe(mMessage);

    mService->SetDoneDelivering();

    return NS_OK;
}

PLDHashOperator
CollectCurrentListeners(nsISupports* aKey, nsIConsoleListener* aValue,
                        void* closure)
{
    LogMessageRunnable* r = static_cast<LogMessageRunnable*>(closure);
    r->AddListener(aValue);
    return PL_DHASH_NEXT;
}

} // anonymous namespace

// nsIConsoleService methods
NS_IMETHODIMP
nsConsoleService::LogMessage(nsIConsoleMessage *message)
{
    if (message == nsnull)
        return NS_ERROR_INVALID_ARG;

    if (NS_IsChromeOwningThread() && mDeliveringMessage) {
        NS_WARNING("Some console listener threw an error while inside itself. Discarding this message");
        return NS_ERROR_FAILURE;
    }

    nsRefPtr<LogMessageRunnable> r = new LogMessageRunnable(message, this);
    nsIConsoleMessage *retiredMessage;

    NS_ADDREF(message); // early, in case it's same as replaced below.

    /*
     * Lock while updating buffer, and while taking snapshot of
     * listeners array.
     */
    {
        MutexAutoLock lock(mLock);

#if defined(ANDROID)
        {
            nsXPIDLString msg;
            message->GetMessageMoz(getter_Copies(msg));
            __android_log_print(ANDROID_LOG_ERROR, "GeckoConsole",
                        "%s",
                        NS_LossyConvertUTF16toASCII(msg).get());
        }
#endif

        /*
         * If there's already a message in the slot we're about to replace,
         * we've wrapped around, and we need to release the old message.  We
         * save a pointer to it, so we can release below outside the lock.
         */
        retiredMessage = mMessages[mCurrent];
        
        mMessages[mCurrent++] = message;
        if (mCurrent == mBufferSize) {
            mCurrent = 0; // wrap around.
            mFull = true;
        }

        /*
         * Copy the listeners into the snapshot array - in case a listener
         * is removed during an Observe(...) notification...
         */
        mListeners.EnumerateRead(CollectCurrentListeners, r);
    }
    if (retiredMessage != nsnull)
        NS_RELEASE(retiredMessage);

    NS_DispatchToMainThread(r);

    return NS_OK;
}

NS_IMETHODIMP
nsConsoleService::LogStringMessage(const PRUnichar *message)
{
    nsConsoleMessage *msg = new nsConsoleMessage(message);
    return this->LogMessage(msg);
}

NS_IMETHODIMP
nsConsoleService::GetMessageArray(nsIConsoleMessage ***messages, PRUint32 *count)
{
    nsIConsoleMessage **messageArray;

    /*
     * Lock the whole method, as we don't want anyone mucking with mCurrent or
     * mFull while we're copying out the buffer.
     */
    MutexAutoLock lock(mLock);

    if (mCurrent == 0 && !mFull) {
        /*
         * Make a 1-length output array so that nobody gets confused,
         * and return a count of 0.  This should result in a 0-length
         * array object when called from script.
         */
        messageArray = (nsIConsoleMessage **)
            nsMemory::Alloc(sizeof (nsIConsoleMessage *));
        *messageArray = nsnull;
        *messages = messageArray;
        *count = 0;
        
        return NS_OK;
    }

    PRUint32 resultSize = mFull ? mBufferSize : mCurrent;
    messageArray =
        (nsIConsoleMessage **)nsMemory::Alloc((sizeof (nsIConsoleMessage *))
                                              * resultSize);

    if (messageArray == nsnull) {
        *messages = nsnull;
        *count = 0;
        return NS_ERROR_FAILURE;
    }

    PRUint32 i;
    if (mFull) {
        for (i = 0; i < mBufferSize; i++) {
            // if full, fill the buffer starting from mCurrent (which'll be
            // oldest) wrapping around the buffer to the most recent.
            messageArray[i] = mMessages[(mCurrent + i) % mBufferSize];
            NS_ADDREF(messageArray[i]);
        }
    } else {
        for (i = 0; i < mCurrent; i++) {
            messageArray[i] = mMessages[i];
            NS_ADDREF(messageArray[i]);
        }
    }
    *count = resultSize;
    *messages = messageArray;

    return NS_OK;
}

NS_IMETHODIMP
nsConsoleService::RegisterListener(nsIConsoleListener *listener)
{
    if (!NS_IsChromeOwningThread()) {
        NS_ERROR("nsConsoleService::RegisterListener is main thread only.");
        return NS_ERROR_NOT_SAME_THREAD;
    }

    nsCOMPtr<nsISupports> canonical = do_QueryInterface(listener);

    MutexAutoLock lock(mLock);
    if (mListeners.GetWeak(canonical)) {
        // Reregistering a listener isn't good
        return NS_ERROR_FAILURE;
    }
    mListeners.Put(canonical, listener);
    return NS_OK;
}

NS_IMETHODIMP
nsConsoleService::UnregisterListener(nsIConsoleListener *listener)
{
    if (!NS_IsChromeOwningThread()) {
        NS_ERROR("nsConsoleService::UnregisterListener is main thread only.");
        return NS_ERROR_NOT_SAME_THREAD;
    }

    nsCOMPtr<nsISupports> canonical = do_QueryInterface(listener);

    MutexAutoLock lock(mLock);

    if (!mListeners.GetWeak(canonical)) {
        // Unregistering a listener that was never registered?
        return NS_ERROR_FAILURE;
    }
    mListeners.Remove(canonical);
    return NS_OK;
}

NS_IMETHODIMP
nsConsoleService::Reset()
{
    /*
     * Make sure nobody trips into the buffer while it's being reset
     */
    MutexAutoLock lock(mLock);

    mCurrent = 0;
    mFull = false;

    /*
     * Free all messages stored so far (cf. destructor)
     */
    for (PRUint32 i = 0; i < mBufferSize && mMessages[i] != nsnull; i++)
        NS_RELEASE(mMessages[i]);

    return NS_OK;
}
