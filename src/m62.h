/*** m62.h -- integer-based monetary types
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

#ifndef INCLUDED_m62_h_
#define INCLUDED_m62_h_		1

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
# if defined __GNUC__
#  define restrict	__restrict__
# else
#  define restrict
# endif
#endif /* __cplusplus */

#if !defined M62T
typedef union m62_u m62_t;
# define M62T

union m62_u {
	int64_t v;
	uint64_t u;
	struct {
		int64_t mant:62;
		uint8_t expo:2 __attribute__((packed));
	};
} __attribute__((transparent_union));
#endif	/* !M62T */

/* muxer */
static inline m62_t __attribute__((pure, always_inline))
ffff_m62_get_ui64(uint64_t v)
{
	m62_t res;
	res.v = v;
	return res;
}

static inline uint64_t __attribute__((pure, always_inline))
ffff_m62_ui64(m62_t v)
{
	return v.v;
}

/* ctors */
/**
 * Convert M to IEEE754r float. */
static inline float __attribute__((always_inline))
ffff_m62_f(m62_t m);

/**
 * Convert IEEE754r float F to monetary. */
static inline m62_t __attribute__((always_inline))
ffff_m62_get_f(float f);

/**
 * Convert M to IEEE754r double. */
static inline double __attribute__((always_inline))
ffff_m62_d(m62_t m);

/**
 * Convert IEEE754r doube D to monetary. */
static inline m62_t __attribute__((always_inline))
ffff_m62_get_d(double d);

/**
 * Convert string S to monetary. */
static inline m62_t __attribute__((always_inline))
ffff_m62_get_s(const char **s);

/**
 * Represent m62 value as string. */
static inline size_t
ffff_m62_s(char *restrict buf, m62_t m);

/* remember to include m30.h BEFORE m62.h to get this defined */
#if defined INCLUDED_m30_h_
/**
 * Convert number in M30 to an m62.
 * This conversion is lossless, as opposed to the converse which is
 * why there is no ffff_m62_m30(). */
static inline m62_t __attribute__((always_inline))
ffff_m62_get_m30(m30_t m30)
{
	m62_t res;
	res.expo = m30.expo;
	res.mant = m30.mant;
	return res;
}
#endif	/* INCLUDED_m30_h_ */

/**
 * For convenience. */
static inline long int
ffff_strtol(const char *p, const char **endp, int b);


/* ctor impls */
#define FFFF_M62_EXPO_DIFF_LOG	4
#define FFFF_M62_EXPO_DIFF	10000
static const float ffff_m62_f_denoms[] = {
	/*00->*/0.00000001f, /*01->*/0.0001f,
	/*10->*/1.f, /*11->*/10000.f,
};

static const float ffff_m62_f_ubounds[] = {
	/*00->*/23058430092.13693952f, /*01->*/230584300921369.3952f,
	/*10->*/2305843009213693952.f, /*11->*/23058430092136939520000.f,
};

static inline float __attribute__((always_inline))
ffff_m62_f(m62_t m)
{
	return (float)m.mant * ffff_m62_f_denoms[m.expo];
}

static const float ffff_m62_ui64_denoms[] = {
	/*00->*/100000000, /*01->*/10000,
	/*10->*/1, /*11->unsupported*/0,
};

static inline m62_t __attribute__((always_inline))
ffff_m62_get_f(float f)
{
	m62_t res;
	uint8_t i = 0;
	float fa = fabsf(f);

	/* simple range check */
	for (i = 0; i < 4; i++) {
		if (fa < ffff_m62_f_ubounds[i]) {
			break;
		}
	}
	res.expo = i;
	res.mant = (int64_t)(f / ffff_m62_f_denoms[i]);
	return res;
}

static const double ffff_m62_d_denoms[] = {
	/*00->*/0.00000001, /*01->*/0.0001,
	/*10->*/1., /*11->*/10000.,
};

static const double ffff_m62_d_ubounds[] = {
	/*00->*/23058430092.13693952, /*01->*/230584300921369.3952,
	/*10->*/2305843009213693952., /*11->*/23058430092136939520000.,
};

static inline double __attribute__((always_inline))
ffff_m62_d(m62_t m)
{
	return (double)m.mant * ffff_m62_d_denoms[m.expo];
}

static inline m62_t __attribute__((always_inline))
ffff_m62_get_d(double d)
{
	m62_t res;
	uint8_t i = 0;
	double fa = fabs(d);

	/* simple range check */
	for (i = 0; i < 4; i++) {
		if (fa < ffff_m62_d_ubounds[i]) {
			break;
		}
	}
	res.expo = i;
	res.mant = (int64_t)(d / ffff_m62_d_denoms[i]);
	return res;
}

static inline uint64_t
__m62_add_helper(m62_t a, m62_t b)
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

#if defined INCLUDED_m30_h_
static inline m62_t
ffff_m62_add_m30(m62_t a, m30_t b)
{
	m62_t res;

	if (a.expo == b.expo) {
		/* common case */
		res.mant = a.mant + b.mant;
		res.expo = a.expo;
	} else if (a.mant == 0) {
		/* easy case too */
		res.mant = b.mant;
		res.expo = b.expo;
	} else {
		/* do me? */
		res.mant = a.mant;
		res.expo = a.expo;
	}
	return res;
}

static inline m62_t
ffff_m62_mul_2m30(m30_t a, m30_t b)
{
	m62_t res;

	if ((a.expo == 1 && b.expo == 2) ||
	    (a.expo == 2 && b.expo == 2)) {
		/* common cases */
		res.mant = (uint64_t)a.mant * (uint64_t)b.mant;
		res.expo = a.expo;
	} else if (a.expo == b.expo && a.expo == 1) {
		res.mant = (uint64_t)a.mant * (uint64_t)b.mant;
		res.expo = 0;
	} else {
		/* no multiplication rule yet */
		res.mant = 0;
		res.expo = a.expo;
	}
	return res;
}

static inline m62_t
ffff_m62_add_mul_2m30(m62_t sum, m30_t a, m30_t b)
{
/* returns summand + a * b */
	m62_t res;
	res.v = 0;
	if (a.expo == 1 && b.expo == 2 && (sum.expo == 1 || sum.v == 0)) {
		/* only case we support */
		res.mant = sum.mant + (uint64_t)a.mant * (uint64_t)b.mant;
		res.expo = 1;
	}
	return res;
}

static inline m30_t
ffff_m30_vwap(m62_t volpri, m62_t vol)
{
	m30_t res;
	res.v = 0;
	if (volpri.expo == 1 && vol.expo == 2 && vol.mant != 0) {
		/* only case we support */
		res.mant = (volpri.mant / vol.mant);
		res.expo = 1;
	}
	return res;
}

static inline m30_t
ffff_m30_from_m62(m62_t v)
{
	m30_t res;
	res.mant = v.mant;
	res.expo = v.expo;
	return res;
}
#endif	/* INCLUDED_m30_h_ */

static inline m62_t
ffff_m62_add(m62_t a, m62_t b)
{
	m62_t res;

	if (a.expo == b.expo) {
		/* common case */
		res.mant = a.mant + b.mant;
		res.expo = a.expo;
	} else if (a.expo > b.expo) {
		res.mant = __m62_add_helper(a, b);
		res.expo = a.expo;
	} else /*if (b.expo > a.expo)*/ {
		/* addition is commutative */
		res.mant = __m62_add_helper(b, a);
		res.expo = b.expo;
	}
	return res;
}

static inline m62_t
ffff_m62_add_ui64(m62_t a, uint64_t b)
{
	m62_t res;

	/* a.expo must be 1 or a.v must be 0 */
	res.mant = a.mant + b * ffff_m62_ui64_denoms[1];
	res.expo = 1;
	return res;
}

static inline m62_t
ffff_m62_add_mul_m30_ui64(m62_t a, m30_t b, uint64_t c)
{
/* -> a + b * c */
	m62_t h;

	h.mant = b.mant * c;
	h.expo = b.expo;
	return ffff_m62_add(a, h);
}

#if !defined DEFINE_GORY_STUFF
static inline m62_t
ffff_m62_get_s(const char **s __attribute__((unused)))
{
	m62_t res;
	res.v = 0;
	return res;
}

static inline size_t __attribute__((unused))
ffff_m62_s(char *restrict buf, m62_t m __attribute__((unused)))
{
	*buf = '\0';
	return 0;
}

#else  /* GORY_STUFF */
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
	res = __62_23_get_s(mant, n) * 100000000;

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

static inline m62_t
ffff_m62_get_s(const char **nptr)
{
	/* spray some pointers */
	const char *mant, *mend, *frac;
	const char *p;
	bool neg = false;
	m62_t r62;

	r62.expo = 1;

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
		r62.expo = 0;
		r62.mant = __62_0_get_s(mant, (mend - mant), frac, p - frac);
	} else if ((p - frac) > 0 ||
		   (uint64_t)(mend - mant) <= ffff_m62_i_ubounds_l10[1]) {
		r62.expo = 1;
		r62.mant = __62_1_get_s(mant, mend - mant, frac, p - frac);
	} else if ((uint64_t)(mend - mant) >= ffff_m62_i_ubounds_l10[2]) {
		r62.expo = 3;
		r62.mant = __62_23_get_s(mant, mend - mant - 4);
	} else {
		r62.expo = 2;
		r62.mant = __62_23_get_s(mant, mend - mant);
	}
	r62.mant = !neg ? r62.mant : -r62.mant;
	return r62;
}

/**
 * Return the number of fractional digits, i.e. the ones right of point. */
static inline int8_t
__m62_nfrac_digits(m62_t m)
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

static inline size_t __attribute__((unused))
ffff_m62_s(char *restrict buf, m62_t m)
{
	char *tmp = buf;
	bool sign;
	int8_t nfrac = __m62_nfrac_digits(m);

	if ((sign = (m.mant < 0))) {
		m.mant = -m.mant;
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

static inline long int
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

#endif	/* GORY_STUFF */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* INCLUDED_m62_h_ */
