/*** strptime.h -- faster date routines
 *
 * Copyright (C) 2009-2015 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@ga-group.nl>
 *
 * This file is part of uterus.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the author nor the names of any contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ***/

#ifndef INCLUDED_date_h_
#define INCLUDED_date_h_		1

#include <stdbool.h>
#include <time.h>
#include "tzraw.h"

#ifdef __cplusplus
extern "C" {
# if defined __GNUC__
#  define restrict	__restrict__
# else
#  define restrict
# endif
#endif /* __cplusplus */

/**
 * Like strptime() but sans the format specs argument and only for ISO
 * dates YYYY-MM-DD HH:MM:SS */
extern void
ffff_strptime(const char *buf, struct tm *restrict tm);

/**
 * Like strptime() but sans the format specs argument and only for ISO
 * dates YYYYMMDDHHMMSS */
extern void
ffff_strptime_ISO(const char *buf, struct tm *restrict tm);
/**
 * Like strptime() but sans the format specs argument but only for date
 * stamps and only for ISO dates YYYY-MM-DD,
 * the separator is actually ignored. */
extern void
ffff_strptime_Ymd(const char *buf, struct tm *restrict tm);

/**
 * Like strptime() but sans the format specs argument but only for date
 * stamps and only for dates in the DD-MM-YYYY format,
 * the separator is actually ignored. */
extern void
ffff_strptime_dmY(const char *buf, struct tm *restrict tm);

/**
 * Like strptime() but sans the format specs argument but only for time
 * stamps in the HH:MM:SS format,
 * the separator is actually ignored. */
extern void
ffff_strptime_HMS(const char *buf, struct tm *restrict tm);

/**
 * Like strftime() but sans the format specs argument and only for ISO
 * dates YYYY-MM-DD HH:MM:SS */
extern void
ffff_strftime(char *restrict buf, size_t len, const struct tm *tm, char sep);

/**
 * Like timegm() but without all the fuss. */
extern time_t
ffff_timegm(register const struct tm *const tm);

/**
 * Like timelocal() (aka mktime) but without all the fuss
 * and better time zone handling. */
extern time_t
ffff_timelocal(register const struct tm *const tm, zif_t z);

/**
 * Like gmtime() but without all the fuss and moreover reentrant. */
extern void
ffff_gmtime(struct tm *tm, time_t t);

/**
 * Like localtime() but without all the fuss and moreover reentrant. */
extern void
ffff_localtime(struct tm *tm, time_t t, zif_t z);


/**
 * Return whether YEAR is a leap year. */
static inline bool __attribute__((always_inline))
__leapp(int year)
{
#if 0
	/* actually this is */
	return (year % 4 == 0 && ((year % 100) || (year % 400 == 0)));
#else
	/* but our design range (1970 to 2099) has no bullshit years in it */
	return (year % 4) == 0;
#endif
}

/**
 * Return the number of leap years in [1970, YEAR). */
static inline int __attribute__((always_inline))
__nleap_years(int year)
{
	/* again, we (ab)use that our design range is 1970 to 2099 and
	 * that it has no bullshit years in it */
	return (year - 1970 + 1) / 4;
}

#if defined INCL_TBLS
/* data */
static const unsigned short int __attribute__((unused)) __mon_yday[2][13] = {
	/* Normal years.  */
	{ 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
	/* Leap years.  */
	{ 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
};
#endif	/* INCL_TBLS */

#ifdef __cplusplus
}
#endif	/* __cplusplus */

#endif	/* INCLUDED_date_h_ */
