/*** strftime.c -- faster date handling
 *
 * Copyright (C) 2009 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <sebastian.freundt@ga-group.nl>
 *
 * This file is part of libffff.
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
#include "date.h"

#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif
#if !defined UNUSED
# define UNUSED(_x)	_x __attribute__((unused))
#endif

static inline void
sprint_year(char *restrict buf, const struct tm *restrict tm)
{
	static const uint16_t years[] = {
		/* 1970 */
		0x3730, 0x3731, 0x3732, 0x3733, 0x3734,
		0x3735, 0x3736, 0x3737, 0x3738, 0x3739,
		/* 1980 */
		0x3830, 0x3831, 0x3832, 0x3833, 0x3834,
		0x3835, 0x3836, 0x3837, 0x3838, 0x3839,
		/* 1990 */
		0x3930, 0x3931, 0x3932, 0x3933, 0x3934,
		0x3935, 0x3936, 0x3937, 0x3938, 0x3939,
		/* 2000 */
		0x3030, 0x3031, 0x3032, 0x3033, 0x3034,
		0x3035, 0x3036, 0x3037, 0x3038, 0x3039,
		/* 2010 */
		0x3130, 0x3131, 0x3132, 0x3133, 0x3134,
		0x3135, 0x3136, 0x3137, 0x3138, 0x3139,
		/* 2020 */
		0x3230, 0x3231, 0x3232, 0x3233, 0x3234,
		0x3235, 0x3236, 0x3237, 0x3238, 0x3239,
		/* 2030 */
		0x3330, 0x3331, 0x3332, 0x3333,
		0x3334, 0x3335, 0x3336, 0x3337,
	};
	uint16_t y = years[tm->tm_year - 70];
	/* big endian copy */
	if (LIKELY(tm->tm_year >= 100)) {
		buf[0] = '2', buf[1] = '0';
	} else {
		buf[0] = '1', buf[1] = '9';
	}
	buf[2] = (char)(y >> 8), buf[3] = (char)(y >> 0);
	return;
}

static inline void
sprint_month(char *restrict buf, const struct tm *restrict tm)
{
	static const uint16_t months[] = {
		0x3031, 0x3032, 0x3033, 0x3034, 0x3035, 0x3036,
		0x3037, 0x3038, 0x3039, 0x3130, 0x3131, 0x3132,
	};
	uint16_t m = months[tm->tm_mon];
	/* big endian copy */
	buf[0] = (char)(m >> 8), buf[1] = (char)(m >> 0);
	return;
}

static inline void
sprint_day(char *restrict buf, const struct tm *restrict tm)
{
	static const uint16_t days[] = {
		0x3031, 0x3032, 0x3033, 0x3034, 0x3035, 0x3036,
		0x3037, 0x3038, 0x3039,
		0x3130, 0x3131, 0x3132, 0x3133, 0x3134,
		0x3135, 0x3136, 0x3137, 0x3138, 0x3139,
		0x3230, 0x3231, 0x3232, 0x3233, 0x3234,
		0x3235, 0x3236, 0x3237, 0x3238, 0x3239,
		0x3330, 0x3331,
	};
	uint16_t d = days[tm->tm_mday - 1];
	/* big endian copy */
	buf[0] = (char)(d >> 8), buf[1] = (char)(d >> 0);
	return;
}

static inline void
sprint_hour(char *restrict buf, const struct tm *restrict tm)
{
	static const uint16_t hours[] = {
		0x3030, 0x3031, 0x3032, 0x3033, 0x3034,
		0x3035, 0x3036, 0x3037, 0x3038, 0x3039,
		0x3130, 0x3131, 0x3132, 0x3133, 0x3134,
		0x3135, 0x3136, 0x3137, 0x3138, 0x3139,
		0x3230, 0x3231, 0x3232, 0x3233,
	};
	uint16_t h = hours[tm->tm_hour];
	/* big endian copy */
	buf[0] = (char)(h >> 8), buf[1] = (char)(h >> 0);
	return;
}

static const char units10[] = "0123456789";

static inline void
sprint_minute(char *restrict buf, const struct tm *restrict tm)
{
	int t = tm->tm_min / 10, u = tm->tm_min % 10;
	/* big endian copy */
	buf[0] = units10[t], buf[1] = units10[u];
	return;
}

static inline void
sprint_second(char *restrict buf, const struct tm *restrict tm)
{
	int t = tm->tm_sec / 10, u = tm->tm_sec % 10;
	/* big endian copy */
	buf[0] = units10[t], buf[1] = units10[u];
	return;
}

void
ffff_strftime(char *restrict b, size_t UNUSED(l), const struct tm *restrict tm)
{
	sprint_year(&b[0], tm);
	b[4] = '-';
	sprint_month(&b[5], tm);
	b[7] = '-';
	sprint_day(&b[8], tm);

	b[10] = ' ';
	sprint_hour(&b[11], tm);
	b[13] = ':';
	sprint_minute(&b[14], tm);
	b[16] = ':';
	sprint_second(&b[17], tm);
	b[19] = '\0';
	return;
}

/* strftime.c ends here */
