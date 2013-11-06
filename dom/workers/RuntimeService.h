/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_runtimeservice_h__
#define mozilla_dom_workers_runtimeservice_h__

#include "Workers.h"

#include "nsIObserver.h"

#include "mozilla/Attributes.h"
#include "mozilla/Mutex.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsAutoPtr.h"
#include "nsClassHashtable.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsString.h"
#include "nsTArray.h"

class MessageLoop;
class nsIThread;
class nsITimer;
class nsPIDOMWindow;

namespace base {
class Thread;
}

BEGIN_WORKERS_NAMESPACE

class SharedWorker;
class WorkerPrivate;
class WorkerPoolParent;
class WorkerPoolChild;

class RuntimeService MOZ_FINAL : public nsIObserver
{
  struct SharedWorkerInfo
  {
    WorkerPrivate* mWorkerPrivate;
    nsCString mScriptSpec;
    nsString mName;

    SharedWorkerInfo(WorkerPrivate* aWorkerPrivate,
                     const nsACString& aScriptSpec,
                     const nsAString& aName)
    : mWorkerPrivate(aWorkerPrivate), mScriptSpec(aScriptSpec), mName(aName)
    { }
  };

  struct WorkerDomainInfo
  {
    nsCString mDomain;
    nsTArray<WorkerPrivate*> mActiveWorkers;
    nsTArray<WorkerPrivate*> mQueuedWorkers;
    nsClassHashtable<nsCStringHashKey, SharedWorkerInfo> mSharedWorkerInfos;
    uint32_t mChildWorkerCount;

    WorkerDomainInfo()
    : mActiveWorkers(1), mChildWorkerCount(0)
    { }

    uint32_t
    ActiveWorkerCount() const
    {
      return mActiveWorkers.Length() + mChildWorkerCount;
    }
  };

  struct IdleThreadInfo
  {
    nsCOMPtr<nsIThread> mThread;
    mozilla::TimeStamp mExpirationTime;
  };

  struct MatchSharedWorkerInfo
  {
    WorkerPrivate* mWorkerPrivate;
    SharedWorkerInfo* mSharedWorkerInfo;

    MatchSharedWorkerInfo(WorkerPrivate* aWorkerPrivate)
    : mWorkerPrivate(aWorkerPrivate), mSharedWorkerInfo(nullptr)
    { }
  };

  mozilla::Mutex mMutex;

  // Protected by mMutex.
  nsClassHashtable<nsCStringHashKey, WorkerDomainInfo> mDomainMap;

  // Protected by mMutex.
  nsTArray<IdleThreadInfo> mIdleThreadArray;

  // Protected by mMutex.
  uint64_t mLastWorkerSerial;

  // *Not* protected by mMutex.
  nsClassHashtable<nsPtrHashKey<nsPIDOMWindow>,
                   nsTArray<WorkerPrivate*> > mWindowMap;

  // Only used on the main thread.
  nsCOMPtr<nsITimer> mIdleThreadTimer;

  nsCString mDetectorName;
  nsCString mSystemCharset;

  static bool sIsMainProcess;
  static JSSettings sDefaultJSSettings;

  base::Thread* mIPCThread;
  MessageLoop* mIPCMessageLoop;
  nsRefPtr<WorkerPoolParent> mWorkerPoolParent;
  nsRefPtr<WorkerPoolChild> mWorkerPoolChild;

public:
  struct NavigatorStrings
  {
    nsString mAppName;
    nsString mAppVersion;
    nsString mPlatform;
    nsString mUserAgent;
  };

private:
  NavigatorStrings mNavigatorStrings;

  // True when the observer service holds a reference to this object.
  bool mObserved;
  bool mShuttingDown;
  bool mNavigatorStringsLoaded;
  bool mIPCInitialized;
  bool mIndexedDBSyncEnabled;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static RuntimeService*
  GetOrCreateService();

  static RuntimeService*
  GetService();

  static bool
  IsMainProcess()
#ifdef DEBUG
  ;
#else
  {
    return sIsMainProcess;
  }
#endif

  bool
  RegisterWorker(JSContext* aCx, WorkerPrivate* aWorkerPrivate);

  void
  UnregisterWorker(JSContext* aCx, WorkerPrivate* aWorkerPrivate);

  void
  CancelWorkersForWindow(nsPIDOMWindow* aWindow);

  void
  SuspendWorkersForWindow(nsPIDOMWindow* aWindow);

  void
  ResumeWorkersForWindow(nsPIDOMWindow* aWindow);

  nsresult
  CreateSharedWorker(const GlobalObject& aGlobal,
                     const nsAString& aScriptURL,
                     const nsAString& aName,
                     SharedWorker** aSharedWorker);

  void
  ForgetSharedWorker(WorkerPrivate* aWorkerPrivate);

  const nsACString&
  GetDetectorName() const
  {
    return mDetectorName;
  }

  const nsACString&
  GetSystemCharset() const
  {
    return mSystemCharset;
  }

  const NavigatorStrings&
  GetNavigatorStrings() const
  {
    return mNavigatorStrings;
  }

  bool IsIndexedDBSyncEnabled() const
  {
    return mIndexedDBSyncEnabled;
  }

  void
  NoteIdleThread(nsIThread* aThread);

  static void
  GetDefaultJSSettings(JSSettings& aSettings)
  {
    AssertIsOnMainThread();
    aSettings = sDefaultJSSettings;
  }

  static void
  SetDefaultJSContextOptions(const JS::ContextOptions& aContentOptions,
                             const JS::ContextOptions& aChromeOptions)
  {
    AssertIsOnMainThread();
    sDefaultJSSettings.content.options = aContentOptions;
    sDefaultJSSettings.chrome.options = aChromeOptions;
  }

  void
  UpdateAllWorkerJSContextOptions();

  static void
  SetDefaultJSGCSettings(JSGCParamKey aKey, uint32_t aValue)
  {
    AssertIsOnMainThread();
    sDefaultJSSettings.ApplyGCSetting(aKey, aValue);
  }

  void
  UpdateAllWorkerMemoryParameter(JSGCParamKey aKey, uint32_t aValue);

  static uint32_t
  GetContentCloseHandlerTimeoutSeconds()
  {
    return sDefaultJSSettings.content.maxScriptRuntime;
  }

  static uint32_t
  GetChromeCloseHandlerTimeoutSeconds()
  {
    return sDefaultJSSettings.chrome.maxScriptRuntime;
  }

#ifdef JS_GC_ZEAL
  static void
  SetDefaultGCZeal(uint8_t aGCZeal, uint32_t aFrequency)
  {
    AssertIsOnMainThread();
    sDefaultJSSettings.gcZeal = aGCZeal;
    sDefaultJSSettings.gcZealFrequency = aFrequency;
  }

  void
  UpdateAllWorkerGCZeal();
#endif

  static void
  SetDefaultJITHardening(bool aJITHardening)
  {
    AssertIsOnMainThread();
    sDefaultJSSettings.jitHardening = aJITHardening;
  }

  void
  UpdateAllWorkerJITHardening(bool aJITHardening);

  void
  GarbageCollectAllWorkers(bool aShrinking);

  bool
  WorkersDumpEnabled();

  bool
  InitIPC();

  static MessageLoop*
  IPCMessageLoop()
  {
    MOZ_ASSERT(IsMainProcess(), "Wrong process!");

    RuntimeService* service = GetService();
    NS_ASSERTION(service, "Must have a service here!");

    return service->mIPCMessageLoop;
  }

private:
  RuntimeService();
  ~RuntimeService();

  nsresult
  Init();

  void
  Shutdown();

  void
  Cleanup();

  static PLDHashOperator
  AddAllTopLevelWorkersToArray(const nsACString& aKey,
                               WorkerDomainInfo* aData,
                               void* aUserArg);

  static PLDHashOperator
  RemoveSharedWorkerFromWindowMap(nsPIDOMWindow* aKey,
                                  nsAutoPtr<nsTArray<WorkerPrivate*> >& aData,
                                  void* aUserArg);

  static PLDHashOperator
  FindSharedWorkerInfo(const nsACString& aKey,
                       SharedWorkerInfo* aData,
                       void* aUserArg);

  void
  GetWorkersForWindow(nsPIDOMWindow* aWindow,
                      nsTArray<WorkerPrivate*>& aWorkers);

  bool
  ScheduleWorker(JSContext* aCx, WorkerPrivate* aWorkerPrivate);

  static void
  ShutdownIdleThreads(nsITimer* aTimer, void* aClosure);
};

END_WORKERS_NAMESPACE

#endif /* mozilla_dom_workers_runtimeservice_h__ */
