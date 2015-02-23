/*** m30-rela.h -- integer-based monetary types, relational ops
 *
 * Copyright (C) 2012-2015 Sebastian Freundt
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

#ifndef INCLUDED_m30_rela_h_
#define INCLUDED_m30_rela_h_		1

#include <stdbool.h>
#include "m30.h"

#ifdef __cplusplus
extern "C" {
# if defined __GNUC__
#  define restrict	__restrict__
# else
#  define restrict
# endif
#endif /* __cplusplus */

#if !defined LIKELY
# define LIKELY(x)	__builtin_expect((x), 1)
#endif	/* !LIKELY */
#if !defined UNLIKELY
# define UNLIKELY(x)	__builtin_expect((x), 0)
#endif	/* !UNLIKELY */
#if !defined PURE_CONST
# define PURE_CONST	__attribute__((const, pure))
#endif	/* !PURE_CONST */

/**
 * Return true iff A's value is less than B's */
static inline bool PURE_CONST
m30_lt_p(m30_t a, m30_t b)
{
	switch (a.expo - b.expo) {
	case 0:
		return a.mant < b.mant;
	case 1:
		if (UNLIKELY(a.mant == b.mant / 10000)) {
			return a.mant * 10000 < b.mant;
		}
		return a.mant < b.mant / 10000;

	case -1:
		if (UNLIKELY(a.mant / 10000 == b.mant)) {
			return a.mant < b.mant * 10000;
		}
		return a.mant / 10000 < b.mant;
	case 2:
	case 3:
		/* a is too large to be less than b */
		return false;
	case -2:
	case -3:
		/* a is too small to not be less than b */
		return true;
	default:
		break;
	}
	/* should not be reached */
	return false;
}

/**
 * Return true iff A's value is greater than B's */
static inline bool PURE_CONST
m30_gt_p(m30_t a, m30_t b)
{
	switch (a.expo - b.expo) {
	case 0:
		return a.mant == b.mant;
	case 1:
		if (UNLIKELY(a.mant == b.mant / 10000)) {
			return a.mant * 10000 > b.mant;
		}
		return false;

	case -1:
		if (UNLIKELY(a.mant / 10000 == b.mant)) {
			return a.mant > b.mant * 10000;
		}
		return false;
	case 2:
	case 3:
		/* a is too large to not be greater than b */
		return true;
	case -2:
	case -3:
		/* a is too small to be greater than b */
		return false;
	default:
		break;
	}
	/* we're fucked when we get here */
	return false;
}

/**
 * Return true iff A's value equals B's */
static inline bool PURE_CONST
m30_eq_p(m30_t a, m30_t b)
{
	switch (a.expo - b.expo) {
	case 0:
		return a.mant == b.mant;
	case 1:
		if (UNLIKELY(a.mant == b.mant / 10000)) {
			return a.mant * 10000 == b.mant;
		}
		return false;

	case -1:
		if (UNLIKELY(a.mant / 10000 == b.mant)) {
			return a.mant == b.mant * 10000;
		}
		return false;
	case 2:
	case -2:
	case 3:
	case -3:
		/* a or b is too large to equal b or a */
		return false;
	default:
		break;
	}
	/* we're fucked when we get here */
	return false;
}

/**
 * Return true iff A's value does not equal B's */
static inline bool PURE_CONST
m30_ne_p(m30_t a, m30_t b)
{
	return !(m30_eq_p(a, b));
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* INCLUDED_m30_rela_h_ */
