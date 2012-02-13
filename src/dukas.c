/*** dukas.c -- dukas -> sl1t muxer
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

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <time.h>
#include <string.h>

#include "scommon.h"
#include "sl1t.h"
#include "scdl.h"
#include "utefile.h"
#include "ute-mux.h"
#define DEFINE_GORY_STUFF
#include "m30.h"

#if !defined htonll
# define htonll(x)	ntohll(x)
#endif	/* !htonll */
#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif	/* !LIKELY */
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif	/* !UNLIKELY */
#if !defined UNUSED
# define UNUSED(_x)	__attribute__((unused)) _x
#endif	/* !UNUSED */

typedef union {
	double d;
	int64_t i;
} float64_t;

typedef union {
	float d;
	int32_t i;
} float32_t;

struct dc_s {
	uint64_t ts;
	float64_t ap;
	float64_t bp;
	float64_t aq;
	float64_t bq;
};

struct dcc_s {
	uint64_t ts;
	float64_t o;
	float64_t c;
	float64_t l;
	float64_t h;
	float64_t v;
};

struct dqbi5_s {
	uint32_t ts;
	uint32_t ap;
	uint32_t bp;
	float32_t aq;
	float32_t bq;
};

struct dcbi5_s {
	uint32_t ts;
	uint32_t o;
	uint32_t c;
	uint32_t l;
	uint32_t h;
	float32_t v;
};


/* little helpers */
static inline int64_t
ntohll2(int32_t upper, int32_t lower)
{
	int64_t up64 = ntohl(upper);
	int64_t lo64 = ntohl(lower);
	return (up64 << 32) | lo64;
}

static inline int64_t
ntohll(int64_t in)
{
	int32_t upper = (int32_t)(in & 0xffffffff);
	int32_t lower = (int32_t)(in >> 32);
	return ntohll2(upper, lower);
}

static m30_t
__m30_get_dukas(uint32_t val)
{
/* turns 131555 into 1.31555000 */
	/* built-in knowledge */
	m30_t res;

	if (val < 536870) {
		/* interpreted as 5 digits beyond the decimal point */
		res.mant = val * 1000;
		res.expo = 0;
	} else {
		/* interpreted as 5 digits beyond the decimal point */
		res.mant = val / 10;
		res.expo = 1;
	}
	return res;
}


static struct sl1t_s t[2];
static struct scdl_s c[1];

static bool
rd1(int f, struct dc_s *b)
{
	if (UNLIKELY(read(f, b, sizeof(*b)) <= 0)) {
		return false;
	}
	b->ts = ntohll(b->ts);
	b->ap.i = ntohll(b->ap.i);
	b->bp.i = ntohll(b->bp.i);
	b->aq.i = ntohll(b->aq.i);
	b->bq.i = ntohll(b->bq.i);
	return true;
}

static bool
rd1bi5(int f, struct dqbi5_s *b)
{
	if (UNLIKELY(read(f, b, sizeof(*b)) <= 0)) {
		return false;
	}
	b->ts = ntohl(b->ts);
	b->ap = ntohl(b->ap);
	b->bp = ntohl(b->bp);
	b->aq.i = ntohl(b->aq.i);
	b->bq.i = ntohl(b->bq.i);
	return true;
}

static bool
rd1c(int f, struct dcc_s *b)
{
	if (UNLIKELY(read(f, b, sizeof(*b)) <= 0)) {
		return false;
	}
	b->ts = ntohll(b->ts);
	b->o.i = ntohll(b->o.i);
	b->c.i = ntohll(b->c.i);
	b->l.i = ntohll(b->l.i);
	b->h.i = ntohll(b->h.i);
	b->v.i = ntohll(b->v.i);
	return true;
}

static bool
rd1cbi5(int f, struct dcbi5_s *b)
{
	if (UNLIKELY(read(f, b, sizeof(*b)) <= 0)) {
		return false;
	}
	b->ts = ntohl(b->ts);
	b->o = ntohl(b->o);
	b->c = ntohl(b->c);
	b->l = ntohl(b->l);
	b->h = ntohl(b->h);
	b->v.i = ntohl(b->v.i);
	return true;
}

#define DUKAS_VMUL	(1.0e6)
#define DUKAS_VMULF	(1.0e6f)

static void
write_tick(mux_ctx_t ctx, struct dc_s *tl)
{
/* create one or more sparse ticks, sl1t_t objects */
	uint32_t ts = tl->ts / 1000;
	uint16_t ms = tl->ts % 1000;

	sl1t_set_stmp_sec(t + 0, ts);
	sl1t_set_stmp_msec(t + 0, ms);
	t[0].bid = ffff_m30_get_d(tl->bp.d).v;
	t[0].bsz = ffff_m30_get_d(tl->bq.d).v;

	sl1t_set_stmp_sec(t + 1, ts);
	sl1t_set_stmp_msec(t + 1, ms);
	t[1].ask = ffff_m30_get_d(tl->ap.d).v;
	t[1].asz = ffff_m30_get_d(tl->aq.d).v;

	ute_add_tick(ctx->wrr, AS_SCOM(t));
	ute_add_tick(ctx->wrr, AS_SCOM(t + 1));
	return;
}

static void
write_tick_bi5(mux_ctx_t ctx, struct dqbi5_s *tl)
{
/* create one or more sparse ticks, sl1t_t objects */
	unsigned int ts = tl->ts / 1000;
	unsigned int ms = tl->ts % 1000;

	sl1t_set_stmp_sec(t + 0, ts + ctx->opts->tsoff);
	sl1t_set_stmp_msec(t + 0, (uint16_t)ms);
	t[0].bid = __m30_get_dukas(tl->bp * ctx->opts->mul / ctx->opts->mag).v;
	t[0].bsz = ffff_m30_get_f(tl->bq.d * DUKAS_VMULF).v;

	sl1t_set_stmp_sec(t + 1, ts + ctx->opts->tsoff);
	sl1t_set_stmp_msec(t + 1, (uint16_t)ms);
	t[1].ask = __m30_get_dukas(tl->ap * ctx->opts->mul / ctx->opts->mag).v;
	t[1].asz = ffff_m30_get_f(tl->aq.d * DUKAS_VMULF).v;

	ute_add_tick(ctx->wrr, AS_SCOM(t));
	ute_add_tick(ctx->wrr, AS_SCOM(t + 1));
	return;
}

static void
write_cdl(mux_ctx_t ctx, struct dcc_s *tl, uint32_t cdl_len)
{
/* create one or more sparse ticks, sl1t_t objects */
	uint32_t ts = tl->ts / 1000;
	uint16_t ms = tl->ts % 1000;

	scdl_set_stmp_sec(c + 0, ts + cdl_len);
	scdl_set_stmp_msec(c + 0, ms);

	c[0].h = ffff_m30_get_d(tl->h.d).v;
	c[0].l = ffff_m30_get_d(tl->l.d).v;
	c[0].o = ffff_m30_get_d(tl->o.d).v;
	c[0].c = ffff_m30_get_d(tl->c.d).v;
	c[0].sta_ts = ts;
	c[0].cnt = ffff_m30_get_d(tl->v.d).v;
	/* and shove it */
	ute_add_tick(ctx->wrr, AS_SCOM(c));
	return;
}

static void
write_cdl_bi5(mux_ctx_t ctx, struct dcbi5_s *tl, uint32_t cdl_len)
{
/* create one or more candles, scdl_t objects */
	/* apparently timestamps are in second offsets now */
	unsigned int ts = tl->ts + ctx->opts->tsoff;

	scdl_set_stmp_sec(c + 0, ts + cdl_len);
	scdl_set_stmp_msec(c + 0, 0);

	c[0].h = __m30_get_dukas(tl->h * ctx->opts->mul / ctx->opts->mag).v;
	c[0].l = __m30_get_dukas(tl->l * ctx->opts->mul / ctx->opts->mag).v;
	c[0].o = __m30_get_dukas(tl->o * ctx->opts->mul / ctx->opts->mag).v;
	c[0].c = __m30_get_dukas(tl->c * ctx->opts->mul / ctx->opts->mag).v;
	c[0].sta_ts = ts;
	c[0].cnt = ffff_m30_get_f(tl->v.d * DUKAS_VMULF).v;
	/* and shove it */
	ute_add_tick(ctx->wrr, AS_SCOM(c));
	return;
}


static void
prepare(mux_ctx_t ctx)
{
	uint16_t idx = 0;

	if (LIKELY(ctx->opts->sname != NULL)) {
		idx = ute_sym2idx(ctx->wrr, ctx->opts->sname);
	}

	/* t is static, so set the static components here */
	sl1t_set_ttf(t + 0, SL1T_TTF_BID);
	sl1t_set_tblidx(t + 0, idx);
	sl1t_set_ttf(t + 1, SL1T_TTF_ASK);
	sl1t_set_tblidx(t + 1, idx);
	/* also c is static (and a pun), set their components here */
	scom_thdr_set_tblidx(c->hdr, idx);
	return;
}

static void
proc_l1bi5(mux_ctx_t ctx)
{
	union {
		struct dc_s bin[1];
		struct dqbi5_s bi5[2];
	} buf[1];

	/* rinse, rinse, rinse */
	memset(buf, 0, sizeof(*buf));

	/* read a probe */
	if (UNLIKELY(read(ctx->infd, buf->bi5, sizeof(buf->bi5)) <= 0)) {
		return;
	}
	/* the only thing we can make assumptions about is the timestamp
	 * we check the two stamps in bi5 and compare their distance */
	{
		uint32_t ts0 = ntohl(buf->bi5[0].ts);
		uint32_t ts1 = ntohl(buf->bi5[1].ts);

		if (ts1 - ts0 > 60/*min*/ * 60/*sec*/ * 1000/*msec*/) {
			/* definitely old_fmt */
			goto old_fmt;
		}

		/* quickly polish the probe */
		buf->bi5[0].ts = ts0;
		buf->bi5[0].ap = ntohl(buf->bi5[0].ap);
		buf->bi5[0].bp = ntohl(buf->bi5[0].bp);
		buf->bi5[0].aq.i = ntohl(buf->bi5[0].aq.i);
		buf->bi5[0].bq.i = ntohl(buf->bi5[0].bq.i);

		buf->bi5[1].ts = ts1;
		buf->bi5[1].ap = ntohl(buf->bi5[1].ap);
		buf->bi5[1].bp = ntohl(buf->bi5[1].bp);
		buf->bi5[1].aq.i = ntohl(buf->bi5[1].aq.i);
		buf->bi5[1].bq.i = ntohl(buf->bi5[1].bq.i);
	}
	/* re-use the probe data */
	write_tick_bi5(ctx, buf->bi5 + 0);
	/* main loop */
	do {
		write_tick_bi5(ctx, buf->bi5 + 1);
	} while (rd1bi5(ctx->infd, buf->bi5 + 1));
	return;
old_fmt:
	/* polish the probe */
	buf->bin->ts = ntohll(buf->bin->ts);
	buf->bin->ap.i = ntohll(buf->bin->ap.i);
	buf->bin->bp.i = ntohll(buf->bin->bp.i);
	buf->bin->aq.i = ntohll(buf->bin->aq.i);
	buf->bin->bq.i = ntohll(buf->bin->bq.i);
	/* main loop */
	do {
		write_tick(ctx, buf->bin);
	} while (rd1(ctx->infd, buf->bin));
	return;
}

static void
proc_cdbi5(mux_ctx_t ctx)
{
	union {
		struct dcc_s bin[2];
		struct dcbi5_s bi5[2];
	} buf[1];
	unsigned int cdl_len;

	/* rinse */
	memset(&buf, 0, sizeof(buf));

	/* read a probe */
	if (UNLIKELY(read(ctx->infd, buf->bi5, sizeof(buf->bi5)) <= 0)) {
		return;
	}

	/* again we can only make assumptions about the timestamps
	 * check the two stamps in bi5 and compare their distance */
	{
		uint32_t ts0 = ntohl(buf->bi5[0].ts);
		uint32_t ts1 = ntohl(buf->bi5[1].ts);

		if ((cdl_len = ts1 - ts0) > 60/*min*/ * 60/*sec*/) {
			/* definitely old_fmt */
			goto old_fmt;
		}

		/* quickly polish the probe */
		buf->bi5[0].ts = ts0;
		buf->bi5[0].o = ntohl(buf->bi5[0].o);
		buf->bi5[0].c = ntohl(buf->bi5[0].c);
		buf->bi5[0].l = ntohl(buf->bi5[0].l);
		buf->bi5[0].h = ntohl(buf->bi5[0].h);
		buf->bi5[0].v.i = ntohl(buf->bi5[0].v.i);

		buf->bi5[1].ts = ts1;
		buf->bi5[1].o = ntohl(buf->bi5[1].o);
		buf->bi5[1].c = ntohl(buf->bi5[1].c);
		buf->bi5[1].l = ntohl(buf->bi5[1].l);
		buf->bi5[1].h = ntohl(buf->bi5[1].h);
		buf->bi5[1].v.i = ntohl(buf->bi5[1].v.i);
	}
	/* re-use the probe data */
	write_cdl_bi5(ctx, buf->bi5 + 0, cdl_len);
	/* main loop */
	do {
		write_cdl_bi5(ctx, buf->bi5 + 1, cdl_len);
	} while (rd1cbi5(ctx->infd, buf->bi5 + 1));
	return;

old_fmt:
	/* polish the probe */
	buf->bin->ts = ntohll(buf->bin->ts);
	buf->bin->o.i = ntohll(buf->bin->o.i);
	buf->bin->c.i = ntohll(buf->bin->c.i);
	buf->bin->l.i = ntohll(buf->bin->l.i);
	buf->bin->h.i = ntohll(buf->bin->h.i);
	buf->bin->v.i = ntohll(buf->bin->v.i);
	if (!rd1c(ctx->infd, buf->bin + 1)) {
		/* assume a day candle and exit */
		write_cdl(ctx, buf->bin, 86400U);
		return;
	}
	/* otherwise compute the candle length, we know dukascopy will
	 * not give us overlapping candles, so just diff the time stamps */
	cdl_len = (buf->bin[1].ts - buf->bin->ts) / 1000;
	/* and emit the original candle from the probe data */
	write_cdl(ctx, buf->bin, cdl_len);
	/* main loop */
	do {
		write_cdl(ctx, buf->bin + 1, cdl_len);
	} while (rd1c(ctx->infd, buf->bin + 1));
	return;
}

/* new all in one dukas slabber */
void
dukas_slab(mux_ctx_t ctx)
{
	prepare(ctx);
	switch (ctx->opts->tt) {
	case SL1T_TTF_BID:
	case SL1T_TTF_ASK:
		scdl_set_ttf(c, ctx->opts->tt);
		proc_cdbi5(ctx);
		break;
	default:
		proc_l1bi5(ctx);
		break;
	}
	return;
}

/* dukas.c ends here */
