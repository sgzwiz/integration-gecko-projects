/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsThreadManager_h__
#define nsThreadManager_h__

#include "mozilla/Mutex.h"
#include "nsIThreadManager.h"
#include "nsRefPtrHashtable.h"
#include "nsThread.h"

class nsIRunnable;

class nsThreadManager : public nsIThreadManager
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITHREADMANAGER

  static nsThreadManager *get() { return &sInstance; }
  static bool initialized() { return sInitialized; }

  nsresult Init();

  // Shutdown all threads.  This function should only be called on the main
  // thread of the application process.
  void Shutdown();

  // Called by nsThread to inform the ThreadManager it exists.  This method
  // must be called when the given thread is the current thread.
  void RegisterCurrentThread(nsThread *thread);

  // Called by nsThread to inform the ThreadManager it is going away.  This
  // method must be called when the given thread is the current thread.
  void UnregisterCurrentThread(nsThread *thread);

  // Returns the current thread.  Returns null if OOM or if ThreadManager isn't
  // initialized.
  nsThread *GetCurrentThread();

  // This needs to be public in order to support static instantiation of this
  // class with older compilers (e.g., egcs-2.91.66).
  ~nsThreadManager() {}

  nsThread *getChromeThread() { return mChromeZone.thread; }

private:
  nsThreadManager()
    : mCurThreadIndex(0)
    , mMainPRThread(nullptr)
    , mLock(nullptr)
    , mInitialized(false) {
  }
  
  static nsThreadManager sInstance;
  static bool sInitialized;

  nsRefPtrHashtable<nsPtrHashKey<PRThread>, nsThread> mThreadsByPRThread;
  PRUintn             mCurThreadIndex;  // thread-local-storage index
  nsRefPtr<nsThread>  mMainThread;
  PRThread           *mMainPRThread;
  uintptr_t           mMainThreadStackPosition;
  // This is a pointer in order to allow creating nsThreadManager from
  // the static context in debug builds.
  nsAutoPtr<mozilla::Mutex> mLock;  // protects tables
  bool                mInitialized;
  size_t              mCantLockNewContent;

  struct Zone {
    Zone()
      : thread(NULL)
      , prThread(NULL)
      , threadStackPosition(0)
      , lock(NULL)
      , owner(NULL)
      , depth(0)
      , stalled(false)
      , waiting(false)
      , sticky(false)
      , unlockCount(0)
    {}

    nsRefPtr<nsThread> thread;
    PRThread *prThread;
    uintptr_t threadStackPosition;
    PRLock *lock;
    PRThread *owner;
    size_t depth;
    bool stalled;
    bool waiting;
    bool sticky;
    size_t unlockCount; // XXX remove -- debugging

    void clearOwner() {
      MOZ_ASSERT(owner);
      owner = NULL;
      depth = 0;
      stalled = false;
      sticky = false;
    }
  };

  struct SavedZone {
    SavedZone(Zone *zone) : zone(zone), depth(0), sticky(false) {}
    Zone *zone;
    size_t depth;
    bool sticky;
  };

  void SaveLock(SavedZone &v);
  void RestoreLock(SavedZone &v, PRThread *current);

  Zone mChromeZone;
  Zone mContentZones[JS_ZONE_CONTENT_LIMIT];
  bool mEverythingLocked;
  PRUint64 mAllocatedBitmask;

  static inline bool HasBit(PRUint64 bitmask, size_t bit)
  {
    MOZ_ASSERT(bit < 64);
    return bitmask & (1ULL << bit);
  }

  static inline void SetBit(PRUint64 *pbitmask, size_t bit)
  {
    MOZ_ASSERT(bit < 64);
    (*pbitmask) |= (1ULL << bit);
  }

  static inline void ClearBit(PRUint64 *pbitmask, size_t bit)
  {
    MOZ_ASSERT(bit < 64);
    (*pbitmask) &= ~(1ULL << bit);
  }

  Zone &getZone(PRInt32 zone) {
    MOZ_ASSERT(zone != JS_ZONE_NONE);
    if (zone == JS_ZONE_CHROME)
      return mChromeZone;
    MOZ_ASSERT(zone < JS_ZONE_CONTENT_LIMIT);
    return mContentZones[zone];
  }
};

#define NS_THREADMANAGER_CLASSNAME "nsThreadManager"
#define NS_THREADMANAGER_CID                       \
{ /* 7a4204c6-e45a-4c37-8ebb-6709a22c917c */       \
  0x7a4204c6,                                      \
  0xe45a,                                          \
  0x4c37,                                          \
  {0x8e, 0xbb, 0x67, 0x09, 0xa2, 0x2c, 0x91, 0x7c} \
}

#endif  // nsThreadManager_h__
