/*** m30.c -- integer-based monetary types
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
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include "m30.h"
#include "m30-rela.h"

static const uint64_t __attribute__((unused)) ffff_m30_i_ubounds[] = {
	/*00->*/5, /*01->*/53687,
	/*10->*/536870912, /*11->*/5368709120000,
};

static const uint8_t __attribute__((unused)) ffff_m30_i_nexpos[] = {
	/*00->*/8, /*01->*/4, /*10->*/0, /*11->*/-4,
};

/* special values */
#define FFFF_M30_MKT	(0xe0000000)


/* number deserialising */
static uint32_t
__30_23_get_s(const char *p, size_t n)
{
	uint32_t res = 0;

	/* inverse duff dev, kick the last 4 digits */
	switch (n & 3) {
	case 3:
		res = 10U * res + (*p++ & 0x0f);
	case 2:
		res = 10U * res + (*p++ & 0x0f);
	case 1:
		res = 10U * res + (*p++ & 0x0f);
	case 0:
	default:
		/* is case 4 actually */
		break;
	}
	/* now in blocks of 4 */
	for (; n >= 4; n -= 4) {
		res = 10U * res + (*p++ & 0x0f);
		res = 10U * res + (*p++ & 0x0f);
		res = 10U * res + (*p++ & 0x0f);
		res = 10U * res + (*p++ & 0x0f);
	}
	return res;
}

static uint32_t
__30_1_get_s(const char *p, size_t n, const char *f, size_t m)
{
	uint32_t res = 0;

	/* just do it */
	switch (n) {
	case 5:
		res = 10U * res + (*p++ & 0x0f);
	case 4:
		res = 10U * res + (*p++ & 0x0f);
	case 3:
		res = 10U * res + (*p++ & 0x0f);
	case 2:
		res = 10U * res + (*p++ & 0x0f);
	case 1:
		res = 10U * res + (*p++ & 0x0f);
	case 0:
	default:
		break;
	}

	/* just do it */
	switch (m) {
	default:
	case 4:
		res = 10U * res + (*f++ & 0x0f);
	case 3:
		res = 10U * res + (*f++ & 0x0f);
	case 2:
		res = 10U * res + (*f++ & 0x0f);
	case 1:
		res = 10U * res + (*f++ & 0x0f);
	case 0:
		break;
	}

	/* just multiply powers of 10 now */
	switch (4 - m) {
	case 4:
		res *= 10U;
	case 3:
		res *= 10U;
	case 2:
		res *= 10U;
	case 1:
		res *= 10U;
	case 0:
		break;
	default:
		if (*f >= '5' /*&& *f <= '9'*/) {
			res++;
		}
		break;
	}
	return res;
}

static uint32_t
__30_0_get_s(const char *mant, const char *f, size_t m)
{
/* only does the fractional part */
	uint32_t res = 0U;

	/* start off with the mantissa left of the decimal point */
	res = mant[0] - '0';
	switch (m) {
	default:
	case 8:
		res = 10U * res + (*f++ & 0x0f);
	case 7:
		res = 10U * res + (*f++ & 0x0f);
	case 6:
		res = 10U * res + (*f++ & 0x0f);
	case 5:
		res = 10U * res + (*f++ & 0x0f);
	case 4:
		res = 10U * res + (*f++ & 0x0f);
	case 3:
		res = 10U * res + (*f++ & 0x0f);
	case 2:
		res = 10U * res + (*f++ & 0x0f);
	case 1:
		res = 10U * res + (*f++ & 0x0f);
	case 0:
		break;
	}
	/* just multiply powers of 10 now */
	switch (8 - m) {
	case 8:
		res *= 10U;
	case 7:
		res *= 10U;
	case 6:
		res *= 10U;
	case 5:
		res *= 10U;
	case 4:
		res *= 10U;
	case 3:
		res *= 10U;
	case 2:
		res *= 10U;
	case 1:
		res *= 10U;
	case 0:
		break;
	default:
		if (*f >= '5' /*&& &f <= '9'*/) {
			res++;
		}
		break;
	}
	return res;
}

m30_t
ffff_m30_get_s(const char **nptr)
{
	/* spray some pointers */
	const char *mant, *mend, *frac;
	const char *p;
	bool neg = false;
	m30_t r30;

	/* yepp, bummer aye? */
	r30.expo = 1;

	/* space skipping omitted, rather die if there's a space */
	if (UNLIKELY(**nptr == '-')) {
		neg = true;
		mant = *nptr + 1;
	} else if (UNLIKELY(**nptr == '+')) {
		/* only morons do that */
		mant = *nptr + 1;
	} else {
		mant = *nptr;
	}

	/* check for specials */
	if (UNLIKELY(mant[0] == 'm' && mant[1] == 'k' && mant[2] == 't')) {
		*nptr += 3;
		r30.u = FFFF_M30_MKT;
		return r30;
	}

	/* find the decimal point */
	for (p = mant; *p >= '0' && *p <= '9'; p++);
	if (*p == '.') {
		frac = (mend = p) + 1;
		for (++p; *p >= '0' && *p <= '9'; p++);
	} else {
		mend = frac = p;
	}
	*nptr = p;

	/* do some estimates, the situation is
	 * +-xxxxxxxxx.yyyyyyy
	 *   ^        ^^      ^
	 *   mant mend frac   p
	 * so frac - mant is the number of integral digits */
	if ((p - frac) >= 4 && (mend - mant) <= 1 && (mant[0] < '5')) {
		r30.expo = 0;
		r30.mant = __30_0_get_s(mant, frac, p - frac);
	} else if ((mend - mant) < 5 || (mend - mant) == 5 && mant[0] < '5') {
		r30.expo = 1;
		r30.mant = __30_1_get_s(mant, mend - mant, frac, p - frac);
	} else if (UNLIKELY((mend - mant) >= 8)) {
		r30.expo = 3;
		r30.mant = __30_23_get_s(mant, mend - mant - 4);
	} else {
		r30.expo = 2;
		r30.mant = __30_23_get_s(mant, mend - mant);
	}

	if (UNLIKELY(neg)) {
		r30.mant = -r30.mant;
	}
	return r30;
}

m30_t
ffff_m30_23_get_s(const char **nptr)
{
	/* spray some pointers */
	const char *mant, *mend;
	bool neg = false;
	m30_t r30;

	/* space skipping omitted, rather die if there's a space */
	if (**nptr == '-') {
		neg = true;
		mant = *nptr + 1;
	} else if (**nptr == '+') {
		/* only morons do that */
		mant = *nptr + 1;
	} else {
		mant = *nptr;
	}

	/* find the end of the number */
	for (mend = mant; *mend >= '0' && *mend <= '9'; mend++);
	*nptr = mend;

	/* do some estimates, the situation is
	 * +-xxxxxxxxx.
	 *   ^        ^
	 *   mant mend
	 *   so mend - mant is the number of integral digits */
	r30.expo = 2;
	r30.mant = __30_23_get_s(mant, mend - mant);

	if (UNLIKELY(neg)) {
		r30.mant = -r30.mant;
	}
	return r30;
}


/* number serialising */
/**
 * Return the number of fractional digits, i.e. the ones right of point. */
static inline int8_t
__m30_nfrac_digits(m30_t m)
{
	/* seq is 8, 4, 0, -4 */
	return (int8_t)(8 - 4 * m.expo);
}

#if !defined STRREV
# define STRREV
static void
strrev(char *restrict tgt, char *restrict src)
{
	char tmp;
	while (src > tgt) {
		tmp = *src, *src-- = *tgt, *tgt++ = tmp;
	}
	return;
}
#endif	/* !STRREV */

size_t
ffff_m30_s(char *restrict buf, m30_t m)
{
	char *tmp = buf;
	bool sign;
	register int8_t nfrac;

	/* special values first */
	if (UNLIKELY(m.u == FFFF_M30_MKT)) {
		*tmp++ = 'm';
		*tmp++ = 'k';
		*tmp++ = 't';
		*tmp = '\0';
		return 3;
	}

	/* otherwise */
	nfrac = __m30_nfrac_digits(m);
	if ((sign = (m.mant < 0))) {
		m.mant = -m.mant;
	}
	if (m.mant == 0) {
		/* mant = 0 means all is 0 */
		*tmp++ = '0';
		*tmp = '\0';
		return 1;
	}
	if (nfrac > 0) {
		for (register int8_t i = 0; i < nfrac; i++) {
			*tmp++ = (char)('0' + m.mant % 10), m.mant /= 10;
		}
		*tmp++ = '.';
	} else {
		for (register int8_t i = nfrac; i < 0; i++) {
			*tmp++ = '0';
		}
	}
	do {
		*tmp++ = (char)('0' + m.mant % 10);
	} while (m.mant /= 10);
	if (sign) {
		*tmp++ = '-';
	}
	*tmp = '\0';
	strrev(buf, tmp - 1);
	return (size_t)(tmp - buf);
}

/* m30.c ends here */
