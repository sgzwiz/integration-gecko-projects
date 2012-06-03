/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAutoLayoutPhase_h
#define nsAutoLayoutPhase_h

#ifdef DEBUG

#include "nsPresContext.h"
#include "nsContentUtils.h"

struct nsAutoLayoutPhase {
  nsAutoLayoutPhase(nsPresContext* aPresContext, nsLayoutPhase aPhase)
    : mPresContext(aPresContext), mPhase(aPhase), mCount(0)
  {
    Enter();
  }

  ~nsAutoLayoutPhase()
  {
    Exit();
    NS_ASSERTION(mCount == 0, "imbalanced");
  }

  void Enter()
  {
    MOZ_ASSERT(NS_IsChromeOwningThread());

    switch (mPhase) {
      case eLayoutPhase_Paint:
      case eLayoutPhase_Reflow:
      case eLayoutPhase_FrameC:
        MOZ_ASSERT(mPresContext->mLayoutPhaseCount[eLayoutPhase_Paint] == 0);
        MOZ_ASSERT(mPresContext->mLayoutPhaseCount[eLayoutPhase_Reflow] == 0);
        MOZ_ASSERT(mPresContext->mLayoutPhaseCount[eLayoutPhase_FrameC] == 0);
        break;
      default:
        break;
    }
    ++(mPresContext->mLayoutPhaseCount[mPhase]);
    ++mCount;
  }

  void Exit()
  {
    MOZ_ASSERT(NS_IsChromeOwningThread());

    MOZ_ASSERT(mCount > 0 && mPresContext->mLayoutPhaseCount[mPhase] > 0);
    --(mPresContext->mLayoutPhaseCount[mPhase]);
    --mCount;
  }

private:
  nsPresContext* mPresContext;
  nsLayoutPhase mPhase;
  PRUint32 mCount;

#ifdef DEBUG
  nsAutoCantLockNewContent mLockGuard;
#endif
};

#define AUTO_LAYOUT_PHASE_ENTRY_POINT(pc_, phase_) \
  nsAutoLayoutPhase autoLayoutPhase((pc_), (eLayoutPhase_##phase_))
#define LAYOUT_PHASE_TEMP_EXIT() \
  PR_BEGIN_MACRO \
    autoLayoutPhase.Exit(); \
  PR_END_MACRO
#define LAYOUT_PHASE_TEMP_REENTER() \
  PR_BEGIN_MACRO \
    autoLayoutPhase.Enter(); \
  PR_END_MACRO

#else

#define AUTO_LAYOUT_PHASE_ENTRY_POINT(pc_, phase_) \
  PR_BEGIN_MACRO PR_END_MACRO
#define LAYOUT_PHASE_TEMP_EXIT() \
  PR_BEGIN_MACRO PR_END_MACRO
#define LAYOUT_PHASE_TEMP_REENTER() \
  PR_BEGIN_MACRO PR_END_MACRO

#endif

#endif // nsAutoLayoutPhase_h
