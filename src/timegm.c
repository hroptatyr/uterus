/*** timegm.c -- faster date routines
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

#define INCL_TBLS
#include "date.h"
#include "nifty.h"

time_t
ffff_timegm(const struct tm *const tm)
{
	long int days = tm->tm_yday +
		365 * (tm->tm_year - 70) +
		/* number of leap years */
		__nleap_years(tm->tm_year + 1900);
	time_t res;

	/* newton adding up the result */
	res = days;
		res = res * HOURS_PER_DAY + tm->tm_hour;
		res = res * MINS_PER_HOUR + tm->tm_min;
		res = res * SECS_PER_MINUTE + tm->tm_sec;
	if (UNLIKELY(tm->tm_hour >= HOURS_PER_DAY ||
		     tm->tm_sec >= SECS_PER_MINUTE)) {
		/* leap second or military midnight */
		res--;
	}
	return res;
}

void
ffff_gmtime(struct tm *tm, time_t t)
{
	register int days, rem, yy;
	const unsigned short int *ip;

	days = t / SECS_PER_DAY, rem = t % SECS_PER_DAY;

	/* set second minute and hour */
	tm->tm_hour = rem / SECS_PER_HOUR, rem %= SECS_PER_HOUR;
	tm->tm_min = rem / SECS_PER_MINUTE;
	tm->tm_sec = rem % SECS_PER_MINUTE;
	/* week day computation, that one's easy, 1 jan '70 was Thu */
	tm->tm_wday = (days + 4) % 7;

	/* gotta do the date now */
	yy = 1970;
	/* stolen from libc */
#define DIV(a, b)		((a) / (b))
/* we only care about 1970 to 2099 and there are no bullshit leap years */
#define LEAPS_TILL(y)		(DIV(y, 4))
	while (days < 0 || days >= (!__leapp(yy) ? 365 : 366)) {
		/* Guess a corrected year, assuming 365 days per year. */
		register int yg = yy + days / 365 - (days % 365 < 0);

		/* Adjust DAYS and Y to match the guessed year.  */
		days -= (yg - yy) * 365 +
			LEAPS_TILL(yg - 1) - LEAPS_TILL(yy - 1);
		yy = yg;
	}
	/* set the year */
	tm->tm_year = yy - 1900;

	ip = __mon_yday[__leapp(yy)];
#if 0
	for (yy = 11; days < (int)ip[yy]; --yy) {
		continue;
	}
#elif 1  /* unrolled */
	yy = 12;
	if (days < ip[--yy] &&
	    days < ip[--yy] &&
	    days < ip[--yy] &&
	    days < ip[--yy] &&
	    days < ip[--yy] &&
	    days < ip[--yy] &&
	    days < ip[--yy] &&
	    days < ip[--yy] &&
	    days < ip[--yy] &&
	    days < ip[--yy] &&
	    days < ip[--yy]) {
		yy = 0;
	}
#endif
	/* set the rest of the tm structure */
	tm->tm_yday = days;
	tm->tm_mon = yy;
	tm->tm_mday = days - ip[yy] + 1;
	return;
}

void
ffff_localtime(struct tm *tm, time_t t, zif_t z)
{
	int32_t new_t = zif_local_time(z, t);
	ffff_gmtime(tm, new_t);
#if defined HAVE_STRUCT_TM_TM_GMTOFF
	tm->tm_gmtoff = new_t - t;
#endif	/* HAVE_STRUCT_TM_TM_GMTOFF */
	return;
}

/* timegm.c ends here */
