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

#ifndef INCLUDED_m62_h_
#define INCLUDED_m62_h_		1

#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
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
# define M62T

union m62_u {
	int64_t v;
	uint64_t u;
	struct {
#if defined WORDS_BIGENDIAN
		uint64_t mant_lower:32;
		uint8_t expo:2;
		int64_t mant_upper:30;
#else  /* !WORDS_BIGENDIAN */
		int64_t mant_lu:62;
		uint8_t expo:2;
#endif	/* WORDS_BIGENDIAN */
	} __attribute__((packed));

	struct {
		uint32_t lower;
		uint32_t upper;
	};
} __attribute__((transparent_union));

/* bug in gcc needs this BEHIND the actual union definition */
typedef union m62_u m62_t;
#endif	/* !M62T */

static inline __attribute__((pure, always_inline)) int64_t
__m62_mant(m62_t m)
{
#if defined WORDS_BIGENDIAN
	return ((int64_t)m.mant_upper << 32U) | (uint64_t)m.mant_lower;
#else  /* !WORDS_BIGENDIAN */
	return m.mant_lu;
#endif	/* WORDS_BIGENDIAN */
}
#define M62_MANT(x)	__m62_mant(x)

#define M62_EXPO(x)	x.expo

#if defined WORDS_BIGENDIAN
#define M62_SET_MANT(tgt, m)					\
	do {							\
		int64_t __m_mant__ = m;				\
		tgt.mant_lower = __m_mant__ & 0xffffffffU;	\
		tgt.mant_upper = __m_mant__ >> 32U;		\
	} while (0)
#else  /* !WORDS_BIGENDIAN */
#define M62_SET_MANT(tgt, m)			\
	tgt.mant_lu = m
#endif	/* WORDS_BIGENDIAN */
#define M62_SET_EXPO(tgt, x)			\
	tgt.expo = x

#if defined WORDS_BIGENDIAN
#define M62_MANT_DIVSET_10(tgt)						\
	({								\
		int64_t __m_mant__ = M62_MANT(tgt);			\
		int64_t __res__ = __m_mant__ / 10;			\
		tgt.mant_lower = __res__ & 0xfffffffffU;		\
		tgt.mant_upper = __res__ >> 32U;			\
		__res__;})
#else  /* !WORDS_BIGENDIAN */
#define M62_MANT_DIVSET_10(tgt)			\
	tgt.mant_lu /= 10
#endif	/* WORDS_BIGENDIAN */

/* muxer */
static inline m62_t __attribute__((pure, always_inline))
ffff_m62_get_ui64(uint64_t v)
{
	m62_t res;
	res.u = v;
	return res;
}

static inline __attribute__((pure, always_inline)) m62_t
ffff_m62_get_ui32(uint32_t v1, uint32_t v2)
{
	m62_t res;

	res.lower = v2;
	res.upper = v1;
	return res;
}

static inline uint64_t __attribute__((pure, always_inline))
ffff_m62_ui64(m62_t v)
{
	return v.u;
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
 * Read number at string *S as m62_t and update *S to the end of the number. */
extern m62_t ffff_m62_get_s(const char **s);

/**
 * Like `strtol()'. */
extern long int ffff_strtol(const char *p, const char **endp, int b);

/**
 * Write m62_t object M into buffer BUF. */
extern size_t ffff_m62_s(char *restrict buf, m62_t m);

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

	M62_SET_EXPO(res, m30.expo);
#if defined WORDS_BIGENDIAN
	res.mant_lower = m30.mant;
	res.mant_upper = 0;
#else  /* !WORDS_BIGENDIAN */
	res.mant_lu = m30.mant;
#endif	/* WORDS_BIGENDIAN */
	return res;
}
#endif	/* INCLUDED_m30_h_ */


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
	/* get the mant in one piece */
	int64_t mant = M62_MANT(m);
	return (float)mant * ffff_m62_f_denoms[M62_EXPO(m)];
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
	M62_SET_EXPO(res, i);
	M62_SET_MANT(res, (int64_t)(f / ffff_m62_f_denoms[i]));
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
	int64_t mant = M62_MANT(m);
	return (double)mant * ffff_m62_d_denoms[M62_EXPO(m)];
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
	M62_SET_EXPO(res, i);
	M62_SET_MANT(res, (int64_t)(d / ffff_m62_d_denoms[i]));
	return res;
}

static inline int64_t
__m62_add_helper(m62_t a, m62_t b)
{
	switch (M62_EXPO(a) - M62_EXPO(b)) {
		int64_t ma, mb;
	case 1:
		ma = M62_MANT(a);
		mb = M62_MANT(b);
		return ma + mb / 10000;
	case 2:
		ma = M62_MANT(a);
		mb = M62_MANT(b);
		return ma + mb / 10000 / 10000;
	case 3:
		/* no need to do anything, it's out of range */
		return M62_MANT(a);
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

	if (M62_EXPO(a) == b.expo) {
		/* common case */
		M62_SET_MANT(res, M62_MANT(a) + b.mant);
		M62_SET_EXPO(res, M62_EXPO(a));
	} else if (M62_MANT(a) == 0) {
		/* easy case too */
		M62_SET_MANT(res, b.mant);
		M62_SET_EXPO(res, b.expo);
	} else {
		/* do me? */
		M62_SET_MANT(res, M62_MANT(a));
		M62_SET_EXPO(res, M62_EXPO(a));
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
		M62_SET_MANT(res, (uint64_t)a.mant * (uint64_t)b.mant);
		M62_SET_EXPO(res, a.expo);
	} else if (a.expo == b.expo && a.expo == 1) {
		M62_SET_MANT(res, (uint64_t)a.mant * (uint64_t)b.mant);
		M62_SET_EXPO(res, 0);
	} else {
		/* no multiplication rule yet */
		M62_SET_MANT(res, 0);
		M62_SET_EXPO(res, a.expo);
	}
	return res;
}

static inline m62_t
ffff_m62_add_mul_2m30(m62_t sum, m30_t a, m30_t b)
{
/* returns summand + a * b */
	m62_t res;
	res.v = 0;
	if (a.expo == 1 && b.expo == 2 && (M62_EXPO(sum) == 1 || sum.v == 0)) {
		/* only case we support */
		M62_SET_MANT(
			res,
			M62_MANT(sum) + (uint64_t)a.mant * (uint64_t)b.mant);
		M62_SET_EXPO(res, 1);
	}
	return res;
}

static inline m30_t
ffff_m30_vwap(m62_t volpri, m62_t vol)
{
	m30_t res;
	res.v = 0;
	if (M62_EXPO(volpri) == 1 && M62_EXPO(vol) == 2 && M62_MANT(vol) != 0) {
		/* only case we support */
		int64_t mvp = M62_MANT(volpri);
		int64_t mv = M62_MANT(vol);

		res.mant = mvp / mv;
		res.expo = 1;
	}
	return res;
}

static inline m30_t
ffff_m30_from_m62(m62_t v)
{
	m30_t res;
	res.mant = M62_MANT(v);
	res.expo = M62_EXPO(v);
	return res;
}
#endif	/* INCLUDED_m30_h_ */

static inline m62_t
ffff_m62_add(m62_t a, m62_t b)
{
	m62_t res;

	if (M62_EXPO(a) == M62_EXPO(b)) {
		/* common case */
		int64_t ma = M62_MANT(a);
		int64_t mb = M62_MANT(b);

		M62_SET_MANT(res, ma + mb);
		M62_SET_EXPO(res, M62_EXPO(a));
	} else if (M62_EXPO(a) > M62_EXPO(b)) {
		M62_SET_MANT(res, __m62_add_helper(a, b));
		M62_SET_EXPO(res, M62_EXPO(a));
	} else /*if (b.expo > a.expo)*/ {
		/* addition is commutative */
		M62_SET_MANT(res, __m62_add_helper(b, a));
		M62_SET_EXPO(res, M62_EXPO(b));
	}
	return res;
}

static inline m62_t
ffff_m62_add_ui64(m62_t a, uint64_t b)
{
	m62_t res;

	/* a.expo must be 1 or a.v must be 0 */
	M62_SET_MANT(res, M62_MANT(a) + b * ffff_m62_ui64_denoms[1]);
	M62_SET_EXPO(res, 1);
	return res;
}

#if defined INCLUDED_m30_h_
static inline m62_t
ffff_m62_add_mul_m30_ui64(m62_t a, m30_t b, uint64_t c)
{
/* -> a + b * c */
	m62_t h;

	M62_SET_MANT(h, b.mant * c);
	M62_SET_EXPO(h, b.expo);
	return ffff_m62_add(a, h);
}
#endif	/* INCLUDED_m30_h_ */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* INCLUDED_m62_h_ */
