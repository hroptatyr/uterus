/*** m30.h -- integer-based monetary types
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

#ifndef INCLUDED_m30_h_
#define INCLUDED_m30_h_		1

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#if !defined M30T
typedef union m30_u m30_t;
# define M30T

union m30_u {
	int32_t v;
	uint32_t u;
	struct {
		int32_t mant:30;
		uint8_t expo:2 __attribute__((packed));
	};
} __attribute__((transparent_union));
#endif	/* !M30T */

/* muxer */
static inline m30_t __attribute__((pure, always_inline))
ffff_m30_get_ui32(uint32_t v)
{
	m30_t res;
	/* gcc can't deal with this? :O */
	res.v = v;
	return res;
}

static inline uint32_t __attribute__((pure, always_inline))
ffff_m30_ui32(m30_t v)
{
	return v.v;
}

/* ctors */
/**
 * Convert M to IEEE754r float. */
static inline float __attribute__((always_inline))
ffff_m30_f(m30_t m);

/**
 * Convert IEEE754r float F to monetary. */
static inline m30_t __attribute__((always_inline))
ffff_m30_get_f(float f);

/**
 * Convert M to IEEE754r double. */
static inline double __attribute__((always_inline))
ffff_m30_d(m30_t m);

/**
 * Convert IEEE754r doube D to monetary. */
static inline m30_t __attribute__((always_inline))
ffff_m30_get_d(double d);

/**
 * Convert string S to monetary. */
static inline m30_t __attribute__((always_inline))
ffff_m30_get_s(char **s);

/**
 * Represent m30 value as string. */
static inline size_t
ffff_m30_s(char *restrict buf, m30_t m);


/* ctor impls */
#define FFFF_M30_EXPO_DIFF_LOG	4
#define FFFF_M30_EXPO_DIFF	10000
static const float ffff_m30_f_denoms[] = {
	/*00->*/0.00000001f, /*01->*/0.0001f,
	/*10->*/1.f, /*11->*/10000.f,
};

static const float ffff_m30_f_ubounds[] = {
	/*00->*/5.36870912f, /*01->*/53687.0912f,
	/*10->*/536870912.f, /*11->*/5368709120000.f,
};

static inline float __attribute__((always_inline))
ffff_m30_f(m30_t m)
{
	return (float)m.mant * ffff_m30_f_denoms[m.expo];
}

static inline m30_t __attribute__((always_inline))
ffff_m30_get_f(float f)
{
	m30_t res;
	uint8_t i = 0;
	float fa = fabsf(f);

	/* simple range check */
	for (i = 0; i < 4; i++) {
		if (fa < ffff_m30_f_ubounds[i]) {
			break;
		}
	}
	res.expo = i;
	res.mant = (int32_t)(f / ffff_m30_f_denoms[i]);
	return res;
}

static const double ffff_m30_d_denoms[] = {
	/*00->*/0.00000001, /*01->*/0.0001,
	/*10->*/1., /*11->*/10000.,
};

static const double ffff_m30_d_denoms_reci[] = {
	/*00*/100000000., /*01*/10000,
	/*10*/1., /*11*/0.0001,
};

static const double ffff_m30_d_ubounds[] = {
	/*00->*/5.36870912, /*01->*/53687.0912,
	/*10->*/536870912., /*11->*/5368709120000.,
};

static inline double __attribute__((always_inline))
ffff_m30_d(m30_t m)
{
	return (double)m.mant * ffff_m30_d_denoms[m.expo];
}

#if defined FPMATH_NO_SSE && 0
static inline int32_t __attribute__((pure))
d2int32(double d)
{
	int32_t res;
	__asm__ volatile(
		"fld %1;"
		"fistpl %0;"
		: "=m" (res)
		: "m" (d)
		);
	return res;
}
#endif	/* FPMATH_NO_SSE */

static inline m30_t __attribute__((always_inline))
ffff_m30_get_d(double d)
{
	m30_t res;
	uint8_t i = 0;
	double fa = fabs(d);

	/* simple range check */
	for (i = 0; i < 4; i++) {
		if (fa < ffff_m30_d_ubounds[i]) {
			break;
		}
	}
	res.expo = i;
	res.mant = (int32_t)(d * ffff_m30_d_denoms_reci[i]);
	return res;
}

static inline uint32_t
__m30_add_helper(m30_t a, m30_t b)
{
	switch (a.expo - b.expo) {
	case 1:
		return a.mant + b.mant / 10000;
	case 2:
		return a.mant + b.mant / 10000 / 10000;
	case 3:
		/* no need to do anything, it's out of range */
		return a.mant;
	default:
		break;
	}
	return 0;
}

static inline m30_t
ffff_m30_add(m30_t a, m30_t b)
{
	m30_t res;

	if (a.expo == b.expo) {
		/* common case */
		res.mant = a.mant + b.mant;
		res.expo = a.expo;
	} else if (a.expo > b.expo) {
		res.mant = __m30_add_helper(a, b);
		res.expo = a.expo;
	} else /*if (b.expo > a.expo)*/ {
		/* addition is commutative */
		res.mant = __m30_add_helper(b, a);
		res.expo = b.expo;
	}
	return res;
}

#if defined WITH_M30_CMP
/**
 * Compare A and B, return -1 if A < B, 0 if A == B and 1 if A > B. */
static inline int
ffff_m30_cmp(m30_t a, m30_t b)
{
	if (a.expo == b.expo) {
		if (a.mant < b.mant) {
			return -1;
		} else if (a.mant > b.mant) {
			return 1;
		} else {
			return 0;
		}
	}
	/* code me */
	abort();
	return -2;
}
#endif	/* WITH_M30_CMP */

#if !defined DEFINE_GORY_STUFF
static inline m30_t
ffff_m30_get_s(char **s __attribute__((unused)))
{
	m30_t res;
	/* I love gcc :( */
	res.v = 0;
	return res;
}

static inline size_t
ffff_m30_s(char *restrict buf, m30_t m __attribute__((unused)))
{
	*buf = '\0';
	return 0;
}

#else  /* GORY_STUFF */
static const uint64_t __attribute__((unused)) ffff_m30_i_ubounds[] = {
	/*00->*/5, /*01->*/53687,
	/*10->*/536870912, /*11->*/5368709120000,
};

static const uint8_t __attribute__((unused)) ffff_m30_i_nexpos[] = {
	/*00->*/8, /*01->*/4, /*10->*/0, /*11->*/-4,
};

static uint32_t
__30_23_get_s(const char *p, size_t n)
{
	uint32_t res = 0;

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

static uint32_t
__30_1_get_s(const char *p, size_t n, const char *f, size_t m)
{
	uint32_t res = 0;

	/* just do it */
	switch (n) {
	case 5:
		res += 100000000 * (*p++ & 0x0f);
	case 4:
		res += 10000000 * (*p++ & 0x0f);
	case 3:
		res += 1000000 * (*p++ & 0x0f);
	case 2:
		res += 100000 * (*p++ & 0x0f);
	case 1:
		res += 10000 * (*p++ & 0x0f);
	case 0:
	default:
		break;
	}

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

static uint32_t
__30_0_get_s(const char *mant, const char *f, size_t m)
{
/* only does the fractional part */
	uint32_t res = 0;

	switch (mant[0]) {
	case '1':
		res = 100000000;
		break;
	case '2':
		res = 200000000;
		break;
	case '3':
		res = 300000000;
		break;
	case '4':
		res = 400000000;
		break;
	case '5':
		res = 500000000;
		break;
	case '0':
		break;
	default:
		return 0;
	}

	f += m;
	switch (m) {
	case 8:
		res += 1 * (*--f & 0x0f);
	case 7:
		res += 10 * (*--f & 0x0f);
	case 6:
		res += 100 * (*--f & 0x0f);
	case 5:
		res += 1000 * (*--f & 0x0f);
	case 4:
		res += 10000 * (*--f & 0x0f);
	case 3:
		res += 100000 * (*--f & 0x0f);
	case 2:
		res += 1000000 * (*--f & 0x0f);
	case 1:
		res += 10000000 * (*--f & 0x0f);
	case 0:
	default:
		break;
	}
	return res;
}

static inline m30_t
ffff_m30_get_s(char **nptr)
{
	/* spray some pointers */
	char *mant, *mend, *frac;
	char *p;
	bool neg = false;
	m30_t r30;

	/* yepp, bummer aye? */
	r30.expo = 1;

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
	if ((p - frac) >= 4 && (mend - mant) <= 1 && (mant[0] <= '5')) {
		r30.expo = 0;
		r30.mant = __30_0_get_s(mant, frac, p - frac);
	} else if ((mend - mant) <= 5) {
		r30.expo = 1;
		r30.mant = __30_1_get_s(mant, mend - mant, frac, p - frac);
	} else if ((mend - mant) >= 8) {
		r30.expo = 3;
		r30.mant = __30_23_get_s(mant, mend - mant - 4);
	} else {
		r30.expo = 2;
		r30.mant = __30_23_get_s(mant, mend - mant);
	}
	r30.mant = !neg ? r30.mant : -r30.mant;
	return r30;
}

static m30_t __attribute__((unused))
ffff_m30_23_get_s(char **nptr)
{
	/* spray some pointers */
	char *mant, *mend;
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
	r30.mant = !neg ? r30.mant : -r30.mant;
	return r30;
}

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

static inline size_t
ffff_m30_s(char *restrict buf, m30_t m)
{
	char *tmp = buf;
	bool sign;
	int8_t nfrac = __m30_nfrac_digits(m);

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
#endif	/* GORY_STUFF */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* INCLUDED_m30_h_ */
