/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsIRDFCursor.h"
#include "nsIRDFNode.h"
#include "nsIRDFDataBase.h"
#include "nsISupportsArray.h"
#include "nsRDFCID.h"
#include "nsRepository.h"
#include "nsVoidArray.h"
#include "prlog.h"

/*

  XXX rvg --- chris, are you happy with this (I rewrote it).

  A simple "database" implementation. An RDF database is just a
  "strategy" pattern for combining individual data sources into a
  collective graph.


  1) A database is a sequence of data sources. The set of data sources
     can be specified during creation of the database. Data sources
     can also be added/deleted from a database later.

  2) The aggregation mechanism is based on simple super-positioning of
     the graphs from the datasources. If there is a conflict (i.e., 
     data source A has a true arc from foo to bar while data source B
     has a false arc from foo to bar), the data source that it earlier
     in the sequence wins.

     The implementation below doesn't really do this and needs to be
     fixed.

*/

static NS_DEFINE_IID(kIRDFArcsInCursorIID,    NS_IRDFARCSINCURSOR_IID);
static NS_DEFINE_IID(kIRDFArcsOutCursorIID,   NS_IRDFARCSOUTCURSOR_IID);
static NS_DEFINE_IID(kIRDFAssertionCursorIID, NS_IRDFASSERTIONCURSOR_IID);
static NS_DEFINE_IID(kIRDFCursorIID,          NS_IRDFCURSOR_IID);
static NS_DEFINE_IID(kIRDFDataBaseIID,        NS_IRDFDATABASE_IID);
static NS_DEFINE_IID(kIRDFDataSourceIID,      NS_IRDFDATASOURCE_IID);
static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);

static NS_DEFINE_CID(kRDFBookmarkDataSourceCID, NS_RDFBOOKMARKDATASOURCE_CID);

////////////////////////////////////////////////////////////////////////
// MultiCursor
//
//   This class encapsulates all of the behavior that is necessary to
//   stripe a cursor across several different data sources, including
//   checks to determine whether an negation in an "earlier" data
//   source masks an assertion in a "later" data source.
//

class MultiCursor {
private:
    nsIRDFDataSource*  mDataSource0;
    nsIRDFDataSource** mDataSources;
    nsIRDFCursor*      mCurrentCursor;
    nsIRDFNode*        mNextResult;
    PRBool             mNextTruthValue;
    PRInt32            mNextDataSource;
    PRInt32            mCount;

public:
    MultiCursor(nsVoidArray& dataSources);
    virtual ~MultiCursor(void);

    NS_IMETHOD AdvanceImpl(void);

    virtual nsresult
    GetCursor(nsIRDFDataSource* ds, nsIRDFCursor** result) = 0;

    virtual nsresult
    IsCurrentNegatedBy(nsIRDFDataSource* ds0,
                       PRBool* result) = 0;

    nsIRDFCursor*
    GetCurrentCursor(void) {
        return mCurrentCursor;
    }
};


MultiCursor::MultiCursor(nsVoidArray& dataSources)
    : mDataSource0(nsnull),
      mDataSources(nsnull),
      mCurrentCursor(nsnull),
      mNextResult(nsnull),
      mCount(0),
      mNextDataSource(0)
{
    mCount = dataSources.Count();
    mDataSources = new nsIRDFDataSource*[mCount];

    PR_ASSERT(mDataSources);
    if (! mDataSources)
        return;

    for (PRInt32 i = 0; i < mCount; ++i) {
        mDataSources[i] = NS_STATIC_CAST(nsIRDFDataSource*, dataSources[i]);
        NS_ADDREF(mDataSources[i]);
    }

    mDataSource0 = mDataSources[0];
    NS_ADDREF(mDataSource0);
}


MultiCursor::~MultiCursor(void)
{
    NS_IF_RELEASE(mNextResult);
    NS_IF_RELEASE(mCurrentCursor);
    for (PRInt32 i = mCount - 1; i >= 0; --i) {
        NS_IF_RELEASE(mDataSources[i]);
    }
    NS_IF_RELEASE(mDataSource0);
}

NS_IMETHODIMP
MultiCursor::AdvanceImpl(void)
{
    nsresult rv;

    while (mNextDataSource < mCount) {
        if (! mCurrentCursor) {
            // We don't have a current cursor, so create a new one on
            // the next data source.
            rv = GetCursor(mDataSources[mNextDataSource], &mCurrentCursor);

            if (NS_FAILED(rv))
                return rv;
        }

        do {
            if (NS_FAILED(rv = mCurrentCursor->Advance())) {
                // If we can't advance the current cursor, then either
                // a catastrophic error occurred, or it's depleted. If
                // it's just depleted break out of this loop and
                // advance to the next cursor.
                if (rv != NS_ERROR_RDF_CURSOR_EMPTY)
                    return rv;

                break;
            }

            // Even if the current cursor has more elements, we still
            // need to check that the current element isn't masked by
            // the "main" data source.
            
            // See if data source zero has the negation
            // XXX rvg --- this needs to be fixed so that we look at all the prior 
            // data sources for negations
            PRBool hasNegation;
            if (NS_FAILED(rv = IsCurrentNegatedBy(mDataSource0,
                                                  &hasNegation)))
                return rv;

            // if not, we're done
            if (! hasNegation)
                return NS_OK;

            // Otherwise, we found the negation in data source
            // zero. Gotta keep lookin'...
        } while (1);

        NS_RELEASE(mCurrentCursor);
        NS_RELEASE(mDataSources[mNextDataSource]);
        ++mNextDataSource;
    }

    // if we get here, there aren't any elements left.
    return NS_ERROR_UNEXPECTED;
}


////////////////////////////////////////////////////////////////////////
// SimpleDBAssertionCursorImpl
//
//   An assertion cursor implementation for the simple db.
//
class SimpleDBAssertionCursorImpl : public MultiCursor,
                                    public nsIRDFAssertionCursor
{
private:
    nsIRDFResource* mSource;
    nsIRDFResource* mProperty;
    PRBool mTruthValue;

public:
    SimpleDBAssertionCursorImpl(nsVoidArray& dataSources,
                     nsIRDFResource* source,
                     nsIRDFResource* property,
                     PRBool tv);

    virtual ~SimpleDBAssertionCursorImpl();

    // MultiCursor protocol methods
    virtual nsresult
    GetCursor(nsIRDFDataSource* ds, nsIRDFCursor** result);

    virtual nsresult
    IsCurrentNegatedBy(nsIRDFDataSource* ds0,
                       PRBool* result);

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIRDFAssertionCursor interface
    NS_IMETHOD Advance(void);

    NS_IMETHOD GetDataSource(nsIRDFDataSource** aDataSource);
    NS_IMETHOD GetSubject(nsIRDFResource** aResource);
    NS_IMETHOD GetPredicate(nsIRDFResource** aPredicate);
    NS_IMETHOD GetObject(nsIRDFNode** aObject);
    NS_IMETHOD GetTruthValue(PRBool* aTruthValue);
};

SimpleDBAssertionCursorImpl::SimpleDBAssertionCursorImpl(nsVoidArray& dataSources,
                                   nsIRDFResource* source,
                                   nsIRDFResource* property,
                                   PRBool tv)
    : MultiCursor(dataSources),
      mSource(source),
      mProperty(property),
      mTruthValue(tv)
{
    NS_IF_ADDREF(mSource);
    NS_IF_ADDREF(mProperty);
}


SimpleDBAssertionCursorImpl::~SimpleDBAssertionCursorImpl(void)
{
    NS_IF_RELEASE(mProperty);
    NS_IF_RELEASE(mSource);
}

NS_IMPL_ADDREF(SimpleDBAssertionCursorImpl);
NS_IMPL_RELEASE(SimpleDBAssertionCursorImpl);

NS_IMETHODIMP_(nsresult)
SimpleDBAssertionCursorImpl::QueryInterface(REFNSIID iid, void** result) {
    if (! result)
        return NS_ERROR_NULL_POINTER;

    if (iid.Equals(kIRDFAssertionCursorIID) ||
        iid.Equals(kIRDFCursorIID) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsIRDFAssertionCursor*, this);
        /* AddRef(); // not necessary */
        return NS_OK;
    }
    return NS_NOINTERFACE;
}


nsresult
SimpleDBAssertionCursorImpl::GetCursor(nsIRDFDataSource* ds, nsIRDFCursor** result)
{
    return ds->GetTargets(mSource, mProperty, mTruthValue,
                          (nsIRDFAssertionCursor**) result);
}

nsresult
SimpleDBAssertionCursorImpl::IsCurrentNegatedBy(nsIRDFDataSource* ds0,
                                                PRBool* result)
{
    nsresult rv;

    // No need to QueryInterface() b/c this is a closed system.
    nsIRDFAssertionCursor* c =
        (nsIRDFAssertionCursor*) GetCurrentCursor();

    PRBool tv;
    if (NS_FAILED(rv = c->GetTruthValue(&tv)))
        return rv;

    nsIRDFNode* object;
    if (NS_FAILED(rv = c->GetObject(&object)))
        return rv;

    rv = ds0->HasAssertion(mSource, mProperty, object, !tv, result);
    NS_RELEASE(object);

    return rv;
}


NS_IMETHODIMP
SimpleDBAssertionCursorImpl::Advance(void)
{
    return AdvanceImpl();
}

NS_IMETHODIMP
SimpleDBAssertionCursorImpl::GetDataSource(nsIRDFDataSource** aDataSource)
{
    nsIRDFAssertionCursor* cursor =
        (nsIRDFAssertionCursor*) GetCurrentCursor();

    if (! cursor)
        return NS_ERROR_UNEXPECTED;

    return cursor->GetDataSource(aDataSource);
}

NS_IMETHODIMP
SimpleDBAssertionCursorImpl::GetSubject(nsIRDFResource** aResource)
{
    nsIRDFAssertionCursor* cursor =
        (nsIRDFAssertionCursor*) GetCurrentCursor();

    if (! cursor)
        return NS_ERROR_UNEXPECTED;

    return cursor->GetSubject(aResource);
}

NS_IMETHODIMP
SimpleDBAssertionCursorImpl::GetPredicate(nsIRDFResource** aPredicate)
{
    nsIRDFAssertionCursor* cursor =
        (nsIRDFAssertionCursor*) GetCurrentCursor();

    if (! cursor)
        return NS_ERROR_UNEXPECTED;

    return cursor->GetPredicate(aPredicate);
}

NS_IMETHODIMP
SimpleDBAssertionCursorImpl::GetObject(nsIRDFNode** aObject)
{
    nsIRDFAssertionCursor* cursor =
        (nsIRDFAssertionCursor*) GetCurrentCursor();

    if (! cursor)
        return NS_ERROR_UNEXPECTED;

    return cursor->GetObject(aObject);
}

NS_IMETHODIMP
SimpleDBAssertionCursorImpl::GetTruthValue(PRBool* aTruthValue)
{
    nsIRDFAssertionCursor* cursor =
        (nsIRDFAssertionCursor*) GetCurrentCursor();

    if (! cursor)
        return NS_ERROR_UNEXPECTED;

    return cursor->GetTruthValue(aTruthValue);
}


////////////////////////////////////////////////////////////////////////
// SimpleDBArcsOutCursorImpl

class SimpleDBArcsOutCursorImpl : public MultiCursor,
                                  public nsIRDFArcsOutCursor
{
private:
    nsIRDFResource* mSource;

public:
    SimpleDBArcsOutCursorImpl(nsVoidArray& dataSources, nsIRDFResource* source);

    virtual ~SimpleDBArcsOutCursorImpl();

    // MultiCursor protocol methods
    virtual nsresult
    GetCursor(nsIRDFDataSource* ds, nsIRDFCursor** result);

    virtual nsresult
    IsCurrentNegatedBy(nsIRDFDataSource* ds0,
                       PRBool* result);


    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIRDFArcsOutCursor interface
    NS_IMETHOD Advance(void);
    NS_IMETHOD GetDataSource(nsIRDFDataSource** aDataSource);
    NS_IMETHOD GetSubject(nsIRDFResource** aSubject);
    NS_IMETHOD GetPredicate(nsIRDFResource** aPredicate);
    NS_IMETHOD GetTruthValue(PRBool* aTruthValue);
};

SimpleDBArcsOutCursorImpl::SimpleDBArcsOutCursorImpl(nsVoidArray& dataSources,
                                 nsIRDFResource* source)
    : MultiCursor(dataSources),
      mSource(source)
{
    NS_IF_ADDREF(mSource);
}


SimpleDBArcsOutCursorImpl::~SimpleDBArcsOutCursorImpl(void)
{
    NS_IF_RELEASE(mSource);
}


nsresult
SimpleDBArcsOutCursorImpl::GetCursor(nsIRDFDataSource* ds, nsIRDFCursor** result)
{
    return ds->ArcLabelsOut(mSource, (nsIRDFArcsOutCursor**) result);
}

nsresult
SimpleDBArcsOutCursorImpl::IsCurrentNegatedBy(nsIRDFDataSource* ds0,
                                              PRBool* result)
{
    *result = PR_FALSE; // XXX always?
    return NS_OK;
}

NS_IMPL_ADDREF(SimpleDBArcsOutCursorImpl);
NS_IMPL_RELEASE(SimpleDBArcsOutCursorImpl);

NS_IMETHODIMP_(nsresult)
SimpleDBArcsOutCursorImpl::QueryInterface(REFNSIID iid, void** result) {
    if (! result)
        return NS_ERROR_NULL_POINTER;

    if (iid.Equals(kIRDFAssertionCursorIID) ||
        iid.Equals(kIRDFCursorIID) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsIRDFArcsOutCursor*, this);
        /* AddRef(); // not necessary */
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

NS_IMETHODIMP
SimpleDBArcsOutCursorImpl::Advance(void)
{
    return AdvanceImpl();
}


NS_IMETHODIMP
SimpleDBArcsOutCursorImpl::GetDataSource(nsIRDFDataSource** aDataSource)
{
    nsIRDFArcsOutCursor* cursor =
        (nsIRDFArcsOutCursor*) GetCurrentCursor();

    if (! cursor)
        return NS_ERROR_UNEXPECTED;

    return cursor->GetDataSource(aDataSource);
}


NS_IMETHODIMP
SimpleDBArcsOutCursorImpl::GetSubject(nsIRDFResource** aSubject)
{
    nsIRDFArcsOutCursor* cursor =
        (nsIRDFArcsOutCursor*) GetCurrentCursor();

    if (! cursor)
        return NS_ERROR_UNEXPECTED;

    return cursor->GetSubject(aSubject);
}


NS_IMETHODIMP
SimpleDBArcsOutCursorImpl::GetPredicate(nsIRDFResource** aPredicate)
{
    nsIRDFArcsOutCursor* cursor =
        (nsIRDFArcsOutCursor*) GetCurrentCursor();

    if (! cursor)
        return NS_ERROR_UNEXPECTED;

    return cursor->GetPredicate(aPredicate);
}


NS_IMETHODIMP
SimpleDBArcsOutCursorImpl::GetTruthValue(PRBool* aTruthValue)
{
    nsIRDFArcsOutCursor* cursor =
        (nsIRDFArcsOutCursor*) GetCurrentCursor();

    if (! cursor)
        return NS_ERROR_UNEXPECTED;

    return cursor->GetTruthValue(aTruthValue);
}



////////////////////////////////////////////////////////////////////////
// SimpleDataBaseImpl
// XXX rvg  --- shouldn't this take a char** argument indicating the data sources
// we want to aggregate?

class SimpleDataBaseImpl : public nsIRDFDataBase {
protected:
    nsVoidArray mDataSources;
    virtual ~SimpleDataBaseImpl(void);

public:
    SimpleDataBaseImpl(void);

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIRDFDataSource interface
    NS_IMETHOD Init(const char* uri);

    NS_IMETHOD GetSource(nsIRDFResource* property,
                         nsIRDFNode* target,
                         PRBool tv,
                         nsIRDFResource** source);

    NS_IMETHOD GetSources(nsIRDFResource* property,
                          nsIRDFNode* target,
                          PRBool tv,
                          nsIRDFAssertionCursor** sources);

    NS_IMETHOD GetTarget(nsIRDFResource* source,
                         nsIRDFResource* property,
                         PRBool tv,
                         nsIRDFNode** target);

    NS_IMETHOD GetTargets(nsIRDFResource* source,
                          nsIRDFResource* property,
                          PRBool tv,
                          nsIRDFAssertionCursor** targets);

    NS_IMETHOD Assert(nsIRDFResource* source, 
                      nsIRDFResource* property, 
                      nsIRDFNode* target,
                      PRBool tv);

    NS_IMETHOD Unassert(nsIRDFResource* source,
                        nsIRDFResource* property,
                        nsIRDFNode* target);

    NS_IMETHOD HasAssertion(nsIRDFResource* source,
                            nsIRDFResource* property,
                            nsIRDFNode* target,
                            PRBool tv,
                            PRBool* hasAssertion);

    NS_IMETHOD AddObserver(nsIRDFObserver* n);

    NS_IMETHOD RemoveObserver(nsIRDFObserver* n);

    NS_IMETHOD ArcLabelsIn(nsIRDFNode* node,
                           nsIRDFArcsInCursor** labels);

    NS_IMETHOD ArcLabelsOut(nsIRDFResource* source,
                            nsIRDFArcsOutCursor** labels);

    NS_IMETHOD Flush();

    // nsIRDFDataBase interface
    NS_IMETHOD AddDataSource(nsIRDFDataSource* source);
    NS_IMETHOD RemoveDataSource(nsIRDFDataSource* source);
};

////////////////////////////////////////////////////////////////////////


SimpleDataBaseImpl::SimpleDataBaseImpl(void)
{
    NS_INIT_REFCNT();
}


SimpleDataBaseImpl::~SimpleDataBaseImpl(void)
{
    for (PRInt32 i = mDataSources.Count() - 1; i >= 0; --i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);
        NS_IF_RELEASE(ds);
    }
}

////////////////////////////////////////////////////////////////////////
// nsISupports interface

NS_IMPL_ADDREF(SimpleDataBaseImpl);
NS_IMPL_RELEASE(SimpleDataBaseImpl);

NS_IMETHODIMP
SimpleDataBaseImpl::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    *result = nsnull;
    if (iid.Equals(kIRDFDataBaseIID) ||
        iid.Equals(kIRDFDataSourceIID) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsIRDFDataBase*, this);
        AddRef();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}



////////////////////////////////////////////////////////////////////////
// nsIRDFDataSource interface

NS_IMETHODIMP
SimpleDataBaseImpl::Init(const char* uri)
{
    PR_ASSERT(0);
    return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
SimpleDataBaseImpl::GetSource(nsIRDFResource* property,
                            nsIRDFNode* target,
                            PRBool tv,
                            nsIRDFResource** source)
{
    PRInt32 count = mDataSources.Count();
    for (PRInt32 i = 0; i < count; ++i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);

        if (NS_FAILED(ds->GetSource(property, target, tv, source)))
            continue;

        // okay, found it. make sure we don't have the opposite
        // asserted in the "local" data source
        nsIRDFDataSource* ds0 = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[0]);
        nsIRDFResource* tmp;
        if (NS_FAILED(ds->GetSource(property, target, !tv, &tmp)))
            return NS_OK;

        NS_RELEASE(tmp);
        NS_RELEASE(*source);
        return NS_ERROR_FAILURE;
    }

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SimpleDataBaseImpl::GetSources(nsIRDFResource* property,
                             nsIRDFNode* target,
                             PRBool tv,
                             nsIRDFAssertionCursor** sources)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SimpleDataBaseImpl::GetTarget(nsIRDFResource* source,
                            nsIRDFResource* property,
                            PRBool tv,
                            nsIRDFNode** target)
{
    PRInt32 count = mDataSources.Count();
    for (PRInt32 i = 0; i < count; ++i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);

        if (NS_FAILED(ds->GetTarget(source, property, tv, target)))
            continue;

        // okay, found it. make sure we don't have the opposite
        // asserted in the "local" data source
        nsIRDFDataSource* ds0 = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[0]);
        nsIRDFNode* tmp;
        if (NS_FAILED(ds0->GetTarget(source, property, !tv, &tmp)))
            return NS_OK;

        NS_RELEASE(tmp);
        NS_RELEASE(*target);
        return NS_ERROR_FAILURE;
    }

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SimpleDataBaseImpl::GetTargets(nsIRDFResource* source,
                               nsIRDFResource* property,
                               PRBool tv,
                               nsIRDFAssertionCursor** targets)
{
    if (! targets)
        return NS_ERROR_NULL_POINTER;

    nsIRDFAssertionCursor* result;
    result = new SimpleDBAssertionCursorImpl(mDataSources, source, property, tv);
    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(result);
    *targets = result;
    return NS_OK;
}

NS_IMETHODIMP
SimpleDataBaseImpl::Assert(nsIRDFResource* source, 
                           nsIRDFResource* property, 
                           nsIRDFNode* target,
                           PRBool tv)
{
    nsresult rv;

    // First see if we just need to remove a negative assertion from ds0. (Sigh)
    nsIRDFDataSource* ds0 = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[0]);

    PRBool ds0HasNegation;
    if (NS_FAILED(rv = ds0->HasAssertion(source, property, target, !tv, &ds0HasNegation)))
        return rv;

    if (ds0HasNegation) {
        if (NS_FAILED(rv = ds0->Unassert(source, property, target)))
            return rv;
    }

    // Now, see if the assertion has been "unmasked"
    PRBool isAlreadyAsserted;
    if (NS_FAILED(rv = HasAssertion(source, property, target, tv, &isAlreadyAsserted)))
        return rv;

    if (isAlreadyAsserted)
        return NS_OK;

    // If not, iterate from the "remote-est" data source to the
    // "local-est", trying to make the assertion.
    for (PRInt32 i = mDataSources.Count() - 1; i >= 0; --i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);
        if (NS_SUCCEEDED(ds->Assert(source, property, target, tv)))
            return NS_OK;
    }

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SimpleDataBaseImpl::Unassert(nsIRDFResource* source,
                           nsIRDFResource* property,
                           nsIRDFNode* target)
{
    // XXX I have no idea what this is trying to do. I'm just going to
    // copy Guha's logic and punt.
    // xxx rvg - first need to check whether the data source does have the
    // assertion. Only then do you try to unassert it.
    nsresult rv;
    PRInt32 count = mDataSources.Count();

    for (PRInt32 i = 0; i < count; ++i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);
        if (NS_FAILED(rv = ds->Unassert(source, property, target)))
            break;
    }

    if (NS_FAILED(rv)) {
        nsIRDFDataSource* ds0 = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[0]);
        rv = ds0->Assert(source, property, target, PR_FALSE);
    }
    return rv;
}

NS_IMETHODIMP
SimpleDataBaseImpl::HasAssertion(nsIRDFResource* source,
                                 nsIRDFResource* property,
                                 nsIRDFNode* target,
                                 PRBool tv,
                                 PRBool* hasAssertion)
{
    nsresult rv;

    // First check to see if ds0 has the negation...
    nsIRDFDataSource* ds0 = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[0]);

    PRBool ds0HasNegation;
    if (NS_FAILED(rv = ds0->HasAssertion(source, property, target, !tv, &ds0HasNegation)))
        return rv;

    if (ds0HasNegation) {
        *hasAssertion = PR_FALSE;
        return NS_OK;
    }

    // Otherwise, look through all the data sources to see if anyone
    // has the positive...
    PRInt32 count = mDataSources.Count();
    for (PRInt32 i = 0; i < count; ++i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);
        if (NS_FAILED(rv = ds->HasAssertion(source, property, target, tv, hasAssertion)))
            return rv;

        if (hasAssertion)
            return NS_OK;
    }

    // If we get here, nobody had the assertion at all
    return NS_OK;
}

NS_IMETHODIMP
SimpleDataBaseImpl::AddObserver(nsIRDFObserver* n)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SimpleDataBaseImpl::RemoveObserver(nsIRDFObserver* n)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SimpleDataBaseImpl::ArcLabelsIn(nsIRDFNode* node,
                                nsIRDFArcsInCursor** labels)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SimpleDataBaseImpl::ArcLabelsOut(nsIRDFResource* source,
                                 nsIRDFArcsOutCursor** labels)
{
    if (! labels)
        return NS_ERROR_NULL_POINTER;

    nsIRDFArcsOutCursor* result = new SimpleDBArcsOutCursorImpl(mDataSources, source);
    if (! result)
        return NS_ERROR_NULL_POINTER;

    NS_ADDREF(result);
    *labels = result;
    return NS_OK;
}

NS_IMETHODIMP
SimpleDataBaseImpl::Flush()
{
    for (PRInt32 i = mDataSources.Count() - 1; i >= 0; --i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);
        ds->Flush();
    }
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsIRDFDataBase methods
// XXX rvg We should make this take an additional argument specifying where
// in the sequence of data sources (of the db), the new data source should
// fit in. Right now, the new datasource gets stuck at the end.

NS_IMETHODIMP
SimpleDataBaseImpl::AddDataSource(nsIRDFDataSource* source)
{
    if (! source)
        return NS_ERROR_NULL_POINTER;

    mDataSources.InsertElementAt(source, 0);
    NS_ADDREF(source);
    return NS_OK;
}



NS_IMETHODIMP
SimpleDataBaseImpl::RemoveDataSource(nsIRDFDataSource* source)
{
    if (! source)
        return NS_ERROR_NULL_POINTER;

    if (mDataSources.IndexOf(source) >= 0) {
        mDataSources.RemoveElement(source);
        NS_RELEASE(source);
    }
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////

nsresult
NS_NewRDFSimpleDataBase(nsIRDFDataBase** result)
{
    SimpleDataBaseImpl* db = new SimpleDataBaseImpl();
    if (! db)
        return NS_ERROR_OUT_OF_MEMORY;

    *result = db;
    NS_ADDREF(*result);
    return NS_OK;
}
