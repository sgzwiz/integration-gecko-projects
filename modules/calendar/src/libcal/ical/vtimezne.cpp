/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- 
 * 
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape 
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

// vtimezne.cpp
// John Sun
// 2:40 PM February 24 1998

#include "stdafx.h"
#include "jdefines.h"

#include "icalcomp.h"
#include "prprtyfy.h"
#include "vtimezne.h"
#include "jlog.h"
#include "jutility.h"
#include "keyword.h"
#include <simpletz.h>

t_bool VTimeZone::ms_bMORE_THAN_TWO_TZPARTS = FALSE;
//---------------------------------------------------------------------

// private never use.
#if 0
VTimeZone::VTimeZone()
{
    PR_ASSERT(FALSE);
}
#endif

//---------------------------------------------------------------------

VTimeZone::VTimeZone(JLog * initLog)
: m_TZID(0), m_LastModified(0), m_TZURL(0),
  m_XTokensVctr(0),
  m_TZPartVctr(0), m_NLSTimeZone(0),
  m_Log(initLog)
{
    //PR_ASSERT(initLog != 0);
}
//---------------------------------------------------------------------
VTimeZone::VTimeZone(VTimeZone & that)
: m_TZID(0), m_LastModified(0), m_TZURL(0),
  m_XTokensVctr(0), 
  m_TZPartVctr(0), m_NLSTimeZone(0)
{
    if (that.m_NLSTimeZone != 0)
    {
        // note: todo: this should be an OK cast,
        // but technically it's a bad thing to do.
        m_NLSTimeZone = (SimpleTimeZone *) (that.m_NLSTimeZone)->clone();
    }

    if (that.m_TZID != 0) 
    { 
        m_TZID = that.m_TZID->clone(m_Log); 
    }
    if (that.m_LastModified != 0) 
    { 
        m_LastModified = that.m_LastModified->clone(m_Log); 
    }
    if (that.m_TZURL != 0) 
    {
        m_TZURL = that.m_TZURL->clone(m_Log); 
    }
    if (that.m_XTokensVctr != 0)
    {
        m_XTokensVctr = new JulianPtrArray(); PR_ASSERT(m_XTokensVctr != 0);
        if (m_XTokensVctr != 0)
        {
            ICalProperty::CloneUnicodeStringVector(that.m_XTokensVctr, m_XTokensVctr);
        }
    }
    if (that.m_TZPartVctr != 0)
    {
        t_int32 i;
        ICalComponent * tzclone;
        ICalComponent * ip;
        m_TZPartVctr = new JulianPtrArray(); PR_ASSERT(m_TZPartVctr != 0);
        if (m_TZPartVctr != 0)
        {
            for (i = 0; i < that.m_TZPartVctr->GetSize(); i++)
            {
                ip = (ICalComponent *) that.m_TZPartVctr->GetAt(i);
                tzclone = ip->clone(m_Log);
                PR_ASSERT(tzclone != 0);
                if (tzclone != 0)
                {
                    m_TZPartVctr->Add(tzclone);
                }
            }
        }
    }
}
//---------------------------------------------------------------------
ICalComponent *
VTimeZone::clone(JLog * initLog)
{
    m_Log = initLog; 
    //PR_ASSERT(m_Log != 0);
    return new VTimeZone(*this);
}
//---------------------------------------------------------------------

VTimeZone::~VTimeZone()
{
    if (m_TZPartVctr != 0)
    {
        ICalComponent::deleteICalComponentVector(m_TZPartVctr); 
        delete m_TZPartVctr; m_TZPartVctr = 0;
    }
    if (m_LastModified != 0) 
    { 
        delete m_LastModified; m_LastModified = 0; 
    }
    if (m_TZID != 0) 
    { 
        delete m_TZID; m_TZID = 0; 
    }
    if (m_TZURL != 0) 
    { 
        delete m_TZURL; m_TZURL = 0; 
    }
    if (m_XTokensVctr != 0) 
    { 
        ICalComponent::deleteUnicodeStringVector(m_XTokensVctr);
        delete m_XTokensVctr; m_XTokensVctr = 0; 
    }
    // NOTE: verify this
    if (m_NLSTimeZone != 0) 
    { 
        delete m_NLSTimeZone; m_NLSTimeZone = 0;
    }
}

//---------------------------------------------------------------------
UnicodeString &
VTimeZone::parse(ICalReader * brFile, UnicodeString & sType,
                 UnicodeString & parseStatus, JulianPtrArray * vTimeZones,
                 t_bool bIgnoreBeginError, JulianUtility::MimeEncoding encoding)
{
    UnicodeString strLine, propName, propVal;
    parseStatus = JulianKeyword::Instance()->ms_sOK;
    JulianPtrArray * parameters = new JulianPtrArray();
    PR_ASSERT(parameters != 0 && brFile != 0);
    if (parameters == 0 || brFile == 0)
    {
        // ran out of memory, return invalid vtimezone
        return parseStatus;
    }

    ErrorCode status = ZERO_ERROR;   
    t_bool parseError = FALSE;
    //PR_ASSERT(vTimeZones == 0);

    // NOTE: remove later, to avoid compiler warning
    if (sType.size() > 0 && vTimeZones) {}

    while (TRUE)
    {
        PR_ASSERT(brFile != 0);
        brFile->readFullLine(strLine, status);
        ICalProperty::Trim(strLine);

        if (FAILURE(status) && strLine.size() == 0)
            break;
        
        ICalProperty::parsePropertyLine(strLine, propName, propVal, parameters);
       
        if (strLine.size() == 0)
        {
            ICalProperty::deleteICalParameterVector(parameters);
            parameters->RemoveAll();
            
            continue;
        }
        else if ((propName.compareIgnoreCase(JulianKeyword::Instance()->ms_sEND) == 0) &&
                 (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVTIMEZONE) == 0))
        {
             ICalProperty::deleteICalParameterVector(parameters);
             parameters->RemoveAll();

             break;            
        }
        else if (((propName.compareIgnoreCase(JulianKeyword::Instance()->ms_sEND) == 0) &&
                   ((propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVCALENDAR) == 0) ||
                   (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVEVENT) == 0) ||
                   (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVTODO) == 0) ||
                   (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVJOURNAL) == 0) ||
                   (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVFREEBUSY) == 0) ||
                   (ICalProperty::IsXToken(propVal)))
                   ) ||
                  ((propName.compareIgnoreCase(JulianKeyword::Instance()->ms_sBEGIN) == 0) &&
                   ((propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVTIMEZONE) == 0) && !bIgnoreBeginError) || 
                   (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVEVENT) == 0) ||
                   (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVTODO) == 0) ||
                   (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVJOURNAL) == 0) ||
                   (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVFREEBUSY) == 0) ||
                   (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVCALENDAR) == 0) ||
                   (ICalProperty::IsXToken(propVal))
                  ))
        
        {
            // abrupt break of parsing
            // Break on END:VCALENDAR, VEVENT, VTODO, VJOURNAL, VFREEBUSY, x-token, 
            // break on BEGIN:VTIMEZONE (and not first BEGIN:VTIMEZONE)
            // break on BEGIN:VEVENT, VTODO, VJOURNAL, VFREEBUSY, VCALENDAR, xtoken
            ICalProperty::deleteICalParameterVector(parameters);
            parameters->RemoveAll();

            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iAbruptEndOfParsing, 
                JulianKeyword::Instance()->ms_sVTIMEZONE, strLine, 300);
           
            parseStatus = strLine;
            break;            
        }
        else if ((propName.compareIgnoreCase(JulianKeyword::Instance()->ms_sBEGIN) == 0) &&
                 ((propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sDAYLIGHT) == 0) ||
                 (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sSTANDARD) == 0)))
        {
            // Add the DAYLIGHT, STANDARD parts 
            ICalProperty::deleteICalParameterVector(parameters);
            parameters->RemoveAll();

            TZPart * tzpart = new TZPart(m_Log);
            PR_ASSERT(tzpart != 0);
            if (tzpart != 0)
            {
                tzpart->parse(brFile, propVal, parseStatus, 0);
                
                if (!tzpart->isValid())
                {
                    if (m_Log) m_Log->logError(
                        JulianLogErrorMessage::Instance()->ms_iInvalidTZPart, 200);
                    delete tzpart; tzpart = 0;
                }
                else
                {
                    addTZPart(tzpart);
                }
            }
        }
        else
        {
            storeData(strLine, propName, propVal, parameters);
            ICalProperty::deleteICalParameterVector(parameters);
            parameters->RemoveAll();
        }
    }
    ICalProperty::deleteICalParameterVector(parameters);
    parameters->RemoveAll();
    delete parameters; parameters = 0;

    selfCheck();
    return parseStatus;
}
//---------------------------------------------------------------------
void VTimeZone::storeData(UnicodeString & strLine, UnicodeString & propName,
                          UnicodeString & propVal, JulianPtrArray * parameters)
{

    if (strLine.size() > 0) 
    {
    } 
    //UnicodeString u;
    t_int32 hashCode = propName.hashCode();
    //if (propName.compareIgnoreCase(JulianKeyword::Instance()->ms_sLASTMODIFIED) == 0)
    
    if (JulianKeyword::Instance()->ms_ATOM_LASTMODIFIED == hashCode)
    {
        // no parameters
        if (parameters->GetSize() > 0)
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sVTIMEZONE, strLine, 100);
        }

        if (getLastModifiedProperty() != 0)
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iDuplicatedProperty, 
                JulianKeyword::Instance()->ms_sVTIMEZONE, propName, 100);
        }
        DateTime d(propVal);
        setLastModified(d, parameters);
    }
    else if (JulianKeyword::Instance()->ms_ATOM_TZID == hashCode)
    {
        // no parameters
        if (parameters->GetSize() > 0)
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sVTIMEZONE, strLine, 100);
        }

        if (getTZIDProperty() != 0)
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iDuplicatedProperty, 
                JulianKeyword::Instance()->ms_sVTIMEZONE, propName, 100);
        }
        setTZID(propVal, parameters);
    }
    else if (JulianKeyword::Instance()->ms_ATOM_TZURL == hashCode)
    {
        // no parameters
        if (parameters->GetSize() > 0)
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sVTIMEZONE, strLine, 100);
        }

        if (getTZURLProperty() != 0)
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iDuplicatedProperty, 
                JulianKeyword::Instance()->ms_sVTIMEZONE, propName, 100);
        }
        setTZURL(propVal, parameters);
    }
    else if (ICalProperty::IsXToken(propName))
    {
        addXTokens(strLine);
    }
    else 
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iInvalidPropertyName, 
            JulianKeyword::Instance()->ms_sVTIMEZONE, propName, 200);
    }
}
//---------------------------------------------------------------------

void VTimeZone::selfCheck()
{
    if (isValid())
    {
        // Make the NLS timezone part
        createNLSTimeZone();
    }
}

//---------------------------------------------------------------------

void VTimeZone::createNLSTimeZone()
{
    float f1, f2;
    t_int32 toOffset;
    UnicodeString u;

    if ((0 != getStandardPart()) && 0 != getDaylightPart())
    {
        // create a NLSTimeZone with daylight and standard part
        u = getStandardPart()->getTZOffsetTo();
        f1 = TZPart::UTCOffsetToFloat(u);
        u = getDaylightPart()->getTZOffsetTo();
        f2 = TZPart::UTCOffsetToFloat(u);

        toOffset = (t_int32) ((f2 - f1) * kMillisPerHour);

        if (m_NLSTimeZone != 0)
        {
            delete m_NLSTimeZone; m_NLSTimeZone = 0;
        }
        m_NLSTimeZone = new SimpleTimeZone((t_int32) (f1 * kMillisPerHour), getTZID(), 
            (t_int8) getDaylightPart()->getMonth(), 
            (t_int8) getDaylightPart()->getDayOfWeekInMonth(), 
            (t_int8) getDaylightPart()->getDay(), 
            (t_int32) getDaylightPart()->getStartTime(),
            (t_int8) getStandardPart()->getMonth(), 
            (t_int8) getStandardPart()->getDayOfWeekInMonth(),  
            (t_int8) getStandardPart()->getDay(), 
            (t_int32) getStandardPart()->getStartTime(), 
            (t_int32) toOffset);

        PR_ASSERT(m_NLSTimeZone != 0);
        if (m_NLSTimeZone != 0)
        {
            m_NLSTimeZone->setRawOffset((t_int32) (f1 * kMillisPerHour));
            m_NLSTimeZone->setID(getTZID());

            m_NLSTimeZone->setStartRule(getDaylightPart()->getMonth(), getDaylightPart()->getDayOfWeekInMonth(), 
                getDaylightPart()->getDay(), getDaylightPart()->getStartTime());
   
            m_NLSTimeZone->setEndRule(getStandardPart()->getMonth(), getStandardPart()->getDayOfWeekInMonth(),  
                getStandardPart()->getDay(), getStandardPart()->getStartTime());
        }
    }
    else
    {
        PR_ASSERT(m_TZPartVctr != 0 && m_TZPartVctr->GetSize() > 0);
        if (m_TZPartVctr != 0 && m_TZPartVctr->GetSize() > 0)
        {
            // use first tzpart to create a single part timezone,
            TZPart * tz = (TZPart *) m_TZPartVctr->GetAt(0);
            u = tz->getTZOffsetTo();
            f1 = TZPart::UTCOffsetToFloat(u);
            
            if (m_NLSTimeZone != 0)
            {
                delete m_NLSTimeZone; m_NLSTimeZone = 0;
            }
            m_NLSTimeZone = new SimpleTimeZone((t_int32) (f1 * kMillisPerHour), getTZID());
            PR_ASSERT(m_NLSTimeZone != 0);
        }
    }
}

//---------------------------------------------------------------------

t_bool VTimeZone::isValid()
{
    // TODO: log invalid ID, Daylight, Standard.
    /* Must have tzid, standard || daylight part && at most 2 parts. */

    if (0 == getTZID().size())
        return FALSE;

    // if no parts, return FALSE
    if ((0 == getDaylightPart()) && (0 == getStandardPart()))
        return FALSE;

    if (!ms_bMORE_THAN_TWO_TZPARTS)
    {
        if (0 != getTZParts())
        {
            if (getTZParts()->GetSize() > 2)
                return FALSE;
        }
    }
    return TRUE;
}

//---------------------------------------------------------------------
UnicodeString VTimeZone::toICALString()
{
    UnicodeString u = JulianKeyword::Instance()->ms_sVTIMEZONE;
    return ICalComponent::format(u, JulianFormatString::Instance()->ms_sVTimeZoneAllMessage, "");
}
//---------------------------------------------------------------------
UnicodeString VTimeZone::toICALString(UnicodeString method,
                                      UnicodeString name,
                                      t_bool isRecurring)
{
    // NOTE: remove later avoid warnings
    if (isRecurring) {}
    return toICALString();
}
//---------------------------------------------------------------------
UnicodeString VTimeZone::toString()
{
    return ICalComponent::toStringFmt(
        JulianFormatString::Instance()->ms_VTimeZoneStrDefaultFmt);
}
//---------------------------------------------------------------------
UnicodeString VTimeZone::toStringChar(t_int32 c, UnicodeString & dateFmt)
{
    UnicodeString s;
    t_int32 i;
    TZPart * tzp;
    
    switch (c)
    {
    case ms_cTZParts:
        if (getTZParts() != 0)
        {
            for (i = 0; i < getTZParts()->GetSize(); i++)
            {
                tzp = (TZPart *) getTZParts()->GetAt(i);
                s += tzp->toString();
            }
        }
        return s;
    case ms_cLastModified:
        return ICalProperty::propertyToString(getLastModifiedProperty(), dateFmt, s); 
    case ms_cTZID:
        return ICalProperty::propertyToString(getTZIDProperty(), dateFmt, s); 
    case ms_cTZURL:
        return ICalProperty::propertyToString(getTZURLProperty(), dateFmt, s);
    default:
        return "";
    }
}
//---------------------------------------------------------------------
UnicodeString VTimeZone::formatChar(t_int32 c, UnicodeString sFilterAttendee,
                                    t_bool delegateRequest)
{
    UnicodeString s, sResult;
    t_int32 i;
    TZPart * tzp;

    // NOTE: remove here is get rid of compiler warnings
    if (sFilterAttendee.size() > 0 || delegateRequest)
    {
    }

    switch (c)
    {
    case ms_cTZParts:
        if (getTZParts() != 0)
        {
            for (i = 0; i < getTZParts()->GetSize(); i++)
            {
                tzp = (TZPart *) getTZParts()->GetAt(i);
                s += tzp->toICALString();
            }
        }
        return s;
    case ms_cLastModified:
        s = JulianKeyword::Instance()->ms_sLASTMODIFIED;
        return ICalProperty::propertyToICALString(s, getLastModifiedProperty(), sResult); 
    case ms_cTZID:
        s = JulianKeyword::Instance()->ms_sTZID;
        return ICalProperty::propertyToICALString(s, getTZIDProperty(), sResult); 
    case ms_cTZURL:
        s = JulianKeyword::Instance()->ms_sTZURL;
        return ICalProperty::propertyToICALString(s, getTZURLProperty(), sResult);
    case ms_cXTokens: 
        return ICalProperty::vectorToICALString(getXTokens(), sResult);
    default:
        return "";
    }
}
//---------------------------------------------------------------------

void 
VTimeZone::updateComponentHelper(VTimeZone * updatedComponent)
{
    DateTime d;
    // no need: last-modified, TZID
    ICalComponent::internalSetXTokensVctr(&m_XTokensVctr, updatedComponent->m_XTokensVctr);

    // call updateComponentHelper on TZPart's
    ICalComponent::internalSetProperty(&m_TZURL, updatedComponent->m_TZURL);
    setLastModified(d);
    if (m_TZPartVctr != 0 && updatedComponent->getTZParts() != 0)
    {
        t_int32 i, j;
        TZPart * tzp;
        TZPart * uctzp;
        for (i = 0; i < m_TZPartVctr->GetSize(); i++)
        {
            tzp = (TZPart *) m_TZPartVctr->GetAt(i);
            for (j = 0; j < updatedComponent->getTZParts()->GetSize(); j++)
            {
                uctzp = (TZPart *) updatedComponent->getTZParts()->GetAt(j);
                                
                tzp->updateComponent(uctzp);
            }
        }   
    }
}
//---------------------------------------------------------------------

t_bool
VTimeZone::updateComponent(ICalComponent * updatedComponent)
{
    if (updatedComponent != 0)
    {
        ICAL_COMPONENT ucType = updatedComponent->GetType();

        // only call updateComponentHelper if it's a VTimeZone and
        // it is an exact matching TZID
        // basically always overwrite
        if (ucType == ICAL_COMPONENT_VTIMEZONE)
        {
            // should be a safe cast with check above.
            VTimeZone * ucvtz = (VTimeZone *) updatedComponent;

            // only if TZID's match and are not empty
            if (ucvtz->getTZID().size() > 0 && getTZID().size() > 0)
            {
                if (ucvtz->getTZID() == getTZID())
                {
                    updateComponentHelper(ucvtz);
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}
//---------------------------------------------------------------------
void VTimeZone::addTZPart(TZPart * part)
{
    if (m_TZPartVctr == 0)
        m_TZPartVctr = new JulianPtrArray();
    PR_ASSERT(m_TZPartVctr != 0);
    PR_ASSERT(part != 0);
    if (m_TZPartVctr != 0 && part != 0)
    {
        m_TZPartVctr->Add(part);
    }
}
//---------------------------------------------------------------------
TZPart * VTimeZone::getPart(UnicodeString & u)
{
    t_int32 i;
    TZPart * tzp;
    if (getTZParts() != 0)
    {
        for (i = 0; i < getTZParts()->GetSize(); i++)
        {
            tzp = (TZPart *) getTZParts()->GetAt(i);
            if (tzp->getName().compareIgnoreCase(u) == 0)
            {
                return tzp;
            }
        }
    }
    return 0;
}
//---------------------------------------------------------------------
TZPart * VTimeZone::getStandardPart()
{
    UnicodeString u = JulianKeyword::Instance()->ms_sSTANDARD;
    return getPart(u);
}
//---------------------------------------------------------------------
TZPart * VTimeZone::getDaylightPart()
{
    UnicodeString u = JulianKeyword::Instance()->ms_sDAYLIGHT;
    return getPart(u);
}
//---------------------------------------------------------------------
VTimeZone * VTimeZone::getTimeZone(UnicodeString & id,
                                   JulianPtrArray * timezones)
{
    t_int32 i;
    VTimeZone * vt;
    if (timezones != 0)
    {
        for (i = 0;  i < timezones->GetSize(); i++)
        {
            vt = (VTimeZone *) timezones->GetAt(i);
            if (id.compareIgnoreCase(vt->getTZID()) == 0)
            {
                return vt;
            }
        }
    
    }
    return 0;
}
//---------------------------------------------------------------------
//LAST-MODIFIED
void VTimeZone::setLastModified(DateTime s, JulianPtrArray * parameters)
{ 
#if 1
    if (m_LastModified == 0)
        m_LastModified = ICalPropertyFactory::Make(ICalProperty::DATETIME, 
                                            (void *) &s, parameters);
    else
    {
        m_LastModified->setValue((void *) &s);
        m_LastModified->setParameters(parameters);
    }
#else
    ICalComponent::setDateTimeValue(((ICalProperty **) &m_LastModified), s, parameters);
#endif
}

DateTime VTimeZone::getLastModified() const
{
#if 1
    DateTime d(-1);
    if (m_LastModified == 0)
        return d; // return 0;
    else
    {
        d = *((DateTime *) m_LastModified->getValue());
        return d;
    }
#else
    DateTime d(-1);
    ICalComponent::getDateTimeValue(((ICalProperty **) &m_LastModified), d);
    return d;
#endif
}
//---------------------------------------------------------------------
//TZID
void VTimeZone::setTZID(UnicodeString s, JulianPtrArray * parameters)
{
#if 1
    //UnicodeString * s_ptr = new UnicodeString(s);
    //PR_ASSERT(s_ptr != 0);

    if (m_TZID == 0)
        m_TZID = ICalPropertyFactory::Make(ICalProperty::TEXT, 
                                            (void *) &s, parameters);
    else
    {
        m_TZID->setValue((void *) &s);
        m_TZID->setParameters(parameters);
    }
#else
    ICalComponent::setStringValue(((ICalProperty **) &m_TZID), s, parameters);
#endif
}
UnicodeString VTimeZone::getTZID() const 
{
#if 1
    UnicodeString u;
    if (m_TZID == 0)
        return "";
    else
    {
        u = *((UnicodeString *) m_TZID->getValue());
        return u;
    }
#else
    UnicodeString us;
    ICalComponent::getStringValue(((ICalProperty **) &m_TZID), us);
    return us;
#endif
}
//---------------------------------------------------------------------
//TZURL
void VTimeZone::setTZURL(UnicodeString s, JulianPtrArray * parameters)
{
#if 1
    //UnicodeString * s_ptr = new UnicodeString(s);
    //PR_ASSERT(s_ptr != 0);

    if (m_TZURL == 0)
        m_TZURL = ICalPropertyFactory::Make(ICalProperty::TEXT, 
                                            (void *) &s, parameters);
    else
    {
        m_TZURL->setValue((void *) &s);
        m_TZURL->setParameters(parameters);
    }
#else
    ICalComponent::setStringValue(((ICalProperty **) &m_TZURL), s, parameters);
#endif
}
UnicodeString VTimeZone::getTZURL() const 
{
#if 1
    UnicodeString u;
    if (m_TZURL == 0)
        return "";
    else
    {
        u = *((UnicodeString *) m_TZURL->getValue());
        return u;
    }
#else
    UnicodeString us;
    ICalComponent::getStringValue(((ICalProperty **) &m_TZID), us);
    return us;
#endif
}
//---------------------------------------------------------------------
DateTime 
VTimeZone::DateTimeApplyTimeZone(UnicodeString & time,
                                 JulianPtrArray * vTimeZones,
                                 JulianPtrArray * parameters)
{
    DateTime d;
    UnicodeString out, u;
    VTimeZone * vt = 0;
    TimeZone *t = 0;
    u = JulianKeyword::Instance()->ms_sTZID;
    out = ICalParameter::GetParameterFromVector(u, out, parameters);

    //if (FALSE) TRACE("out = %s\r\n", out.toCString(""));
    if (out.size() > 0)
    {
        vt = VTimeZone::getTimeZone(out, vTimeZones);
        if (vt != 0)
            t = vt->getNLSTimeZone();    
    }
    if (t != 0)
        d.setTimeString(time, t);
    else
        d.setTimeString(time);

    return d;
}
//---------------------------------------------------------------------
// XTOKENS
void VTimeZone::addXTokens(UnicodeString s)         
{
    if (m_XTokensVctr == 0)
        m_XTokensVctr = new JulianPtrArray(); 
    PR_ASSERT(m_XTokensVctr != 0);
    if (m_XTokensVctr != 0)
    {
        m_XTokensVctr->Add(new UnicodeString(s));
    }
}
//---------------------------------------------------------------------
