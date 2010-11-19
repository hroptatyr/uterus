/*** secu.h -- packed securities
 *
 * Copyright (C) 2009 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <sebastian.freundt@ga-group.nl>
 *
 * This file is part of sushi.
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

#if !defined INCLUDED_secu_h_
#define INCLUDED_secu_h_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define USE_PACKED_STRUCTS

/* type muxing
 * uint24_t quodi
 * int24_t quoti (-1 means add-%, interests et al, -2 means mul-%, bonds
 * uint16_t pot */
#if !defined USE_PACKED_STRUCTS
typedef uint64_t su_secu_t;
#else  /* USE_PACKED_STRUCTS */
typedef union {
	struct {
		uint32_t quodi:24;
		int32_t quoti:24 __attribute__((packed));
		uint16_t pot:16;
	};
	uint64_t mux;
} __attribute__((transparent_union)) su_secu_t;
#endif	/* !USE_PACKED_STRUCTS */

/* ctor */
static inline su_secu_t __attribute__((pure))
su_secu(uint32_t quodi, int32_t quoti, uint16_t pot)
{
#if !defined USE_PACKED_STRUCTS
	quodi &= 0xffffff;
	quoti &= 0xffffff;
	return pot | quoti << 16 | (uint64_t)quodi << (16 + 24);
#else  /* USE_PACKED_STRUCTS */
	su_secu_t res;
	res.quodi = quodi;
	res.quoti = quoti;
	res.pot = pot;
	return res;
#endif	/* !USE_PACKED_STRUCTS */
}

static inline su_secu_t __attribute__((pure, const, always_inline))
su_null_secu(void)
{
/* return the empty security */
	su_secu_t res;
	/* just for C--, sigh */
	res.mux = 0;
	return res;
}

static inline bool
su_null_secu_p(su_secu_t s)
{
	return s.mux == 0;
}

static inline uint32_t
su_secu_quodi(su_secu_t s)
{
#if !defined USE_PACKED_STRUCTS
	return s >> (16 + 24);
#else  /* USE_PACKED_STRUCTS */
	return s.quodi;
#endif	/* !USE_PACKED_STRUCTS */
}

static inline int32_t
su_secu_quoti(su_secu_t s)
{
#if !defined USE_PACKED_STRUCTS
	/* the actual value */
	int32_t val = (s >> 16) & 0xffffff;
	/* cope with the msb in this 24bit word */
	int32_t res = (-(val >> 23)) & 0xff000000;
	return val | res;
#else  /* USE_PACKED_STRUCTS */
	return s.quoti;
#endif	/* !USE_PACKED_STRUCTS */
}

static inline uint16_t
su_secu_pot(su_secu_t s)
{
#if !defined USE_PACKED_STRUCTS
	return s & 0xffff;
#else  /* USE_PACKED_STRUCTS */
	return s.pot;
#endif	/* !USE_PACKED_STRUCTS */
}

#if defined USE_PACKED_STRUCTS
/**
 * Return the contents of S as 8-byte integer. */
static inline uint64_t
su_secu_ui64(su_secu_t s)
{
	return s.mux;
}

/**
 * Treat 8-byte integer INP as su_secu_t. */
static inline su_secu_t
su_secu_set_ui64(uint64_t inp)
{
	su_secu_t res;
	res.mux = inp;
	return res;
}
#endif	/* USE_PACKED_STRUCTS */

/**
 * Return true if MATCHEE matches MATCHER.
 * su_secu's match if they are identical or if some components (qd, qt, p)
 * in MATCHER are 0 and their corresponding component in MATCHEE is set. */
static inline bool
su_secu_match_p(su_secu_t matcher, su_secu_t matchee)
{
	if (matcher.mux == matchee.mux) {
		return true;
	} else if ((matcher.mux | matchee.mux) == matchee.mux) {
		return true;
	}
	return false;
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* !INCLUDED_secu_h_ */
