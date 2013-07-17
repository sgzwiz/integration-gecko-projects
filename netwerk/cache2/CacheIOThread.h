/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheIOThread__h__
#define CacheIOThread__h__

#include "prthread.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"
#include "mozilla/Monitor.h"

class nsIRunnable;

namespace mozilla {
namespace net {

class CacheIOThread
{
public:
  nsrefcnt AddRef(void);
  nsrefcnt Release(void);

  CacheIOThread();
  ~CacheIOThread();

  enum ELevel {
    IMMEDIATE,
    DOOM_PRIORITY,
    OPEN_PRIORITY,
    READ_PRIORITY,
    DOOM,
    OPEN,
    READ,
    OPEN_TRUNCATE,
    WRITE,
    CLOSE,
    LAST_LEVEL
  };

  nsresult Init();
  nsresult Dispatch(nsIRunnable* aRunnable, uint32_t aLevel);
  bool IsCurrentThread();
  nsresult Shutdown();

private:
  static void ThreadFunc(void* aClosure);
  void ThreadFunc();
  void LoopOneLevel(uint32_t aLevel);

  mozilla::Monitor mMonitor;
  PRThread* mThread;
  uint32_t mLowestLevelWaiting;
  nsTArray<nsRefPtr<nsIRunnable> > mEventQueue[LAST_LEVEL];

  bool mShutdown;

  nsAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD
};

} // net
} // mozilla

#endif
