/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/* From JSRef modified for Classilla by Cameron Kaiser
   THIS IS NOT SUITABLE FOR WIN32 */

/*
 * PR time code.
 */
#include "jsstddef.h"
#ifdef SOLARIS
#define _REENTRANT 1
#endif
#include <string.h>
#include <time.h>
#include "jstypes.h"
#include "jsutil.h"

#include "jsprf.h"
#include "prmjtime.h"

#define PRMJ_DO_MILLISECONDS 1

#ifdef XP_OS2
#include <sys/timeb.h>
#endif
#ifdef XP_WIN
#include <WINDEF.H>
#include <WINBASE.H>
#endif

#ifdef XP_MAC
#include <OSUtils.h>
#include <TextUtils.h>
#include <Resources.h>
#include <Timer.h>
#include <UTCUtils.h>
#include <Power.h>
#include <CodeFragments.h>
#if !TARGET_CARBON
#include <Traps.h>
#endif
#endif

#if defined(XP_UNIX) || defined(XP_BEOS)

#ifdef _SVID_GETTOD   /* Defined only on Solaris, see Solaris <sys/types.h> */
extern int gettimeofday(struct timeval *tv);
#endif

#include <sys/time.h>

#endif /* XP_UNIX */

#ifdef XP_MAC
static uint64 			 dstLocalBaseMicroseconds;
static unsigned long	 gJanuaryFirst1970Seconds;

static void MacintoshInitializeTime(void)
{
    uint64					upTime;
    unsigned long			currentLocalTimeSeconds,
	   startupTimeSeconds;
    uint64				startupTimeMicroSeconds;
    uint32				upTimeSeconds;
    uint64				oneMillion, upTimeSecondsLong, microSecondsToSeconds;
    DateTimeRec				firstSecondOfUnixTime;

    /*
     * Figure out in local time what time the machine started up. This information can be added to
     * upTime to figure out the current local time as well as GMT.
     */

    Microseconds((UnsignedWide*)&upTime);

    GetDateTime(&currentLocalTimeSeconds);

    JSLL_I2L(microSecondsToSeconds, PRMJ_USEC_PER_SEC);
    JSLL_DIV(upTimeSecondsLong, upTime, microSecondsToSeconds);
    JSLL_L2I(upTimeSeconds, upTimeSecondsLong);

    startupTimeSeconds = currentLocalTimeSeconds - upTimeSeconds;

    /*  Make sure that we normalize the macintosh base seconds to the unix base of January 1, 1970.
     */

    firstSecondOfUnixTime.year = 1970;
    firstSecondOfUnixTime.month = 1;
    firstSecondOfUnixTime.day = 1;
    firstSecondOfUnixTime.hour = 0;
    firstSecondOfUnixTime.minute = 0;
    firstSecondOfUnixTime.second = 0;
    firstSecondOfUnixTime.dayOfWeek = 0;

    DateToSeconds(&firstSecondOfUnixTime, &gJanuaryFirst1970Seconds);

    startupTimeSeconds -= gJanuaryFirst1970Seconds;

    /*  Now convert the startup time into a wide so that we can figure out GMT and DST.
     */

    JSLL_I2L(startupTimeMicroSeconds, startupTimeSeconds);
    JSLL_I2L(oneMillion, PRMJ_USEC_PER_SEC);
    JSLL_MUL(dstLocalBaseMicroseconds, oneMillion, startupTimeMicroSeconds);
}

static SleepQRec  gSleepQEntry = { NULL, sleepQType, NULL, 0 };
static JSBool     gSleepQEntryInstalled = JS_FALSE;

static pascal long MySleepQProc(long message, SleepQRecPtr sleepQ)
{
    /* just woke up from sleeping, so must recompute dstLocalBaseMicroseconds. */
    if (message == kSleepWakeUp)
        MacintoshInitializeTime();
    return 0;
}

/* Because serial port and SLIP conflict with ReadXPram calls,
 * we cache the call here
 */

static void MyReadLocation(MachineLocation * loc)
{
    static MachineLocation storedLoc;	/* InsideMac, OSUtilities, page 4-20 */
    static JSBool didReadLocation = JS_FALSE;
    if (!didReadLocation)
    {
        MacintoshInitializeTime();
        ReadLocation(&storedLoc);
        /* install a sleep queue routine, so that when the machine wakes up, time can be recomputed. */
        if (&SleepQInstall != (void*)kUnresolvedCFragSymbolAddress
#if !TARGET_CARBON
            && NGetTrapAddress(0xA28A, OSTrap) != NGetTrapAddress(_Unimplemented, ToolTrap)
#endif
           ) {
            if ((gSleepQEntry.sleepQProc = NewSleepQUPP(MySleepQProc)) != NULL) {
                SleepQInstall(&gSleepQEntry);
                gSleepQEntryInstalled = JS_TRUE;
            }
        }
        didReadLocation = JS_TRUE;
     }
     *loc = storedLoc;
}


#ifndef XP_MACOSX

/* CFM library init and terminate routines. We'll use the terminate routine
   to clean up the sleep Q entry. On Mach-O, the sleep Q entry gets cleaned
   up for us, so nothing to do there.
*/

extern pascal OSErr __NSInitialize(const CFragInitBlock* initBlock);
extern pascal void __NSTerminate();

pascal OSErr __JSInitialize(const CFragInitBlock* initBlock);
pascal void __JSTerminate(void);

pascal OSErr __JSInitialize(const CFragInitBlock* initBlock)
{
	return __NSInitialize(initBlock);
}

pascal void __JSTerminate()
{
  /* clean up the sleepQ entry */
  if (gSleepQEntryInstalled)
    SleepQRemove(&gSleepQEntry);
  
	__NSTerminate();
}
#endif /* XP_MACOSX */

#endif /* XP_MAC */

#define PRMJ_YEAR_DAYS 365L
#define PRMJ_FOUR_YEARS_DAYS (4 * PRMJ_YEAR_DAYS + 1)
#define PRMJ_CENTURY_DAYS (25 * PRMJ_FOUR_YEARS_DAYS - 1)
#define PRMJ_FOUR_CENTURIES_DAYS (4 * PRMJ_CENTURY_DAYS + 1)
#define PRMJ_HOUR_SECONDS  3600L
#define PRMJ_DAY_SECONDS  (24L * PRMJ_HOUR_SECONDS)
#define PRMJ_YEAR_SECONDS (PRMJ_DAY_SECONDS * PRMJ_YEAR_DAYS)
#define PRMJ_MAX_UNIX_TIMET 2145859200L /*time_t value equiv. to 12/31/2037 */

/* function prototypes */
static void PRMJ_basetime(JSInt64 tsecs, PRMJTime *prtm);
/*
 * get the difference in seconds between this time zone and UTC (GMT)
 */
JSInt32
PRMJ_LocalGMTDifference()
{
#if defined(XP_UNIX) || defined(XP_PC) || defined(XP_BEOS)
    struct tm ltime;

    /* get the difference between this time zone and GMT */
    memset((char *)&ltime,0,sizeof(ltime));
    ltime.tm_mday = 2;
    ltime.tm_year = 70;
#ifdef SUNOS4
    ltime.tm_zone = 0;
    ltime.tm_gmtoff = 0;
    return timelocal(&ltime) - (24 * 3600);
#else
    return mktime(&ltime) - (24L * 3600L);
#endif
#endif
#if defined(XP_MAC)
    static JSInt32   zone = -1L;
    MachineLocation  machineLocation;
    JSInt32 	     gmtOffsetSeconds;

    /* difference has been set no need to recalculate */
    if (zone != -1)
        return zone;

    /* Get the information about the local machine, including
     * its GMT offset and its daylight savings time info.
     * Convert each into wides that we can add to
     * startupTimeMicroSeconds.
     */

    MyReadLocation(&machineLocation);

    /* Mask off top eight bits of gmtDelta, sign extend lower three. */
    gmtOffsetSeconds = (machineLocation.u.gmtDelta << 8);
    gmtOffsetSeconds >>= 8;

    /* Backout OS adjustment for DST, to give consistent GMT offset. */
    if (machineLocation.u.dlsDelta != 0)
        gmtOffsetSeconds -= PRMJ_HOUR_SECONDS;
    return (zone = -gmtOffsetSeconds);
#endif
}

/* Constants for GMT offset from 1970 */
#define G1970GMTMICROHI        0x00dcdcad /* micro secs to 1970 hi */
#define G1970GMTMICROLOW       0x8b3fa000 /* micro secs to 1970 low */

#define G2037GMTMICROHI        0x00e45fab /* micro secs to 2037 high */
#define G2037GMTMICROLOW       0x7a238000 /* micro secs to 2037 low */

/* Convert from base time to extended time */
static JSInt64
PRMJ_ToExtendedTime(JSInt32 base_time)
{
    JSInt64 exttime;
    JSInt64 g1970GMTMicroSeconds;
    JSInt64 low;
    JSInt32 diff;
    JSInt64  tmp;
    JSInt64  tmp1;

    diff = PRMJ_LocalGMTDifference();
    JSLL_UI2L(tmp, PRMJ_USEC_PER_SEC);
    JSLL_I2L(tmp1,diff);
    JSLL_MUL(tmp,tmp,tmp1);

    JSLL_UI2L(g1970GMTMicroSeconds,G1970GMTMICROHI);
    JSLL_UI2L(low,G1970GMTMICROLOW);
#ifndef JS_HAVE_LONG_LONG
    JSLL_SHL(g1970GMTMicroSeconds,g1970GMTMicroSeconds,16);
    JSLL_SHL(g1970GMTMicroSeconds,g1970GMTMicroSeconds,16);
#else
    JSLL_SHL(g1970GMTMicroSeconds,g1970GMTMicroSeconds,32);
#endif
    JSLL_ADD(g1970GMTMicroSeconds,g1970GMTMicroSeconds,low);

    JSLL_I2L(exttime,base_time);
    JSLL_ADD(exttime,exttime,g1970GMTMicroSeconds);
    JSLL_SUB(exttime,exttime,tmp);
    return exttime;
}

JSInt64
PRMJ_Now(void)
{
#ifdef XP_OS2
    JSInt64 s, us, ms2us, s2us;
    struct timeb b;
#endif
#ifdef XP_WIN
    JSInt64 s, us,
    win2un = JSLL_INIT(0x19DB1DE, 0xD53E8000),
    ten = JSLL_INIT(0, 10);
    FILETIME time, midnight;
#endif
#if defined(XP_UNIX) || defined(XP_BEOS)
    struct timeval tv;
    JSInt64 s, us, s2us;
#endif /* XP_UNIX */
#ifdef XP_MAC
    JSUint64 upTime;
    JSInt64	 localTime;
    JSInt64       gmtOffset;
    JSInt64    dstOffset;
    JSInt32       gmtDiff;
    JSInt64	 s2us;
#endif /* XP_MAC */

#ifdef XP_OS2
    ftime(&b);
    JSLL_UI2L(ms2us, PRMJ_USEC_PER_MSEC);
    JSLL_UI2L(s2us, PRMJ_USEC_PER_SEC);
    JSLL_UI2L(s, b.time);
    JSLL_UI2L(us, b.millitm);
    JSLL_MUL(us, us, ms2us);
    JSLL_MUL(s, s, s2us);
    JSLL_ADD(s, s, us);
    return s;
#endif
#ifdef XP_WIN
    /* The windows epoch is around 1600. The unix epoch is around 1970.
       win2un is the difference (in windows time units which are 10 times
       more precise than the JS time unit) */
    GetSystemTimeAsFileTime(&time);
    /* Win9x gets confused at midnight
       http://support.microsoft.com/default.aspx?scid=KB;en-us;q224423
       So if the low part (precision <8mins) is 0 then we get the time
       again. */
    if (!time.dwLowDateTime) {
        GetSystemTimeAsFileTime(&midnight);
        time.dwHighDateTime = midnight.dwHighDateTime;
    }
    JSLL_UI2L(s, time.dwHighDateTime);
    JSLL_UI2L(us, time.dwLowDateTime);
    JSLL_SHL(s, s, 32);
    JSLL_ADD(s, s, us);
    JSLL_SUB(s, s, win2un);
    JSLL_DIV(s, s, ten);
    return s;
#endif

#if defined(XP_UNIX) || defined(XP_BEOS)
#ifdef _SVID_GETTOD   /* Defined only on Solaris, see Solaris <sys/types.h> */
    gettimeofday(&tv);
#else
    gettimeofday(&tv, 0);
#endif /* _SVID_GETTOD */
    JSLL_UI2L(s2us, PRMJ_USEC_PER_SEC);
    JSLL_UI2L(s, tv.tv_sec);
    JSLL_UI2L(us, tv.tv_usec);
    JSLL_MUL(s, s, s2us);
    JSLL_ADD(s, s, us);
    return s;
#endif /* XP_UNIX */
#ifdef XP_MAC
    JSLL_UI2L(localTime,0);
    gmtDiff = PRMJ_LocalGMTDifference();
    JSLL_I2L(gmtOffset,gmtDiff);
    JSLL_UI2L(s2us, PRMJ_USEC_PER_SEC);
    JSLL_MUL(gmtOffset,gmtOffset,s2us);

    /* don't adjust for DST since it sets ctime and gmtime off on the MAC */
    Microseconds((UnsignedWide*)&upTime);
    JSLL_ADD(localTime,localTime,gmtOffset);
    JSLL_ADD(localTime,localTime, dstLocalBaseMicroseconds);
    JSLL_ADD(localTime,localTime, upTime);

    dstOffset = PRMJ_DSTOffset(localTime);
    JSLL_SUB(localTime,localTime,dstOffset);

    return *((JSUint64 *)&localTime);
#endif /* XP_MAC */
}

/* Get the DST timezone offset for the time passed in */
JSInt64
PRMJ_DSTOffset(JSInt64 local_time)
{
    JSInt64 us2s;
#ifdef XP_MAC
    /*
     * Convert the local time passed in to Macintosh epoch seconds. Use UTC utilities to convert
     * to UTC time, then compare difference with our GMT offset. If they are the same, then
     * DST must not be in effect for the input date/time.
     */
    UInt32 macLocalSeconds = (local_time / PRMJ_USEC_PER_SEC) + gJanuaryFirst1970Seconds, utcSeconds;
    ConvertLocalTimeToUTC(macLocalSeconds, &utcSeconds);
    if ((utcSeconds - macLocalSeconds) == PRMJ_LocalGMTDifference())
        return 0;
    else {
        JSInt64 dlsOffset;
    	JSLL_UI2L(us2s, PRMJ_USEC_PER_SEC);
    	JSLL_UI2L(dlsOffset, PRMJ_HOUR_SECONDS);
    	JSLL_MUL(dlsOffset, dlsOffset, us2s);
        return dlsOffset;
    }
#else
    time_t local;
    JSInt32 diff;
    JSInt64  maxtimet;
    struct tm tm;
    PRMJTime prtm;
#ifndef HAVE_LOCALTIME_R
    struct tm *ptm;
#endif


    JSLL_UI2L(us2s, PRMJ_USEC_PER_SEC);
    JSLL_DIV(local_time, local_time, us2s);

    /* get the maximum of time_t value */
    JSLL_UI2L(maxtimet,PRMJ_MAX_UNIX_TIMET);

    if(JSLL_CMP(local_time,>,maxtimet)){
        JSLL_UI2L(local_time,PRMJ_MAX_UNIX_TIMET);
    } else if(!JSLL_GE_ZERO(local_time)){
        /*go ahead a day to make localtime work (does not work with 0) */
        JSLL_UI2L(local_time,PRMJ_DAY_SECONDS);
    }
    JSLL_L2UI(local,local_time);
    PRMJ_basetime(local_time,&prtm);
#ifndef HAVE_LOCALTIME_R
    ptm = localtime(&local);
    if(!ptm){
        return JSLL_ZERO;
    }
    tm = *ptm;
#else
    localtime_r(&local,&tm); /* get dst information */
#endif

    diff = ((tm.tm_hour - prtm.tm_hour) * PRMJ_HOUR_SECONDS) +
	((tm.tm_min - prtm.tm_min) * 60);

    if(diff < 0){
	diff += PRMJ_DAY_SECONDS;
    }

    JSLL_UI2L(local_time,diff);

    JSLL_MUL(local_time,local_time,us2s);

    return(local_time);
#endif
}

#ifdef NS_HAVE_INVALID_PARAMETER_HANDLER
JS_STATIC_DLL_CALLBACK(void)
PRMJ_InvalidParameterHandler(const wchar_t *expression,
                             const wchar_t *function, 
                             const wchar_t *file, 
                             unsigned int   line, 
                             uintptr_t      pReserved)
{
    /* empty */
}
#endif

/* Format a time value into a buffer. Same semantics as strftime() */
size_t
PRMJ_FormatTime(char *buf, int buflen, const char *fmt, PRMJTime *prtm)
{
#if defined(XP_UNIX) || defined(XP_PC) || defined(XP_MAC) || defined(XP_BEOS)
	size_t result = 0;
    struct tm a;

    /* Zero out the tm struct.  Linux, SunOS 4 struct tm has extra members int
     * tm_gmtoff, char *tm_zone; when tm_zone is garbage, strftime gets
     * confused and dumps core.  NSPR20 prtime.c attempts to fill these in by
     * calling mktime on the partially filled struct, but this doesn't seem to
     * work as well; the result string has "can't get timezone" for ECMA-valid
     * years.  Might still make sense to use this, but find the range of years
     * for which valid tz information exists, and map (per ECMA hint) from the
     * given year into that range.
     
     * N.B. This hasn't been tested with anything that actually _uses_
     * tm_gmtoff; zero might be the wrong thing to set it to if you really need
     * to format a time.  This fix is for jsdate.c, which only uses
     * JS_FormatTime to get a string representing the time zone.  */
    memset(&a, 0, sizeof(struct tm));

    a.tm_sec = prtm->tm_sec;
    a.tm_min = prtm->tm_min;
    a.tm_hour = prtm->tm_hour;
    a.tm_mday = prtm->tm_mday;
    a.tm_mon = prtm->tm_mon;
    a.tm_wday = prtm->tm_wday;
    a.tm_year = prtm->tm_year - 1900;
    a.tm_yday = prtm->tm_yday;
    a.tm_isdst = prtm->tm_isdst;

    /* Even with the above, SunOS 4 seems to detonate if tm_zone and tm_gmtoff
     * are null.  This doesn't quite work, though - the timezone is off by
     * tzoff + dst.  (And mktime seems to return -1 for the exact dst
     * changeover time.)

     */

#if defined(SUNOS4)
    if (mktime(&a) == -1) {
        /* Seems to fail whenever the requested date is outside of the 32-bit
         * UNIX epoch.  We could proceed at this point (setting a.tm_zone to
         * "") but then strftime returns a string with a 2-digit field of
         * garbage for the year.  So we return 0 and hope jsdate.c
         * will fall back on toString.
         */
        return 0;
    }
#endif

#ifdef NS_HAVE_INVALID_PARAMETER_HANDLER
    oldHandler = _set_invalid_parameter_handler(PRMJ_InvalidParameterHandler);
    oldReportMode = _CrtSetReportMode(_CRT_ASSERT, 0);
#endif

    result = strftime(buf, buflen, fmt, &a);
    
#ifdef NS_HAVE_INVALID_PARAMETER_HANDLER
    _set_invalid_parameter_handler(oldHandler);
    _CrtSetReportMode(_CRT_ASSERT, oldReportMode);
#endif

	return result;

#endif
}

/* table for number of days in a month */
static int mtab[] = {
    /* jan, feb,mar,apr,may,jun */
    31,28,31,30,31,30,
    /* july,aug,sep,oct,nov,dec */
    31,31,30,31,30,31
};

/*
 * basic time calculation functionality for localtime and gmtime
 * setups up prtm argument with correct values based upon input number
 * of seconds.
 */
static void
PRMJ_basetime(JSInt64 tsecs, PRMJTime *prtm)
{
    /* convert tsecs back to year,month,day,hour,secs */
    JSInt32 year    = 0;
    JSInt32 month   = 0;
    JSInt32 yday    = 0;
    JSInt32 mday    = 0;
    JSInt32 wday    = 6; /* start on a Sunday */
    JSInt32 days    = 0;
    JSInt32 seconds = 0;
    JSInt32 minutes = 0;
    JSInt32 hours   = 0;
    JSInt32 isleap  = 0;

    /* Temporaries used for various computations */
    JSInt64 result;
    JSInt64	result1;
    JSInt64	result2;

    JSInt64 base;

    /* Some variables for intermediate result storage to make computing isleap
       easier/faster */
    JSInt32 fourCenturyBlocks;
    JSInt32 centuriesLeft;
    JSInt32 fourYearBlocksLeft;
    JSInt32 yearsLeft;

    /* Since leap years work by 400/100/4 year intervals, precompute the length
       of those in seconds if they start at the beginning of year 1. */
    JSInt64 fourYears;
    JSInt64 century;
    JSInt64 fourCenturies;

    JSLL_UI2L(result, PRMJ_DAY_SECONDS);

    JSLL_I2L(fourYears, PRMJ_FOUR_YEARS_DAYS);
    JSLL_MUL(fourYears, fourYears, result);

    JSLL_I2L(century, PRMJ_CENTURY_DAYS);
    JSLL_MUL(century, century, result);

    JSLL_I2L(fourCenturies, PRMJ_FOUR_CENTURIES_DAYS);
    JSLL_MUL(fourCenturies, fourCenturies, result);

    /* get the base time via UTC */
    base = PRMJ_ToExtendedTime(0);
    JSLL_UI2L(result,  PRMJ_USEC_PER_SEC);
    JSLL_DIV(base,base,result);
    JSLL_ADD(tsecs,tsecs,base);

    /* Compute our |year|, |isleap|, and part of |days|.  When this part is
       done, |year| should hold the year our date falls in (number of whole
       years elapsed before our date), isleap should hold 1 if the year the
       date falls in is a leap year and 0 otherwise. */

    /* First do year 0; it's special and nonleap. */
    JSLL_UI2L(result, PRMJ_YEAR_SECONDS);
    if (!JSLL_CMP(tsecs,<,result)) {
        days = PRMJ_YEAR_DAYS;
        year = 1;
        JSLL_SUB(tsecs, tsecs, result);
    }

    /* Now use those constants we computed above */
    JSLL_UDIVMOD(&result1, &result2, tsecs, fourCenturies);
    JSLL_L2I(fourCenturyBlocks, result1);
    year += fourCenturyBlocks * 400;
    days += fourCenturyBlocks * PRMJ_FOUR_CENTURIES_DAYS;
    tsecs = result2;

    JSLL_UDIVMOD(&result1, &result2, tsecs, century);
    JSLL_L2I(centuriesLeft, result1);
    year += centuriesLeft * 100;
    days += centuriesLeft * PRMJ_CENTURY_DAYS;
    tsecs = result2;

    JSLL_UDIVMOD(&result1, &result2, tsecs, fourYears);
    JSLL_L2I(fourYearBlocksLeft, result1);
    year += fourYearBlocksLeft * 4;
    days += fourYearBlocksLeft * PRMJ_FOUR_YEARS_DAYS;
    tsecs = result2;

    /* Recall that |result| holds PRMJ_YEAR_SECONDS */
    JSLL_UDIVMOD(&result1, &result2, tsecs, result);
    JSLL_L2I(yearsLeft, result1);
    year += yearsLeft;
    days += yearsLeft * PRMJ_YEAR_DAYS;
    tsecs = result2;

    /* now compute isleap.  Note that we don't have to use %, since we've
       already computed those remainders.  Also note that they're all offset by
       1 because of the 1 for year 0. */
    isleap =
        (yearsLeft == 3) && (fourYearBlocksLeft != 24 || centuriesLeft == 3);
    JS_ASSERT(isleap ==
              ((year % 4 == 0) && (year % 100 != 0 || year % 400 == 0)));

    JSLL_UI2L(result1,PRMJ_DAY_SECONDS);

    JSLL_DIV(result,tsecs,result1);
    JSLL_L2I(mday,result);

    /* let's find the month */
    while(((month == 1 && isleap) ?
            (mday >= mtab[month] + 1) :
            (mday >= mtab[month]))){
	 yday += mtab[month];
	 days += mtab[month];

	 mday -= mtab[month];

         /* it's a Feb, check if this is a leap year */
	 if(month == 1 && isleap != 0){
	     yday++;
	     days++;
	     mday--;
	 }
	 month++;
    }

    /* now adjust tsecs */
    JSLL_MUL(result,result,result1);
    JSLL_SUB(tsecs,tsecs,result);

    mday++; /* day of month always start with 1 */
    days += mday;
    wday = (days + wday) % 7;

    yday += mday;

    /* get the hours */
    JSLL_UI2L(result1,PRMJ_HOUR_SECONDS);
    JSLL_DIV(result,tsecs,result1);
    JSLL_L2I(hours,result);
    JSLL_MUL(result,result,result1);
    JSLL_SUB(tsecs,tsecs,result);

    /* get minutes */
    JSLL_UI2L(result1,60);
    JSLL_DIV(result,tsecs,result1);
    JSLL_L2I(minutes,result);
    JSLL_MUL(result,result,result1);
    JSLL_SUB(tsecs,tsecs,result);

    JSLL_L2I(seconds,tsecs);

    prtm->tm_usec  = 0L;
    prtm->tm_sec   = (JSInt8)seconds;
    prtm->tm_min   = (JSInt8)minutes;
    prtm->tm_hour  = (JSInt8)hours;
    prtm->tm_mday  = (JSInt8)mday;
    prtm->tm_mon   = (JSInt8)month;
    prtm->tm_wday  = (JSInt8)wday;
    prtm->tm_year  = (JSInt16)year;
    prtm->tm_yday  = (JSInt16)yday;
}
