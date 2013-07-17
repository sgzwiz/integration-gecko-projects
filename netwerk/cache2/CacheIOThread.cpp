/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheIOThread.h"

#include "nsIRunnable.h"
#include "nsPrintfCString.h"
#include "mozilla/VisualEventTracer.h"

namespace mozilla {
namespace net {

nsrefcnt CacheIOThread::AddRef(void)
{
  MOZ_ASSERT(int32_t(mRefCnt) >= 0, "illegal refcnt");
  nsrefcnt count = NS_AtomicIncrementRefcnt(mRefCnt);
  NS_LOG_ADDREF(this, count, "CacheIOThread", sizeof(*this));
  return (nsrefcnt) count;
}

nsrefcnt CacheIOThread::Release(void)
{
  MOZ_ASSERT(int32_t(mRefCnt) > 0, "dup release");
  nsrefcnt count = NS_AtomicDecrementRefcnt(mRefCnt);
  NS_LOG_RELEASE(this, count, "CacheIOThread");
  if (0 == count) {
    mRefCnt = 1; /* stabilize */
    /* enable this to find non-threadsafe destructors: */
    /* NS_ASSERT_OWNINGTHREAD(CacheIOThread); */
    delete (this);
    return 0;
  }
  return count;
}

CacheIOThread::CacheIOThread()
: mMonitor("CacheIOThread")
, mThread(nullptr)
, mLowestLevelWaiting(LAST_LEVEL)
, mShutdown(false)
{
}

CacheIOThread::~CacheIOThread()
{
#ifdef DEBUG
  for (uint32_t level = 0; level < LAST_LEVEL; ++level) {
    MOZ_ASSERT(!mEventQueue[level].Length());
  }
#endif
}

nsresult CacheIOThread::Init()
{
  mThread = PR_CreateThread(PR_USER_THREAD, ThreadFunc, this,
                            PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD,
                            PR_JOINABLE_THREAD, 128 * 1024);
  if (!mThread)
    return NS_ERROR_FAILURE;

  return NS_OK;
}

nsresult CacheIOThread::Dispatch(nsIRunnable* aRunnable, uint32_t aLevel)
{
  NS_ENSURE_ARG(aLevel < LAST_LEVEL);

  MonitorAutoLock lock(mMonitor);

  if (mShutdown && (PR_GetCurrentThread() != mThread))
    return NS_ERROR_UNEXPECTED;

  mEventQueue[aLevel].AppendElement(aRunnable);
  if (mLowestLevelWaiting > aLevel)
    mLowestLevelWaiting = aLevel;

  mMonitor.NotifyAll();

  return NS_OK;
}

bool CacheIOThread::IsCurrentThread()
{
  return mThread == PR_GetCurrentThread();
}

nsresult CacheIOThread::Shutdown()
{
  {
    MonitorAutoLock lock(mMonitor);
    mShutdown = true;
    mMonitor.NotifyAll();
  }

  PR_JoinThread(mThread);
  mThread = nullptr;

  return NS_OK;
}

// static
void CacheIOThread::ThreadFunc(void* aClosure)
{
  PR_SetCurrentThreadName("Cache2 I/O");
  CacheIOThread* thread = static_cast<CacheIOThread*>(aClosure);
  thread->ThreadFunc();
}

void CacheIOThread::ThreadFunc()
{
  MonitorAutoLock lock(mMonitor);

  static PRIntervalTime const waitTime = PR_MillisecondsToInterval(5000);

  do {
    // Reset the lowest level now, so that we can detect a new event on
    // a lower level (i.e. higher priority) has been scheduled while
    // executing any previously scheduled event.
    mLowestLevelWaiting = LAST_LEVEL;

    for (uint32_t level = 0; level < LAST_LEVEL; ++level) {
      if (!mEventQueue[level].Length()) {
        // no events on this level, go to the next level
        continue;
      }

      LoopOneLevel(level);

      // Go to the first (lowest) level again
      break;
    }

    if (mLowestLevelWaiting < LAST_LEVEL)
      continue;

    lock.Wait(waitTime);

    if (mLowestLevelWaiting < LAST_LEVEL)
      continue;

  } while (!mShutdown);
}

static const char* const sLevelTraceName[] = {
  "net::cache::io::level(0)",
  "net::cache::io::level(1)",
  "net::cache::io::level(2)",
  "net::cache::io::level(3)",
  "net::cache::io::level(4)",
  "net::cache::io::level(5)",
  "net::cache::io::level(6)",
  "net::cache::io::level(7)",
  "net::cache::io::level(8)",
  "net::cache::io::level(9)",
  "net::cache::io::level(10)",
  "net::cache::io::level(11)",
  "net::cache::io::level(12)"
};

void CacheIOThread::LoopOneLevel(uint32_t aLevel)
{
  eventtracer::AutoEventTracer tracer(this, eventtracer::eExec, eventtracer::eDone,
    sLevelTraceName[aLevel]);

  nsTArray<nsRefPtr<nsIRunnable> > events;
  events.SwapElements(mEventQueue[aLevel]);
  uint32_t length = events.Length();

  MonitorAutoUnlock unlock(mMonitor);

  for (uint32_t index = 0; index < length; ++index) {
    if (mLowestLevelWaiting < aLevel) {
      // Somebody scheduled a new event on a lower level, break and harry
      // to execute it!  Don't forget to return what we haven't exec.
      MonitorAutoLock lock(mMonitor); // TODO somehow save unlock/lock here...
      mEventQueue[aLevel].InsertElementsAt(0, events.Elements() + index, length - index);
      return;
    }

    events[index]->Run();
    events[index] = nullptr;
  }
}

} // net
} // mozilla
