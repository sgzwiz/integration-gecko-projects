/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_serializationhelpers_h__
#define mozilla_dom_workers_serializationhelpers_h__

#include "ipc/IPCMessageUtils.h"

#include "mozilla/dom/workers/WorkerHandle.h"

namespace IPC {

template<>
struct ParamTraits<mozilla::dom::workers::WorkerHandle>
{
  typedef mozilla::dom::workers::WorkerHandle paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mId);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mId);
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    LogParam(aParam.mId, aLog);
  }
};

} // namespace IPC

#endif // mozilla_dom_workers_serializationhelpers_h__
