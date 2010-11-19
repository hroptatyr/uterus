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
#include "sumux.h"
#define DEFINE_GORY_STUFF
#include "m30.h"

#if !defined htonll
# define htonll(x)	ntohll(x)
#endif	/* !htonll */
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


static inline int64_t
ntohll(int64_t in)
{
	int64_t upper = ntohl((int32_t)(in & 0xffffffff));
	int64_t lower = ntohl((int32_t)(in >> 32));
	return (upper << 32) | lower;
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
	return;
}

static void
write_cdl(mux_ctx_t ctx, struct dcc_s *tl)
{
/* create one or more sparse ticks, sl1t_t objects */
	uint32_t ts = tl->ts / 1000;
	uint16_t ms = tl->ts % 1000;

	sl1t_set_stmp_sec((void*)c, ts);
	sl1t_set_stmp_msec((void*)c, ms);

	c[0].h = ffff_m30_get_d(tl->h.d).v;
	c[0].l = ffff_m30_get_d(tl->l.d).v;
	c[0].o = ffff_m30_get_d(tl->o.d).v;
	c[0].c = ffff_m30_get_d(tl->c.d).v;
	c[0].sta_ts = ts - 9;
	c[0].cnt = ffff_m30_get_d(tl->v.d).v;
	/* and shove it */
	ute_add_tick(ctx->wrr, AS_SCOM(c));
	ute_add_tick(ctx->wrr, AS_SCOM(c + 1));
	return;
}


static void
prepare(mux_ctx_t UNUSED(ctx))
{
	uint16_t idx = 1;

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
proc_l1(mux_ctx_t ctx)
{
	struct dc_s buf;

	/* rinse, rinse, rinse */
	memset(&buf, 0, sizeof(buf));
	/* unroll the first element, so we dont have to write the
	 * time range table info like fucktards */
	if (!rd1(ctx->infd, &buf)) {
		return;
	}
	/* main loop */
	do {
		write_tick(ctx, &buf);
	} while (rd1(ctx->infd, &buf));
	return;
}

static void
proc_cd(mux_ctx_t ctx)
{
	struct dcc_s buf;

	/* rinse */
	memset(&buf, 0, sizeof(buf));
	/* unroll the first element, so we dont have to write the
	 * time range table info like fucktards */
	if (!rd1c(ctx->infd, &buf)) {
		return;
	}
	/* main loop */
	do {
		write_cdl(ctx, &buf);
	} while (rd1c(ctx->infd, &buf));
	return;
}

void
dukasq_slab(mux_ctx_t ctx)
{
	/* prepare ourselves */
	prepare(ctx);

	proc_l1(ctx);
	return;
}

void
dukasa_slab(mux_ctx_t ctx)
{
	/* prepare ourselves */
	prepare(ctx);

	scdl_set_ttf(c, SL1T_TTF_ASK);
	proc_cd(ctx);
	return;
}

void
dukasb_slab(mux_ctx_t ctx)
{
	/* prepare ourselves */
	prepare(ctx);

	scdl_set_ttf(c, SL1T_TTF_BID);
	proc_cd(ctx);
	return;
}

/* dukas.c ends here */
