/*** ssnp.h -- sparse snapshot condensators
 *
 * Copyright (C) 2009-2012 Sebastian Freundt
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

#if !defined INCLUDED_ssnp_h_
#define INCLUDED_ssnp_h_

/* for memset() and friends */
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "scommon.h"

typedef struct ssnp_s *ssnp_t;
typedef const struct ssnp_s *const_ssnp_t;

/* bid/ask packs are ssnp's little brother, only tracks the bid and ask price */
typedef struct sbap_s *sbap_t;
typedef const struct sbap_s *const_sbap_t;

#define SSNP_FLAVOUR	(SL1T_TTF_BIDASK | SCOM_FLAG_LM)
#define SBAP_FLAVOUR	(SL1T_TTF_BIDASK)

/**
 * Snaps are useful whenever bid/ask events occur at the same time. */
/* This one is especially tricky because it exists in a single and
 * a double sandwich version */
struct ssnp_s {
	union scom_thdr_u hdr[1];
	union {
		uint32_t v[6];
		uint64_t w[3];
		struct {
			uint32_t bp;
			uint32_t ap;
			/* this is where the linked mode kicks in */
			uint32_t bq;
			uint32_t aq;
			uint32_t tvpr;
			uint32_t tq;
		};
	} __attribute__((aligned(8)));
};
#define __STRUCT_SSNP_S_DEFINED

/* single sandwich version, coincides with struct ssnp_s */
struct sbap_s {
	union scom_thdr_u hdr[1];
	union {
		uint32_t v[2];
		uint64_t w[1];
		struct {
			uint32_t bp;
			uint32_t ap;
		};
	} __attribute__((aligned(8)));
};


/* header glue */
static inline __attribute__((pure)) uint16_t
ssnp_ttf(const_ssnp_t t)
{
	uint16_t ttf = scom_thdr_ttf(t->hdr);
	if (ttf == SBAP_FLAVOUR || ttf == SSNP_FLAVOUR) {
		return ttf;
	}
	return SCOM_TTF_UNK;
}

static inline void
ssnp_set_ttf(ssnp_t t, uint16_t ttf)
{
	if (ttf & SCOM_FLAG_LM) {
		ttf = SSNP_FLAVOUR;
	} else {
		ttf = SBAP_FLAVOUR;
	}
	scom_thdr_set_ttf(t->hdr, ttf);
	return;
}

static inline uint32_t
ssnp_stmp_sec(const_ssnp_t s)
{
	return scom_thdr_sec(s->hdr);
}

static inline void
ssnp_set_stmp_sec(ssnp_t s, uint32_t ts)
{
	scom_thdr_set_sec(s->hdr, ts);
	return;
}

static inline uint32_t
ssnp_stmp_msec(const_ssnp_t s)
{
	return scom_thdr_msec(s->hdr);
}

static inline void
ssnp_set_stmp_msec(ssnp_t s, uint16_t ms)
{
	scom_thdr_set_msec(s->hdr, ms);
	return;
}

static inline uint16_t
ssnp_tblidx(const_ssnp_t s)
{
	return scom_thdr_tblidx(s->hdr);
}

static inline void
ssnp_set_tblidx(ssnp_t s, uint16_t i)
{
	scom_thdr_set_tblidx(s->hdr, i);
	return;
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* INCLUDED_ssnp_h_ */
