/*** sl1t.h -- sparse level1 ticks
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

#if !defined INCLUDED_sl1t_h_
#define INCLUDED_sl1t_h_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "scommon.h"
#include "utehdr.h"

typedef struct sl1t_s *sl1t_t;
/* more or less a macro to see whether we've been here */
#define AS_SL1T_T(x)		((sl1t_t)(x))
#define AS_SL1T(x)		((sl1t_t)(x))

typedef const struct sl1t_s *const_sl1t_t;
#define CONST_SL1T_T(x)		((const_sl1t_t)(x))
#define AS_CONST_SL1T(x)	((const_sl1t_t)(x))

struct sl1t_s {
	/* +sizeof(su_sl1t_hdr_s) */
	union scom_thdr_u hdr[1];
	/* +32*2 = +64*1 */
	union {
		uint32_t v[2];
		uint64_t w[1];
		/* convenience blabla to access these via common names */
		struct {
			uint32_t tra;
			uint32_t tsz;
		};
		struct {
			uint32_t bid;
			uint32_t bsz;
		};
		struct {
			uint32_t ask;
			uint32_t asz;
		};
		struct {
			uint32_t pri;
			uint32_t qty;
		};
		/* BIDASKs */
		struct {
			uint32_t bp;
			uint32_t ap;
		};
	} __attribute__((aligned(8)));
};
#define __STRUCT_SL1T_S_DEFINED

/* link mode structure */
struct sl1t_lm_s {
	/* +sizeof(su_sl1t_hdr_s) */
	union scom_thdr_u p1;
	union {
		uint32_t v[6];
		uint64_t w[3];
		/* convenience blabla to access these via common names */
		struct {
			uint32_t tra;
			uint32_t tsz;
			uint32_t bid;
			uint32_t bsz;
			uint32_t ask;
			uint32_t asz;
		};
	} __attribute__((aligned(8)));
};
#define __STRUCT_SL1T_LM_S_DEFINED


/* accessors */
static inline __attribute__((pure)) uint32_t
sl1t_stmp_sec(const_sl1t_t t)
{
	return scom_thdr_sec(t->hdr);
}

static inline void
sl1t_set_stmp_sec(sl1t_t t, uint32_t sec)
{
	scom_thdr_set_sec(t->hdr, sec);
	return;
}

static inline __attribute__((pure)) uint16_t
sl1t_stmp_msec(const_sl1t_t t)
{
	return scom_thdr_msec(t->hdr);
}

static inline void
sl1t_set_stmp_msec(sl1t_t t, uint16_t msec)
{
	scom_thdr_set_msec(t->hdr, msec);
	return;
}

static inline __attribute__((pure)) uint16_t
sl1t_tblidx(const_sl1t_t t)
{
	return scom_thdr_tblidx(t->hdr);
}

static inline void
sl1t_set_tblidx(sl1t_t t, uint16_t idx)
{
	scom_thdr_set_tblidx(t->hdr, idx);
	return;
}


/* msec overloading */
static inline __attribute__((pure)) bool
sl1t_nexist_p(const_sl1t_t t)
{
	return scom_thdr_nexist_p(t->hdr);
}

static inline __attribute__((const, pure)) bool
sl1t_cont_p(const_sl1t_t t)
{
	return scom_thdr_cont_p(t->hdr);
}

static inline __attribute__((const, pure)) bool
sl1t_halt_p(const_sl1t_t t)
{
	return scom_thdr_halt_p(t->hdr);
}

static inline __attribute__((pure)) bool
sl1t_padding_p(const_sl1t_t t)
{
	return scom_thdr_padding_p(t->hdr);
}

/**
 * Return true if tick actually contains tick data, as opposed to mere
 * control or flow data. */
static inline __attribute__((pure)) bool
sl1t_proper_p(const_sl1t_t t)
{
	return scom_thdr_proper_p(t->hdr);
}

/* setters */
static inline void
sl1t_mark_nexist(sl1t_t t)
{
	scom_thdr_mark_nexist(t->hdr);
	return;
}

static inline void
sl1t_mark_cont(sl1t_t t)
{
	scom_thdr_mark_cont(t->hdr);
	/* convenience */
	t->v[0] = t->v[1] = 0;
	return;
}

static inline void
sl1t_mark_halt(sl1t_t t)
{
	scom_thdr_mark_halt(t->hdr);
	/* convenience */
	t->v[0] = t->v[1] = 0;
	return;
}

static inline void
sl1t_mark_padding(sl1t_t t)
{
	scom_thdr_mark_padding(t->hdr);
	return;
}

static inline void
sl1t_copy_hdr(sl1t_t tgt, const_sl1t_t src)
{
	scom_thdr_copy(tgt->hdr, src->hdr);
	return;
}


static inline __attribute__((pure)) uint16_t
sl1t_ttf(const_sl1t_t t)
{
	return scom_thdr_ttf(t->hdr);
}

static inline void
sl1t_set_ttf(sl1t_t t, uint16_t ttf)
{
	scom_thdr_set_ttf(t->hdr, ttf);
	return;
}


/* price overloading for MKT orders */
#define SL1T_PRC_MKT	(0xe0000000)
#define SL1T_M30_MKT	((m30_t){.u = SL1T_PRC_MKT})

static inline __attribute__((const, pure)) bool
sl1t_mkt_p(sl1t_t t)
{
	return t->v[0] == SL1T_PRC_MKT;
}

static inline __attribute__((const, pure)) bool
sl1t_mkt_bid_p(sl1t_t t)
{
	return sl1t_mkt_p(t) && scom_thdr_ttf(t->hdr) == SL1T_TTF_BID;
}

static inline __attribute__((const, pure)) bool
sl1t_mkt_ask_p(sl1t_t t)
{
	return sl1t_mkt_p(t) && scom_thdr_ttf(t->hdr) == SL1T_TTF_ASK;
}

static inline void
sl1t_set_mkt(sl1t_t t)
{
	t->v[0] = SL1T_PRC_MKT;
	return;
}

static inline void
sl1t_set_mkt_bid(sl1t_t t)
{
	scom_thdr_set_ttf(t->hdr, SL1T_TTF_BID);
	sl1t_set_mkt(t);
	return;
}

static inline void
sl1t_set_mkt_ask(sl1t_t t)
{
	scom_thdr_set_ttf(t->hdr, SL1T_TTF_ASK);
	sl1t_set_mkt(t);
	return;
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* INCLUDED_sl1t_h_ */
