/*** scdl.h -- sparse candle condensators
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

#if !defined INCLUDED_scdl_h_
#define INCLUDED_scdl_h_

/* for memset() and friends */
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "scommon.h"

#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif	/* !UNLIKELY */

typedef struct scdl_s *scdl_t;
typedef const struct scdl_s *const_scdl_t;
#define AS_SCDL(x)		((scdl_t)(x))
#define AS_CONST_SCDL(x)	((const_scdl_t)(x))

typedef struct sbatcdl_s *sbatcdl_t;
typedef const struct sbatcdl_s *const_sbatcdl_t;
#define AS_SBATCDL(x)		((sbatcdl_t)(x))
#define AS_CONST_SBATCDL(x)	((const_sbatcdl_t)(x))

/* always clear your candles before using them */
struct scdl_s {
	union scom_thdr_u hdr[1];
	union {
		uint32_t v[6];
		uint64_t w[3];
		/* simplified candle */
		struct {
			uint32_t h;
			uint32_t l;
			uint32_t o;
			uint32_t c;

			/* carries the starting stamp of this candle */
			uint32_t sta_ts;
			/* use this free slot for a counter */
			uint32_t cnt;
		};

		struct {
			/* volume, oi, etc. */
			uint64_t vol;
			/* wap (vwap, oiwap) */
			uint32_t wap;
			/* nothing */
			uint32_t:32;

			/* carries the starting stamp of this candle */
			uint32_t:32;
			/* use this free slot for a counter */
			uint32_t:32;
		};
	} __attribute__((aligned(8)));
};
#define __STRUCT_SCDL_S_DEFINED

/**
 * BAT-OHLC,V candles, 4 sl1ts wide. */
struct sbatcdl_s {
	union scom_thdr_u hdr[1];
	union {
		uint32_t v[14];
		uint64_t w[7];
		struct {
			uint32_t bo;
			uint32_t bh;
			uint32_t bl;
			uint32_t bc;

			uint32_t ao;
			uint32_t ah;
			uint32_t al;
			uint32_t ac;

			uint32_t to;
			uint32_t th;
			uint32_t tl;
			uint32_t tc;
			uint32_t tv;

			uint32_t sta_ts;
		};
	} __attribute__((aligned(8)));
};
#define __STRUCT_SBATCDL_S_DEFINED


/* header glue */
/* this makes OHLC{V,S,K} candles use the scom link mode */
#define SCDL_FLAVOUR	SCOM_FLAG_LM

static inline __attribute__((pure)) uint16_t
scdl_ttf(const_scdl_t t)
{
	uint16_t res = scom_thdr_ttf(t->hdr);
	if (res & SCDL_FLAVOUR) {
		res &= ~SCDL_FLAVOUR;
	} else {
		res = 0;
	}
	return res;
}

static inline void
scdl_set_ttf(scdl_t t, uint16_t ttf)
{
	scom_thdr_set_ttf(t->hdr, (uint16_t)(ttf | SCDL_FLAVOUR));
	return;
}

static inline uint32_t
scdl_stmp_sec(const_scdl_t c)
{
	return scom_thdr_sec(c->hdr);
}

static inline void
scdl_set_stmp_sec(scdl_t c, uint32_t ts)
{
	scom_thdr_set_sec(c->hdr, ts);
	return;
}

static inline uint32_t
scdl_stmp_msec(const_scdl_t c)
{
	return scom_thdr_msec(c->hdr);
}

static inline void
scdl_set_stmp_msec(scdl_t c, uint16_t ms)
{
	scom_thdr_set_msec(c->hdr, ms);
	return;
}

static inline uint16_t
scdl_tblidx(const_scdl_t c)
{
	return scom_thdr_tblidx(c->hdr);
}

static inline void
scdl_set_tblidx(scdl_t c, uint16_t i)
{
	scom_thdr_set_tblidx(c->hdr, i);
	return;
}


/* inlines */
static inline void
scdl_reset_cnt(scdl_t tgt)
{
	tgt->cnt = 0;
	return;
}

static inline bool
scdl_reset_p(const_scdl_t tgt)
{
	return tgt->cnt == 0;
}


#if defined __STRUCT_SL1T_S_DEFINED
/* ctor */
static inline struct scdl_s
scdl_ticks(const_sl1t_t t, size_t nt)
{
	struct scdl_s res;
	uint32_t p;

	if (UNLIKELY(nt == 0)) {
		memset(&res, 0, sizeof(res));
		return res;
	}
	res.o = p = t[0].v[0];
	res.c = t[nt - 1].v[0];
	res.cnt = nt;

	/* now the actual candle, minmax finding */
	res.h = res.l = p;
	for (size_t i = 0; i < nt; i++) {
		if (t[i].v[0] > res.h) {
			res.h = t[i].v[0];
		} else if (t[i].v[0] < res.l) {
			res.l = t[i].v[0];
		}
	}
	return res;
}

static inline void
scdl_join_ticks(scdl_t tgt, const_sl1t_t t, size_t nt)
{
	if (UNLIKELY(nt == 0)) {
		/* dont fucking bother */
		return;
	}
	if (tgt->o == 0) {
		tgt->o = tgt->h = tgt->l = t[0].v[0];
	}
	tgt->c = t[nt - 1].v[0];
	tgt->sta_ts = sl1t_stmp_sec(&t[nt - 1]);
	tgt->cnt += nt;

	/* now the actual candle, minmax finding */
	for (size_t i = 0; i < nt; i++) {
		if (t[i].v[0] > tgt->h) {
			tgt->h = t[i].v[0];
		} else if (t[i].v[0] < tgt->l) {
			tgt->l = t[i].v[0];
		}
	}
	return;
}
#endif	/* __STRUCT_SL1T_S_DEFINED */

static inline void
scdl_join(scdl_t tgt, const_scdl_t src)
{
/* join tgt and src into tgt */
	if (UNLIKELY(scdl_reset_p(src))) {
		/* dont fucking bother */
		return;
	}
	if (tgt->o == 0) {
		tgt->o = tgt->h = tgt->l = src->v[0];
	}
	tgt->c = src->c;
	tgt->sta_ts = src->sta_ts;
	tgt->cnt += src->cnt;
	if (src->l < tgt->l) {
		tgt->l = src->l;
	}
	if (src->h > tgt->h) {
		tgt->h = src->h;
	}
	return;
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* INCLUDED_scdl_h_ */
