/*** m30.h -- integer-based monetary types
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

#ifndef INCLUDED_m30_h_
#define INCLUDED_m30_h_		1

#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
# if defined __GNUC__
#  define restrict	__restrict__
# else
#  define restrict
# endif
#endif /* __cplusplus */

#if !defined UNLIKELY
# define UNLIKELY(x)	__builtin_expect((x), 0)
#endif	/* !UNLIKELY */

#if !defined M30T
# define M30T

union m30_u {
	int32_t v;
	uint32_t u;
	struct {
#if defined WORDS_BIGENDIAN
		uint8_t expo:2;
		int32_t mant:30;
#else  /* !WORDS_BIGENDIAN */
		int32_t mant:30;
		uint8_t expo:2;
#endif	/* WORDS_BIGENDIAN */
	};
} __attribute__((transparent_union));

/* bug in gcc needs this BEHIND the actual union definition */
typedef union m30_u m30_t;
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
 * Read number at string *S as m30_t and update *S to the end of the number. */
extern m30_t ffff_m30_get_s(const char **s);

/**
 * Like ffff_m30_get_s() but for numbers with expo 2 or 3. */
extern m30_t ffff_m30_23_get_s(const char **s);

/**
 * Write m30_t object M into buffer BUF. */
extern size_t ffff_m30_s(char *restrict buf, m30_t m);


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

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* INCLUDED_m30_h_ */
