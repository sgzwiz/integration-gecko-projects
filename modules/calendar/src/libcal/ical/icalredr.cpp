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

// icalredr.cpp
// John Sun
// 3/23/98 3:29:47 PM

#include "stdafx.h"
#include "icalredr.h"

//---------------------------------------------------------------------

ICalReader::ICalReader() {}

//---------------------------------------------------------------------

t_bool ICalReader::isHex(t_int8 aToken)
{
    if (aToken >= '0' && aToken <= '9')
        return TRUE;
    else if (aToken >= 'A' && aToken <= 'F')
        return TRUE;
    else if (aToken >= 'a' && aToken <= 'f')
        return TRUE;
    else 
        return FALSE;
}

//---------------------------------------------------------------------

t_int8 ICalReader::convertHex(char fToken, 
                              char sToken)
{
	unsigned char c = 0;
	if (fToken >= '0' && fToken <= '9')
        c = fToken - '0';
	else if (fToken >= 'A' && fToken <= 'F')
        c = fToken - ('A' - 10);
    //else if (fToken >= 'a' && fToken <= 'f')
    else
        c = fToken - ('a' - 10);

	/* Second hex digit */
	c = (c << 4);
	if (sToken >= '0' && sToken <= '9')
	    c += sToken - '0';
	else if (sToken >= 'A' && sToken <= 'F')
	    c += sToken - ('A' - 10);
	//else (sToken >= 'a' && sToken <= 'f')
	else
        c += sToken - ('a' - 10);

    return (t_int8) c;
}

//---------------------------------------------------------------------
