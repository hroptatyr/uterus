/*** m62.h -- integer-based monetary types
 *
 * Copyright (C) 2009-2013 Sebastian Freundt
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
#include "m62.h"
#include "m62-rela.h"

static const uint64_t __attribute__((unused)) ffff_m62_i_ubounds_l10[] = {
	/*00->*/11, /*01->*/15, /*10->*/19, /*11->*/23,
};

static const uint8_t __attribute__((unused)) ffff_m62_i_nexpos[] = {
	/*00->*/8, /*01->*/4, /*10->*/0, /*11->*/-4,
};


static uint64_t
__62_23_get_s(const char *p, size_t n)
{
	uint64_t res = 0;

	/* inverse duff dev, kick the last 4 digits */
	switch (n & 3) {
	case 3:
		res += 100 * (*p++ & 0x0f);
	case 2:
		res += 10 * (*p++ & 0x0f);
	case 1:
		res += (*p++ & 0x0f);
	case 0:
	default:
		/* is case 4 actually */
		break;
	}
	/* now in blocks of 4 */
	for (; n >= 4; n -= 4) {
		res *= 10000;
		res += 1000 * (*p++ & 0x0f);
		res += 100 * (*p++ & 0x0f);
		res += 10 * (*p++ & 0x0f);
		res += 1 * (*p++ & 0x0f);
	}
	return res;
}

static uint64_t
__62_1_get_s(const char *p, size_t n, const char *f, size_t m)
{
	uint64_t res;

	/* use the 23_getter to obtain the integral part */
	res = __62_23_get_s(p, n) * 10000;

	/* just do it */
	f += m;
	switch (m) {
	case 4:
		res += 1 * (*--f & 0x0f);
	case 3:
		res += 10 * (*--f & 0x0f);
	case 2:
		res += 100 * (*--f & 0x0f);
	case 1:
		res += 1000 * (*--f & 0x0f);
	case 0:
	default:
		break;
	}
	return res;
}

static uint64_t
__62_0_get_s(const char *mant, size_t n, const char *f, size_t m)
{
	uint64_t res;

	/* use the 23_getter to obtain the integral part */
	res = __62_23_get_s(mant, n);

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

m62_t
ffff_m62_get_s(const char **nptr)
{
	/* spray some pointers */
	const char *mant, *mend, *frac;
	const char *p;
	bool neg = false;
	m62_t r62;

	M62_SET_EXPO(r62, 1);

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
	if ((p - frac) >= 4 &&
	    (uint64_t)(mend - mant) <= ffff_m62_i_ubounds_l10[0]) {
		M62_SET_EXPO(r62, 0);
		M62_SET_MANT(
			r62, __62_0_get_s(mant, (mend - mant), frac, p - frac));
	} else if ((p - frac) > 0 ||
		   (uint64_t)(mend - mant) <= ffff_m62_i_ubounds_l10[1]) {
		M62_SET_EXPO(r62, 1);
		M62_SET_MANT(
			r62, __62_1_get_s(mant, mend - mant, frac, p - frac));
	} else if ((uint64_t)(mend - mant) >= ffff_m62_i_ubounds_l10[2]) {
		M62_SET_EXPO(r62, 3);
		M62_SET_MANT(r62, __62_23_get_s(mant, mend - mant - 4));
	} else {
		M62_SET_EXPO(r62, 2);
		M62_SET_MANT(r62, __62_23_get_s(mant, mend - mant));
	}
	M62_SET_MANT(r62, !neg ? M62_MANT(r62) : -M62_MANT(r62));
	return r62;
}


/**
 * Return the number of fractional digits, i.e. the ones right of point. */
static inline int8_t
__m62_nfrac_digits(m62_t m)
{
	/* seq is 8, 4, 0, -4 */
	return (int8_t)(8 - 4 * M62_EXPO(m));
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
ffff_m62_s(char *restrict buf, m62_t m)
{
	char *tmp = buf;
	bool sign;
	int8_t nfrac = __m62_nfrac_digits(m);

	if ((sign = (M62_MANT(m) < 0))) {
		M62_SET_MANT(m, -M62_MANT(m));
	}
	if (nfrac > 0) {
		for (register int8_t i = 0; i < nfrac; i++) {
			*tmp++ = (char)('0' + M62_MANT(m) % 10);
			M62_MANT_DIVSET_10(m);
		}
		*tmp++ = '.';
	} else {
		for (register int8_t i = nfrac; i < 0; i++) {
			*tmp++ = '0';
		}
	}
	do {
		*tmp++ = (char)('0' + M62_MANT(m) % 10);
	} while (M62_MANT_DIVSET_10(m));
	if (sign) {
		*tmp++ = '-';
	}
	*tmp = '\0';
	strrev(buf, tmp - 1);
	return (size_t)(tmp - buf);
}


long int
ffff_strtol(const char *p, const char **endp, int b __attribute__((unused)))
{
	long int res;
	bool neg = false;

	/* space skipping omitted, rather die if there's a space */
	if (*p == '-') {
		neg = true;
		p++;
	} else if (*p == '+') {
		/* only morons do that */
		p++;
	}
	/* ffw to the first non-digit */
	for (*endp = p; **endp >= '0' && **endp <= '9'; (*endp)++);
	res = __62_23_get_s(p, *endp - p);
	return !neg ? res : -res;
}

/* m62.c ends here */
