/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DOMStringList_h
#define mozilla_dom_DOMStringList_h

#include "nsISupports.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"
#include "nsString.h"

namespace mozilla {
namespace dom {

class DOMStringList : public nsISupports,
                      public nsWrapperCache
{
public:
  DOMStringList()
  {
    SetIsDOMBinding();
  }
  virtual ~DOMStringList();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DOMStringList)

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope);
  nsISupports* GetParentObject()
  {
    return nullptr;
  }
  static DOMStringList* FromSupports(nsISupports* aSupports)
  {
    return static_cast<DOMStringList*>(aSupports);
  }

  void IndexedGetter(uint32_t aIndex, bool& aFound, nsAString& aResult)
  {
    EnsureFresh();
    if (aIndex < mNames.Length()) {
      aFound = true;
      aResult = mNames[aIndex];
    } else {
      aFound = false;
    }
  }

  void Item(uint32_t aIndex, nsAString& aResult)
  {
    EnsureFresh();
    if (aIndex < mNames.Length()) {
      aResult = mNames[aIndex];
    } else {
      aResult.SetIsVoid(true);
    }
  }

  uint32_t Length()
  {
    EnsureFresh();
    return mNames.Length();
  }

  bool Contains(const nsAString& aString)
  {
    EnsureFresh();
    return mNames.Contains(aString);
  }

  bool Add(const nsAString& aName)
  {
    return mNames.AppendElement(aName) != nullptr;
  }

  void Clear()
  {
    mNames.Clear();
  }

  nsTArray<nsString>& Names()
  {
    return mNames;
  }

  void CopyList(nsTArray<nsString>& aNames)
  {
    aNames = mNames;
  }

  virtual void EnsureFresh()
  {
  }

protected:
  nsTArray<nsString> mNames;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_DOMStringList_h */
