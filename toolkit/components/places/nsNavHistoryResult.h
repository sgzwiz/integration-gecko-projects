/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The definitions of objects that make up a history query result set. This file
 * should only be included by nsNavHistory.h, include that if you want these
 * classes.
 */

#ifndef nsNavHistoryResult_h_
#define nsNavHistoryResult_h_

#include "nsTArray.h"
#include "nsInterfaceHashtable.h"
#include "nsDataHashtable.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/storage.h"
#include "Helpers.h"

class nsNavHistory;
class nsNavHistoryQuery;
class nsNavHistoryQueryOptions;

class nsNavHistoryContainerResultNode;
class nsNavHistoryFolderResultNode;
class nsNavHistoryQueryResultNode;
class nsNavHistoryVisitResultNode;

/**
 * hashkey wrapper using PRInt64 KeyType
 *
 * @see nsTHashtable::EntryType for specification
 *
 * This just truncates the 64-bit int to a 32-bit one for using a hash number.
 * It is used for bookmark folder IDs, which should be way less than 2^32.
 */
class nsTrimInt64HashKey : public PLDHashEntryHdr
{
public:
  typedef const PRInt64& KeyType;
  typedef const PRInt64* KeyTypePointer;

  nsTrimInt64HashKey(KeyTypePointer aKey) : mValue(*aKey) { }
  nsTrimInt64HashKey(const nsTrimInt64HashKey& toCopy) : mValue(toCopy.mValue) { }
  ~nsTrimInt64HashKey() { }

  KeyType GetKey() const { return mValue; }
  bool KeyEquals(KeyTypePointer aKey) const { return *aKey == mValue; }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey)
    { return static_cast<PRUint32>((*aKey) & PR_UINT32_MAX); }
  enum { ALLOW_MEMMOVE = true };

private:
  const PRInt64 mValue;
};


// Declare methods for implementing nsINavBookmarkObserver
// and nsINavHistoryObserver (some methods, such as BeginUpdateBatch overlap)
#define NS_DECL_BOOKMARK_HISTORY_OBSERVER                               \
  NS_DECL_NSINAVBOOKMARKOBSERVER                                        \
  NS_IMETHOD OnVisit(nsIURI* aURI, PRInt64 aVisitId, PRTime aTime,      \
                     PRInt64 aSessionId, PRInt64 aReferringId,          \
                     PRUint32 aTransitionType, const nsACString& aGUID, \
                     PRUint32* aAdded);                                 \
  NS_IMETHOD OnTitleChanged(nsIURI* aURI, const nsAString& aPageTitle,  \
                            const nsACString& aGUID);                   \
  NS_IMETHOD OnBeforeDeleteURI(nsIURI *aURI, const nsACString& aGUID,   \
                               PRUint16 aReason);                       \
  NS_IMETHOD OnDeleteURI(nsIURI *aURI, const nsACString& aGUID,         \
                         PRUint16 aReason);                             \
  NS_IMETHOD OnClearHistory();                                          \
  NS_IMETHOD OnPageChanged(nsIURI *aURI, PRUint32 aChangedAttribute,    \
                           const nsAString &aNewValue,                  \
                           const nsACString &aGUID);                    \
  NS_IMETHOD OnDeleteVisits(nsIURI* aURI, PRTime aVisitTime,            \
                            const nsACString& aGUID, PRUint16 aReason);

// nsNavHistoryResult
//
//    nsNavHistory creates this object and fills in mChildren (by getting
//    it through GetTopLevel()). Then FilledAllResults() is called to finish
//    object initialization.

#define NS_NAVHISTORYRESULT_IID \
  { 0x455d1d40, 0x1b9b, 0x40e6, { 0xa6, 0x41, 0x8b, 0xb7, 0xe8, 0x82, 0x23, 0x87 } }

class nsNavHistoryResult : public nsSupportsWeakReference,
                           public nsINavHistoryResult,
                           public nsINavBookmarkObserver,
                           public nsINavHistoryObserver
{
public:
  static nsresult NewHistoryResult(nsINavHistoryQuery** aQueries,
                                   PRUint32 aQueryCount,
                                   nsNavHistoryQueryOptions* aOptions,
                                   nsNavHistoryContainerResultNode* aRoot,
                                   bool aBatchInProgress,
                                   nsNavHistoryResult** result);

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_NAVHISTORYRESULT_IID)

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  NS_IMETHODIMP_(JSZoneId) GetZone() { return JS_ZONE_CHROME; }

  NS_DECL_NSINAVHISTORYRESULT
  NS_DECL_BOOKMARK_HISTORY_OBSERVER
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsNavHistoryResult, nsINavHistoryResult)

  void AddHistoryObserver(nsNavHistoryQueryResultNode* aNode);
  void AddBookmarkFolderObserver(nsNavHistoryFolderResultNode* aNode, PRInt64 aFolder);
  void AddAllBookmarksObserver(nsNavHistoryQueryResultNode* aNode);
  void RemoveHistoryObserver(nsNavHistoryQueryResultNode* aNode);
  void RemoveBookmarkFolderObserver(nsNavHistoryFolderResultNode* aNode, PRInt64 aFolder);
  void RemoveAllBookmarksObserver(nsNavHistoryQueryResultNode* aNode);
  void StopObserving();

public:
  // two-stage init, use NewHistoryResult to construct
  nsNavHistoryResult(nsNavHistoryContainerResultNode* mRoot);
  virtual ~nsNavHistoryResult();
  nsresult Init(nsINavHistoryQuery** aQueries,
                PRUint32 aQueryCount,
                nsNavHistoryQueryOptions *aOptions);

  nsRefPtr<nsNavHistoryContainerResultNode> mRootNode;

  nsCOMArray<nsINavHistoryQuery> mQueries;
  nsCOMPtr<nsNavHistoryQueryOptions> mOptions;

  // One of nsNavHistoryQueryOptions.SORY_BY_* This is initialized to mOptions.sortingMode,
  // but may be overridden if the user clicks on one of the columns.
  PRUint16 mSortingMode;
  // If root node is closed and we try to apply a sortingMode, it would not
  // work.  So we will apply it when the node will be reopened and populated.
  // This var states the fact we need to apply sortingMode in such a situation.
  bool mNeedsToApplySortingMode;

  // The sorting annotation to be used for in SORT_BY_ANNOTATION_* modes
  nsCString mSortingAnnotation;

  // node observers
  bool mIsHistoryObserver;
  bool mIsBookmarkFolderObserver;
  bool mIsAllBookmarksObserver;

  typedef nsTArray< nsRefPtr<nsNavHistoryQueryResultNode> > QueryObserverList;
  QueryObserverList mHistoryObservers;
  QueryObserverList mAllBookmarksObservers;

  typedef nsTArray< nsRefPtr<nsNavHistoryFolderResultNode> > FolderObserverList;
  nsDataHashtable<nsTrimInt64HashKey, FolderObserverList*> mBookmarkFolderObservers;
  FolderObserverList* BookmarkFolderObserversForId(PRInt64 aFolderId, bool aCreate);

  typedef nsTArray< nsRefPtr<nsNavHistoryContainerResultNode> > ContainerObserverList;

  void RecursiveExpandCollapse(nsNavHistoryContainerResultNode* aContainer,
                               bool aExpand);

  void InvalidateTree();
  
  bool mBatchInProgress;

  nsMaybeWeakPtrArray<nsINavHistoryResultObserver> mObservers;
  bool mSuppressNotifications;

  ContainerObserverList mRefreshParticipants;
  void requestRefresh(nsNavHistoryContainerResultNode* aContainer);
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsNavHistoryResult, NS_NAVHISTORYRESULT_IID)

// nsNavHistoryResultNode
//
//    This is the base class for every node in a result set. The result itself
//    is a node (nsNavHistoryResult inherits from this), as well as every
//    leaf and branch on the tree.

#define NS_NAVHISTORYRESULTNODE_IID \
  {0x54b61d38, 0x57c1, 0x11da, {0x95, 0xb8, 0x00, 0x13, 0x21, 0xc9, 0xf6, 0x9e}}

// These are all the simple getters, they can be used for the result node
// implementation and all subclasses. More complex are GetIcon, GetParent
// (which depends on the definition of container result node), and GetUri
// (which is overridded for lazy construction for some containers).
#define NS_IMPLEMENT_SIMPLE_RESULTNODE_NO_GETITEMMID \
  NS_IMETHOD GetTitle(nsACString& aTitle) \
    { aTitle = mTitle; return NS_OK; } \
  NS_IMETHOD GetAccessCount(PRUint32* aAccessCount) \
    { *aAccessCount = mAccessCount; return NS_OK; } \
  NS_IMETHOD GetTime(PRTime* aTime) \
    { *aTime = mTime; return NS_OK; } \
  NS_IMETHOD GetIndentLevel(PRInt32* aIndentLevel) \
    { *aIndentLevel = mIndentLevel; return NS_OK; } \
  NS_IMETHOD GetBookmarkIndex(PRInt32* aIndex) \
    { *aIndex = mBookmarkIndex; return NS_OK; } \
  NS_IMETHOD GetDateAdded(PRTime* aDateAdded) \
    { *aDateAdded = mDateAdded; return NS_OK; } \
  NS_IMETHOD GetLastModified(PRTime* aLastModified) \
    { *aLastModified = mLastModified; return NS_OK; }

#define NS_IMPLEMENT_SIMPLE_RESULTNODE \
  NS_IMPLEMENT_SIMPLE_RESULTNODE_NO_GETITEMMID \
  NS_IMETHOD GetItemId(PRInt64* aId) \
    { *aId = mItemId; return NS_OK; }

// This is used by the base classes instead of
// NS_FORWARD_NSINAVHISTORYRESULTNODE(nsNavHistoryResultNode) because they
// need to redefine GetType and GetUri rather than forwarding them. This
// implements all the simple getters instead of forwarding because they are so
// short and we can save a virtual function call.
//
// (GetUri is redefined only by QueryResultNode and FolderResultNode because
// the queries might not necessarily be parsed. The rest just return the node's
// buffer.)
#define NS_FORWARD_COMMON_RESULTNODE_TO_BASE_NO_GETITEMMID \
  NS_IMPLEMENT_SIMPLE_RESULTNODE_NO_GETITEMMID \
  NS_IMETHOD GetIcon(nsACString& aIcon) \
    { return nsNavHistoryResultNode::GetIcon(aIcon); } \
  NS_IMETHOD GetParent(nsINavHistoryContainerResultNode** aParent) \
    { return nsNavHistoryResultNode::GetParent(aParent); } \
  NS_IMETHOD GetParentResult(nsINavHistoryResult** aResult) \
    { return nsNavHistoryResultNode::GetParentResult(aResult); } \
  NS_IMETHOD GetTags(nsAString& aTags) \
    { return nsNavHistoryResultNode::GetTags(aTags); }

#define NS_FORWARD_COMMON_RESULTNODE_TO_BASE \
  NS_FORWARD_COMMON_RESULTNODE_TO_BASE_NO_GETITEMMID \
  NS_IMETHOD GetItemId(PRInt64* aId) \
    { *aId = mItemId; return NS_OK; }

class nsNavHistoryResultNode : public nsINavHistoryResultNode
{
public:
  nsNavHistoryResultNode(const nsACString& aURI, const nsACString& aTitle,
                         PRUint32 aAccessCount, PRTime aTime,
                         const nsACString& aIconURI);
  virtual ~nsNavHistoryResultNode() {}

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_NAVHISTORYRESULTNODE_IID)

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsNavHistoryResultNode)

  NS_IMPLEMENT_SIMPLE_RESULTNODE
  NS_IMETHOD GetIcon(nsACString& aIcon);
  NS_IMETHOD GetParent(nsINavHistoryContainerResultNode** aParent);
  NS_IMETHOD GetParentResult(nsINavHistoryResult** aResult);
  NS_IMETHOD GetType(PRUint32* type)
    { *type = nsNavHistoryResultNode::RESULT_TYPE_URI; return NS_OK; }
  NS_IMETHOD GetUri(nsACString& aURI)
    { aURI = mURI; return NS_OK; }
  NS_IMETHOD GetTags(nsAString& aTags);

  virtual void OnRemoving();

  // Called from result's onItemChanged, see also bookmark observer declaration in
  // nsNavHistoryFolderResultNode
  NS_IMETHOD OnItemChanged(PRInt64 aItemId,
                           const nsACString &aProperty,
                           bool aIsAnnotationProperty,
                           const nsACString &aValue,
                           PRTime aNewLastModified,
                           PRUint16 aItemType,
                           PRInt64 aParentId,
                           const nsACString& aGUID,
                           const nsACString& aParentGUID);

public:

  nsNavHistoryResult* GetResult();
  nsNavHistoryQueryOptions* GetGeneratingOptions();

  // These functions test the type. We don't use a virtual function since that
  // would take a vtable slot for every one of (potentially very many) nodes.
  // Note that GetType() already has a vtable slot because its on the iface.
  bool IsTypeContainer(PRUint32 type) {
    return (type == nsINavHistoryResultNode::RESULT_TYPE_QUERY ||
            type == nsINavHistoryResultNode::RESULT_TYPE_FOLDER ||
            type == nsINavHistoryResultNode::RESULT_TYPE_FOLDER_SHORTCUT);
  }
  bool IsContainer() {
    PRUint32 type;
    GetType(&type);
    return IsTypeContainer(type);
  }
  static bool IsTypeURI(PRUint32 type) {
    return (type == nsINavHistoryResultNode::RESULT_TYPE_URI ||
            type == nsINavHistoryResultNode::RESULT_TYPE_VISIT ||
            type == nsINavHistoryResultNode::RESULT_TYPE_FULL_VISIT);
  }
  bool IsURI() {
    PRUint32 type;
    GetType(&type);
    return IsTypeURI(type);
  }
  static bool IsTypeVisit(PRUint32 type) {
    return (type == nsINavHistoryResultNode::RESULT_TYPE_VISIT ||
            type == nsINavHistoryResultNode::RESULT_TYPE_FULL_VISIT);
  }
  bool IsVisit() {
    PRUint32 type;
    GetType(&type);
    return IsTypeVisit(type);
  }
  static bool IsTypeFolder(PRUint32 type) {
    return (type == nsINavHistoryResultNode::RESULT_TYPE_FOLDER ||
            type == nsINavHistoryResultNode::RESULT_TYPE_FOLDER_SHORTCUT);
  }
  bool IsFolder() {
    PRUint32 type;
    GetType(&type);
    return IsTypeFolder(type);
  }
  static bool IsTypeQuery(PRUint32 type) {
    return (type == nsINavHistoryResultNode::RESULT_TYPE_QUERY);
  }
  bool IsQuery() {
    PRUint32 type;
    GetType(&type);
    return IsTypeQuery(type);
  }
  bool IsSeparator() {
    PRUint32 type;
    GetType(&type);
    return (type == nsINavHistoryResultNode::RESULT_TYPE_SEPARATOR);
  }
  nsNavHistoryContainerResultNode* GetAsContainer() {
    NS_ASSERTION(IsContainer(), "Not a container");
    return reinterpret_cast<nsNavHistoryContainerResultNode*>(this);
  }
  nsNavHistoryVisitResultNode* GetAsVisit() {
    NS_ASSERTION(IsVisit(), "Not a visit");
    return reinterpret_cast<nsNavHistoryVisitResultNode*>(this);
  }
  nsNavHistoryFolderResultNode* GetAsFolder() {
    NS_ASSERTION(IsFolder(), "Not a folder");
    return reinterpret_cast<nsNavHistoryFolderResultNode*>(this);
  }
  nsNavHistoryQueryResultNode* GetAsQuery() {
    NS_ASSERTION(IsQuery(), "Not a query");
    return reinterpret_cast<nsNavHistoryQueryResultNode*>(this);
  }

  nsRefPtr<nsNavHistoryContainerResultNode> mParent;
  nsCString mURI; // not necessarily valid for containers, call GetUri
  nsCString mTitle;
  nsString mTags;
  bool mAreTagsSorted;
  PRUint32 mAccessCount;
  PRInt64 mTime;
  nsCString mFaviconURI;
  PRInt32 mBookmarkIndex;
  PRInt64 mItemId;
  PRInt64 mFolderId;
  PRTime mDateAdded;
  PRTime mLastModified;

  // The indent level of this node. The root node will have a value of -1.  The
  // root's children will have a value of 0, and so on.
  PRInt32 mIndentLevel;

  PRInt32 mFrecency; // Containers have 0 frecency.
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsNavHistoryResultNode, NS_NAVHISTORYRESULTNODE_IID)

// nsNavHistoryVisitResultNode

#define NS_IMPLEMENT_VISITRESULT \
  NS_IMETHOD GetUri(nsACString& aURI) { aURI = mURI; return NS_OK; } \
  NS_IMETHOD GetSessionId(PRInt64* aSessionId) \
    { *aSessionId = mSessionId; return NS_OK; }

class nsNavHistoryVisitResultNode : public nsNavHistoryResultNode,
                                    public nsINavHistoryVisitResultNode
{
public:
  nsNavHistoryVisitResultNode(const nsACString& aURI, const nsACString& aTitle,
                              PRUint32 aAccessCount, PRTime aTime,
                              const nsACString& aIconURI, PRInt64 aSession);

  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_COMMON_RESULTNODE_TO_BASE
  NS_IMETHOD GetType(PRUint32* type)
    { *type = nsNavHistoryResultNode::RESULT_TYPE_VISIT; return NS_OK; }
  NS_IMPLEMENT_VISITRESULT

public:

  PRInt64 mSessionId;
};


// nsNavHistoryFullVisitResultNode

#define NS_IMPLEMENT_FULLVISITRESULT \
  NS_IMPLEMENT_VISITRESULT \
  NS_IMETHOD GetVisitId(PRInt64 *aVisitId) \
    { *aVisitId = mVisitId; return NS_OK; } \
  NS_IMETHOD GetReferringVisitId(PRInt64 *aReferringVisitId) \
    { *aReferringVisitId = mReferringVisitId; return NS_OK; } \
  NS_IMETHOD GetTransitionType(PRInt32 *aTransitionType) \
    { *aTransitionType = mTransitionType; return NS_OK; }

class nsNavHistoryFullVisitResultNode : public nsNavHistoryVisitResultNode,
                                        public nsINavHistoryFullVisitResultNode
{
public:
  nsNavHistoryFullVisitResultNode(
    const nsACString& aURI, const nsACString& aTitle, PRUint32 aAccessCount,
    PRTime aTime, const nsACString& aIconURI, PRInt64 aSession,
    PRInt64 aVisitId, PRInt64 aReferringVisitId, PRInt32 aTransitionType);

  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_COMMON_RESULTNODE_TO_BASE
  NS_IMETHOD GetType(PRUint32* type)
    { *type = nsNavHistoryResultNode::RESULT_TYPE_FULL_VISIT; return NS_OK; }
  NS_IMPLEMENT_FULLVISITRESULT

public:
  PRInt64 mVisitId;
  PRInt64 mReferringVisitId;
  PRInt32 mTransitionType;
};


// nsNavHistoryContainerResultNode
//
//    This is the base class for all nodes that can have children. It is
//    overridden for nodes that are dynamically populated such as queries and
//    folders. It is used directly for simple containers such as host groups
//    in history views.

// derived classes each provide their own implementation of has children and
// forward the rest to us using this macro
#define NS_FORWARD_CONTAINERNODE_EXCEPT_HASCHILDREN_AND_READONLY \
  NS_IMETHOD GetState(PRUint16* _state) \
    { return nsNavHistoryContainerResultNode::GetState(_state); } \
  NS_IMETHOD GetContainerOpen(bool *aContainerOpen) \
    { return nsNavHistoryContainerResultNode::GetContainerOpen(aContainerOpen); } \
  NS_IMETHOD SetContainerOpen(bool aContainerOpen) \
    { return nsNavHistoryContainerResultNode::SetContainerOpen(aContainerOpen); } \
  NS_IMETHOD GetChildCount(PRUint32 *aChildCount) \
    { return nsNavHistoryContainerResultNode::GetChildCount(aChildCount); } \
  NS_IMETHOD GetChild(PRUint32 index, nsINavHistoryResultNode **_retval) \
    { return nsNavHistoryContainerResultNode::GetChild(index, _retval); } \
  NS_IMETHOD GetChildIndex(nsINavHistoryResultNode* aNode, PRUint32* _retval) \
    { return nsNavHistoryContainerResultNode::GetChildIndex(aNode, _retval); } \
  NS_IMETHOD FindNodeByDetails(const nsACString& aURIString, PRTime aTime, \
                               PRInt64 aItemId, bool aRecursive, \
                               nsINavHistoryResultNode** _retval) \
    { return nsNavHistoryContainerResultNode::FindNodeByDetails(aURIString, aTime, aItemId, \
                                                                aRecursive, _retval); }

#define NS_NAVHISTORYCONTAINERRESULTNODE_IID \
  { 0x6e3bf8d3, 0x22aa, 0x4065, { 0x86, 0xbc, 0x37, 0x46, 0xb5, 0xb3, 0x2c, 0xe8 } }

class nsNavHistoryContainerResultNode : public nsNavHistoryResultNode,
                                        public nsINavHistoryContainerResultNode
{
public:
  nsNavHistoryContainerResultNode(
    const nsACString& aURI, const nsACString& aTitle,
    const nsACString& aIconURI, PRUint32 aContainerType,
    bool aReadOnly, nsNavHistoryQueryOptions* aOptions);
  nsNavHistoryContainerResultNode(
    const nsACString& aURI, const nsACString& aTitle,
    PRTime aTime,
    const nsACString& aIconURI, PRUint32 aContainerType,
    bool aReadOnly, nsNavHistoryQueryOptions* aOptions);

  virtual nsresult Refresh();
  virtual ~nsNavHistoryContainerResultNode();

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_NAVHISTORYCONTAINERRESULTNODE_IID)

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsNavHistoryContainerResultNode, nsNavHistoryResultNode)
  NS_FORWARD_COMMON_RESULTNODE_TO_BASE
  NS_IMETHOD GetType(PRUint32* type)
    { *type = mContainerType; return NS_OK; }
  NS_IMETHOD GetUri(nsACString& aURI)
    { aURI = mURI; return NS_OK; }
  NS_DECL_NSINAVHISTORYCONTAINERRESULTNODE

public:

  virtual void OnRemoving();

  bool AreChildrenVisible();

  // Overridded by descendents to populate.
  virtual nsresult OpenContainer();
  nsresult CloseContainer(bool aSuppressNotifications = false);

  virtual nsresult OpenContainerAsync();

  // This points to the result that owns this container. All containers have
  // their result pointer set so we can quickly get to the result without having
  // to walk the tree. Yet, this also saves us from storing a million pointers
  // for every leaf node to the result.
  nsRefPtr<nsNavHistoryResult> mResult;

  // For example, RESULT_TYPE_QUERY. Query and Folder results override GetType
  // so this is not used, but is still kept in sync.
  PRUint32 mContainerType;

  // When there are children, this stores the open state in the tree
  // this is set to the default in the constructor.
  bool mExpanded;

  // Filled in by the result type generator in nsNavHistory.
  nsCOMArray<nsNavHistoryResultNode> mChildren;

  bool mChildrenReadOnly;

  nsCOMPtr<nsNavHistoryQueryOptions> mOptions;

  void FillStats();
  nsresult ReverseUpdateStats(PRInt32 aAccessCountChange);

  // Sorting methods.
  typedef nsCOMArray<nsNavHistoryResultNode>::nsCOMArrayComparatorFunc SortComparator;
  virtual PRUint16 GetSortType();
  virtual void GetSortingAnnotation(nsACString& aSortingAnnotation);

  static SortComparator GetSortingComparator(PRUint16 aSortType);
  virtual void RecursiveSort(const char* aData,
                             SortComparator aComparator);
  PRUint32 FindInsertionPoint(nsNavHistoryResultNode* aNode, SortComparator aComparator,
                              const char* aData, bool* aItemExists);
  bool DoesChildNeedResorting(PRUint32 aIndex, SortComparator aComparator,
                                const char* aData);

  static PRInt32 SortComparison_StringLess(const nsAString& a, const nsAString& b);

  static PRInt32 SortComparison_Bookmark(nsNavHistoryResultNode* a,
                                         nsNavHistoryResultNode* b,
                                         void* closure);
  static PRInt32 SortComparison_TitleLess(nsNavHistoryResultNode* a,
                                          nsNavHistoryResultNode* b,
                                          void* closure);
  static PRInt32 SortComparison_TitleGreater(nsNavHistoryResultNode* a,
                                             nsNavHistoryResultNode* b,
                                             void* closure);
  static PRInt32 SortComparison_DateLess(nsNavHistoryResultNode* a,
                                         nsNavHistoryResultNode* b,
                                         void* closure);
  static PRInt32 SortComparison_DateGreater(nsNavHistoryResultNode* a,
                                            nsNavHistoryResultNode* b,
                                            void* closure);
  static PRInt32 SortComparison_URILess(nsNavHistoryResultNode* a,
                                        nsNavHistoryResultNode* b,
                                        void* closure);
  static PRInt32 SortComparison_URIGreater(nsNavHistoryResultNode* a,
                                           nsNavHistoryResultNode* b,
                                           void* closure);
  static PRInt32 SortComparison_VisitCountLess(nsNavHistoryResultNode* a,
                                               nsNavHistoryResultNode* b,
                                               void* closure);
  static PRInt32 SortComparison_VisitCountGreater(nsNavHistoryResultNode* a,
                                                  nsNavHistoryResultNode* b,
                                                  void* closure);
  static PRInt32 SortComparison_KeywordLess(nsNavHistoryResultNode* a,
                                            nsNavHistoryResultNode* b,
                                            void* closure);
  static PRInt32 SortComparison_KeywordGreater(nsNavHistoryResultNode* a,
                                               nsNavHistoryResultNode* b,
                                               void* closure);
  static PRInt32 SortComparison_AnnotationLess(nsNavHistoryResultNode* a,
                                               nsNavHistoryResultNode* b,
                                               void* closure);
  static PRInt32 SortComparison_AnnotationGreater(nsNavHistoryResultNode* a,
                                                  nsNavHistoryResultNode* b,
                                                  void* closure);
  static PRInt32 SortComparison_DateAddedLess(nsNavHistoryResultNode* a,
                                              nsNavHistoryResultNode* b,
                                              void* closure);
  static PRInt32 SortComparison_DateAddedGreater(nsNavHistoryResultNode* a,
                                                 nsNavHistoryResultNode* b,
                                                 void* closure);
  static PRInt32 SortComparison_LastModifiedLess(nsNavHistoryResultNode* a,
                                                 nsNavHistoryResultNode* b,
                                                 void* closure);
  static PRInt32 SortComparison_LastModifiedGreater(nsNavHistoryResultNode* a,
                                                    nsNavHistoryResultNode* b,
                                                    void* closure);
  static PRInt32 SortComparison_TagsLess(nsNavHistoryResultNode* a,
                                         nsNavHistoryResultNode* b,
                                         void* closure);
  static PRInt32 SortComparison_TagsGreater(nsNavHistoryResultNode* a,
                                            nsNavHistoryResultNode* b,
                                            void* closure);
  static PRInt32 SortComparison_FrecencyLess(nsNavHistoryResultNode* a,
                                             nsNavHistoryResultNode* b,
                                             void* closure);
  static PRInt32 SortComparison_FrecencyGreater(nsNavHistoryResultNode* a,
                                                nsNavHistoryResultNode* b,
                                                void* closure);

  // finding children: THESE DO NOT ADDREF
  nsNavHistoryResultNode* FindChildURI(nsIURI* aURI, PRUint32* aNodeIndex)
  {
    nsCAutoString spec;
    if (NS_FAILED(aURI->GetSpec(spec)))
      return nullptr;
    return FindChildURI(spec, aNodeIndex);
  }
  nsNavHistoryResultNode* FindChildURI(const nsACString& aSpec,
                                       PRUint32* aNodeIndex);
  nsNavHistoryContainerResultNode* FindChildContainerByName(const nsACString& aTitle,
                                                            PRUint32* aNodeIndex);
  // returns the index of the given node, -1 if not found
  PRInt32 FindChild(nsNavHistoryResultNode* aNode)
    { return mChildren.IndexOf(aNode); }

  nsresult InsertChildAt(nsNavHistoryResultNode* aNode, PRInt32 aIndex,
                         bool aIsTemporary = false);
  nsresult InsertSortedChild(nsNavHistoryResultNode* aNode,
                             bool aIsTemporary = false,
                             bool aIgnoreDuplicates = false);
  bool EnsureItemPosition(PRUint32 aIndex);
  void MergeResults(nsCOMArray<nsNavHistoryResultNode>* aNodes);
  nsresult ReplaceChildURIAt(PRUint32 aIndex, nsNavHistoryResultNode* aNode);
  nsresult RemoveChildAt(PRInt32 aIndex, bool aIsTemporary = false);

  void RecursiveFindURIs(bool aOnlyOne,
                         nsNavHistoryContainerResultNode* aContainer,
                         const nsCString& aSpec,
                         nsCOMArray<nsNavHistoryResultNode>* aMatches);
  nsresult UpdateURIs(bool aRecursive, bool aOnlyOne, bool aUpdateSort,
                      const nsCString& aSpec,
                      nsresult (*aCallback)(nsNavHistoryResultNode*,void*, nsNavHistoryResult*),
                      void* aClosure);
  nsresult ChangeTitles(nsIURI* aURI, const nsACString& aNewTitle,
                        bool aRecursive, bool aOnlyOne);

protected:

  enum AsyncCanceledState {
    NOT_CANCELED, CANCELED, CANCELED_RESTART_NEEDED
  };

  void CancelAsyncOpen(bool aRestart);
  nsresult NotifyOnStateChange(PRUint16 aOldState);

  nsCOMPtr<mozIStoragePendingStatement> mAsyncPendingStmt;
  AsyncCanceledState mAsyncCanceledState;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsNavHistoryContainerResultNode,
                              NS_NAVHISTORYCONTAINERRESULTNODE_IID)

// nsNavHistoryQueryResultNode
//
//    Overridden container type for complex queries over history and/or
//    bookmarks. This keeps itself in sync by listening to history and
//    bookmark notifications.

class nsNavHistoryQueryResultNode : public nsNavHistoryContainerResultNode,
                                    public nsINavHistoryQueryResultNode
{
public:
  nsNavHistoryQueryResultNode(const nsACString& aTitle,
                              const nsACString& aIconURI,
                              const nsACString& aQueryURI);
  nsNavHistoryQueryResultNode(const nsACString& aTitle,
                              const nsACString& aIconURI,
                              const nsCOMArray<nsNavHistoryQuery>& aQueries,
                              nsNavHistoryQueryOptions* aOptions);
  nsNavHistoryQueryResultNode(const nsACString& aTitle,
                              const nsACString& aIconURI,
                              PRTime aTime,
                              const nsCOMArray<nsNavHistoryQuery>& aQueries,
                              nsNavHistoryQueryOptions* aOptions);

  virtual ~nsNavHistoryQueryResultNode();

  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_COMMON_RESULTNODE_TO_BASE
  NS_IMETHOD GetType(PRUint32* type)
    { *type = nsNavHistoryResultNode::RESULT_TYPE_QUERY; return NS_OK; }
  NS_IMETHOD GetUri(nsACString& aURI); // does special lazy creation
  NS_FORWARD_CONTAINERNODE_EXCEPT_HASCHILDREN_AND_READONLY
  NS_IMETHOD GetHasChildren(bool* aHasChildren);
  NS_IMETHOD GetChildrenReadOnly(bool *aChildrenReadOnly)
    { return nsNavHistoryContainerResultNode::GetChildrenReadOnly(aChildrenReadOnly); }
  NS_DECL_NSINAVHISTORYQUERYRESULTNODE

  bool CanExpand();
  bool IsContainersQuery();

  virtual nsresult OpenContainer();

  NS_DECL_BOOKMARK_HISTORY_OBSERVER
  virtual void OnRemoving();

public:
  // this constructs lazily mURI from mQueries and mOptions, call
  // VerifyQueriesSerialized either this or mQueries/mOptions should be valid
  nsresult VerifyQueriesSerialized();

  // these may be constructed lazily from mURI, call VerifyQueriesParsed
  // either this or mURI should be valid
  nsCOMArray<nsNavHistoryQuery> mQueries;
  PRUint32 mLiveUpdate; // one of QUERYUPDATE_* in nsNavHistory.h
  bool mHasSearchTerms;
  nsresult VerifyQueriesParsed();

  // safe options getter, ensures queries are parsed
  nsNavHistoryQueryOptions* Options();

  // this indicates whether the query contents are valid, they don't go away
  // after the container is closed until a notification comes in
  bool mContentsValid;

  nsresult FillChildren();
  void ClearChildren(bool unregister);
  nsresult Refresh();

  virtual PRUint16 GetSortType();
  virtual void GetSortingAnnotation(nsACString& aSortingAnnotation);
  virtual void RecursiveSort(const char* aData,
                             SortComparator aComparator);

  nsCOMPtr<nsIURI> mRemovingURI;
  nsresult NotifyIfTagsChanged(nsIURI* aURI);

  PRUint32 mBatchChanges;
};


// nsNavHistoryFolderResultNode
//
//    Overridden container type for bookmark folders. It will keep the contents
//    of the folder in sync with the bookmark service.

class nsNavHistoryFolderResultNode : public nsNavHistoryContainerResultNode,
                                     public nsINavHistoryQueryResultNode,
                                     public mozilla::places::AsyncStatementCallback
{
public:
  nsNavHistoryFolderResultNode(const nsACString& aTitle,
                               nsNavHistoryQueryOptions* options,
                               PRInt64 aFolderId);

  virtual ~nsNavHistoryFolderResultNode();

  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_COMMON_RESULTNODE_TO_BASE_NO_GETITEMMID
  NS_IMETHOD GetType(PRUint32* type) {
    if (mQueryItemId != -1) {
      *type = nsNavHistoryResultNode::RESULT_TYPE_FOLDER_SHORTCUT;
    } else {
      *type = nsNavHistoryResultNode::RESULT_TYPE_FOLDER;
    }
    return NS_OK;
  }
  NS_IMETHOD GetUri(nsACString& aURI);
  NS_FORWARD_CONTAINERNODE_EXCEPT_HASCHILDREN_AND_READONLY
  NS_IMETHOD GetHasChildren(bool* aHasChildren);
  NS_IMETHOD GetChildrenReadOnly(bool *aChildrenReadOnly);
  NS_IMETHOD GetItemId(PRInt64 *aItemId);
  NS_DECL_NSINAVHISTORYQUERYRESULTNODE

  virtual nsresult OpenContainer();

  virtual nsresult OpenContainerAsync();
  NS_DECL_ASYNCSTATEMENTCALLBACK

  // This object implements a bookmark observer interface without deriving from
  // the bookmark observers. This is called from the result's actual observer
  // and it knows all observers are FolderResultNodes
  NS_DECL_NSINAVBOOKMARKOBSERVER

  virtual void OnRemoving();

  // this indicates whether the folder contents are valid, they don't go away
  // after the container is closed until a notification comes in
  bool mContentsValid;

  // If the node is generated from a place:folder=X query, this is the query's
  // itemId.
  PRInt64 mQueryItemId;

  nsresult FillChildren();
  void ClearChildren(bool aUnregister);
  nsresult Refresh();

  bool StartIncrementalUpdate();
  void ReindexRange(PRInt32 aStartIndex, PRInt32 aEndIndex, PRInt32 aDelta);

  nsNavHistoryResultNode* FindChildById(PRInt64 aItemId,
                                        PRUint32* aNodeIndex);

private:

  nsresult OnChildrenFilled();
  void EnsureRegisteredAsFolderObserver();
  nsresult FillChildrenAsync();

  bool mIsRegisteredFolderObserver;
  PRInt32 mAsyncBookmarkIndex;
};

// nsNavHistorySeparatorResultNode
//
// Separator result nodes do not hold any data.
class nsNavHistorySeparatorResultNode : public nsNavHistoryResultNode
{
public:
  nsNavHistorySeparatorResultNode();

  NS_IMETHOD GetType(PRUint32* type)
    { *type = nsNavHistoryResultNode::RESULT_TYPE_SEPARATOR; return NS_OK; }
};

#endif // nsNavHistoryResult_h_
