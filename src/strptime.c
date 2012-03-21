/*** strptime.c -- faster date parsing
 *
 * Copyright (C) 2009 Sebastian Freundt
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

#include <stdint.h>
#include <stdbool.h>
#define INCL_TBLS
#include "date.h"

#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif	/* !UNLIKELY */

static const int tenners[10] = {0, 10, 20, 30, 40, 50, 60, 70, 80, 90};

#define M	(1U)
#define T	(2U)
#define W	(3U)
#define R	(4U)
#define F	(5U)
#define A	(6U)
#define S	(0U)

/* week days of the years 1970 to 2029 */
static const uint8_t jan01_wday[] = {
#define __JAN01_WDAY_BEG	(1970)
	/* 1970 - 1979 */
	R, F, A, M, T, W, R, A, S, M, 0,
	/* 1980 - 1989 */
	T, R, F, A, S, T, W, R, F, S, 0,
	/* 1990 - 1999 */
	M, T, W, F, A, S, M, W, R, F, 0,
	/* 2000 - 2009 */
	A, M, T, W, R, A, S, M, T, R, 0,
	/* 2010 - 2019 */
	F, A, S, T, W, R, F, S, M, T, 0,
	/* 2020 - 2029 */
	W, F, A, S, M, W, R, F, A, M, 0,
#define __JAN01_WDAY_END	(2030)
};


static int
get_year(const char *buf)
{
/* we have to do year - 1900, so we just use the third and fourth char
 * and add 100 if buf[1] is '0' */
	return (buf[1] == '0' ? 100 : 0) +
		tenners[(buf[2] - '0')] + (int)(buf[3] - '0');
}

static int
get_month(const char *buf)
{
	return (buf[0] == '0' ? 0 : 10) + (int)(buf[1] - '0');
}

static int
get_day(const char *buf)
{
	return tenners[(buf[0] - '0')] + (int)(buf[1] - '0');
}

static int
get_hour(const char *buf)
{
	return tenners[(buf[0] - '0')] + (int)(buf[1] - '0');
}

static int
get_minute(const char *buf)
{
	return tenners[(buf[0] - '0')] + (int)(buf[1] - '0');
}

static int
get_second(const char *buf)
{
	return tenners[(buf[0] - '0')] + (int)(buf[1] - '0');
}

static int
yday(int y, int m, int d)
{
	return d + __mon_yday[__leapp(y)][m-1];
}

void
ffff_strptime(const char *buf, struct tm *restrict tm)
{
	tm->tm_year = get_year(buf + 0);
	tm->tm_mon = get_month(buf + 5) - 1;
	tm->tm_mday = get_day(buf + 8);
	tm->tm_hour = get_hour(buf + 11);
	tm->tm_min = get_minute(buf + 14);
	tm->tm_sec = get_second(buf + 17);
	/* need set this as timegm makes use of it */
	tm->tm_yday = yday(tm->tm_year, tm->tm_mon + 1, tm->tm_mday) - 1;
	return;
}

void
ffff_strptime_ISO(const char *buf, struct tm *restrict tm)
{
	int y = get_year(buf + 0);
	if (UNLIKELY(y < __JAN01_WDAY_BEG - 1900 ||
		     y >= __JAN01_WDAY_END - 1900)) {
		return;
	}
	tm->tm_year = y;
	tm->tm_mon = get_month(buf + 4) - 1;
	tm->tm_mday = get_day(buf + 6);
	tm->tm_hour = get_hour(buf + 8);
	tm->tm_min = get_minute(buf + 10);
	tm->tm_sec = get_second(buf + 12);
	/* need set this as timegm makes use of it */
	tm->tm_yday = yday(tm->tm_year, tm->tm_mon + 1, tm->tm_mday) - 1;
	tm->tm_wday = (tm->tm_yday + jan01_wday[tm->tm_year - 70]) % 7;
	return;
}

void
ffff_strptime_Ymd(const char *buf, struct tm *restrict tm)
{
/* like strptime() with %Y-%m-%d as format */
	tm->tm_year = get_year(buf + 0);
	tm->tm_mon = get_month(buf + 5) - 1;
	tm->tm_mday = get_day(buf + 8);
	/* need set this as timegm makes use of it */
	tm->tm_yday = yday(tm->tm_year, tm->tm_mon + 1, tm->tm_mday) - 1;
	return;
}

void
ffff_strptime_dmY(const char *buf, struct tm *restrict tm)
{
/* like strptime() with %d-%m-%Y as format */
	tm->tm_mday = get_day(buf + 0);
	tm->tm_mon = get_month(buf + 3) - 1;
	tm->tm_year = get_year(buf + 6);
	/* need set this as timegm makes use of it */
	tm->tm_yday = yday(tm->tm_year, tm->tm_mon + 1, tm->tm_mday) - 1;
	return;
}

void
ffff_strptime_HMS(const char *buf, struct tm *restrict tm)
{
/* like strptime() with %H:%M:%S as format */
	tm->tm_hour = get_hour(buf + 0);
	tm->tm_min = get_minute(buf + 3);
	tm->tm_sec = get_second(buf + 6);
	return;
}

/* strptime.c ends here */
