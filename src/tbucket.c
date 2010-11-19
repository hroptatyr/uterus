/*** tbucket.c -- tick buckets, to group by contract id
 *
 * Copyright (C) 2009 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <sebastian.freundt@ga-group.nl>
 *
 * This file is part of unserding.
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

#include <string.h>
#include "sushi.h"
#include "tbucket.h"

struct tbucket_s {
	struct sushi_cmbcdl_s cdl;
	size_t sz;
#if OPTIM_LEVEL >= 2
	index32_t nbids;
	index32_t nasks;
	index32_t ntras;
#else  /* OPTIM_LEVEL < 2 */
	index32_t nticks;
#endif	/* OPTIM_LEVEL */
	struct tick_s ticks[];
};


/* buckets */
static tbucket_t
make_tbucket(size_t nbuckets)
{
	struct tbucket_s *res;
#if OPTIM_LEVEL >= 2
	size_t bsz = (sizeof(*res) + 3 * nbuckets * sizeof(*res->ticks));
#else  /* OPTIM_LEVEL < 2 */
	size_t bsz = (sizeof(*res) + nbuckets * sizeof(*res->ticks));
#endif	/* OPTIM_LEVEL */

	res = xmalloc(bsz);
	memset(res, 0, bsz);
	init_cmbcdl(&res->cdl);
	res->sz = nbuckets;
	return res;
}

static void
free_tbucket(tbucket_t b)
{
	xfree(b);
	return;
}

/* accessors */
static inline bool
tbucket_full_p(tbucket_t b)
{
#if OPTIM_LEVEL >= 2
	return b->nbids >= b->sz || b->nasks >= b->sz || b->ntras >= b->sz;
#else  /* OPTIM_LEVEL < 2 */
	return b->nticks >= b->sz;
#endif	/* OPTIM_LEVEL */
}

static inline void*
tbucket_candle(tbucket_t b)
{
	return &b->cdl;
}

/* modifiers */
static inline void
tbucket_flush(tbucket_t b)
{
#if OPTIM_LEVEL >= 2
	b->nbids = 0;
	b->nasks = 0;
	b->ntras = 0;
#else  /* OPTIM_LEVEL < 2 */
	b->nticks = 0;
#endif	/* OPTIM_LEVEL */
	return;
}

/* i'm not sure if it's wise to expose sushi intrinsics like TRUE_OPEN here */
static inline void
tbucket_rinse(tbucket_t b)
{
#if defined TRUE_OPEN
/* no need to bother with them old candles */
	/* type-punning! */
	WIPE_CID(*((cid_t*)b));
#endif	/* TRUE_OPEN */
	/* type-punning^2! */
	reinit_cmbcdl((sushi_cmbcdl_t)b);
	tbucket_flush(b);
	return;
}

static inline bool
tbucket_rinsed_p(tbucket_t b)
{
#if defined TRUE_OPEN
	return NULL_CID_P(b->cdl.cid);
#else  /* !TRUE_OPEN */
	return b->nticks == 0;
#endif	/* TRUE_OPEN */
}

static inline bool
tbucket_cid_eq_p(tbucket_t b, cid_t cid)
{
	return cid_equal_p(b->cdl.cid, cid);
}

static inline void
tbucket_set_cid(tbucket_t b, cid_t cid)
{
	ASS_CID(b->cdl.cid, cid);
	return;
}

#if OPTIM_LEVEL >= 2
static inline tick_t
tbucket_next_bid(tbucket_t b)
{
	return &b->ticks[0 * b->sz + b->nbids++];
}

static inline tick_t
tbucket_next_ask(tbucket_t b)
{
	return &b->ticks[1 * b->sz + b->nasks++];
}

static inline tick_t
tbucket_next_tra(tbucket_t b)
{
	return &b->ticks[2 * b->sz + b->ntras++];
}

static inline tick_t
tbucket_next_tick(tbucket_t b, tick_type_t tt)
{
	switch (tt) {
	case TT_BID:
		return tbucket_next_bid(b);
	case TT_ASK:
		return tbucket_next_ask(b);
	case TT_TRA:
		return tbucket_next_tra(b);
	default:
		return NULL;
	}
}

#else  /* OPTIM_LEVEL < 2 */

static inline tick_t
tbucket_next_tick(tbucket_t b, tick_type_t UNUSED(tt))
{
	return &b->ticks[b->nticks++];
}
#endif	/* OPTIM_LEVEL */

static void
tbucket_add_tick(tbucket_t b, time_t ts, tick_type_t tt, m32_t p, q32_t q)
{
	tick_t t;

	if (LIKELY((t = tbucket_next_tick(b, tt)) != NULL)) {
		t->tstt = tstt(ts, tt);
		t->pri = p;
		t->qty = q;
	}
	return;
}

static void
tbucket_add_2ticks(
	tbucket_t b, time_t ts,
	tick_type_t tt1, m32_t p1, q32_t q1,
	tick_type_t tt2, m32_t p2, q32_t q2)
{
	tick_t t[2] = {
		tbucket_next_tick(b, tt1),
		tbucket_next_tick(b, tt2)
	};

	if (LIKELY(t[0] != NULL)) {
		t[0]->tstt = tstt(ts, tt1);
		t[0]->pri = p1;
		t[0]->qty = q1;
	}
	if (LIKELY(t[1] != NULL)) {
		t[1]->tstt = tstt(ts, tt2);
		t[1]->pri = p2;
		t[1]->qty = q2;
	}
	return;
}

static void
tbucket_add_3ticks(
	tbucket_t b, time_t ts,
	tick_type_t tt1, m32_t p1, q32_t q1,
	tick_type_t tt2, m32_t p2, q32_t q2,
	tick_type_t tt3, m32_t p3, q32_t q3)
{
	tick_t t[3] = {
		tbucket_next_tick(b, tt1),
		tbucket_next_tick(b, tt2),
		tbucket_next_tick(b, tt3)
	};

	if (LIKELY(t[0] != NULL)) {
		t[0]->tstt = tstt(ts, tt1);
		t[0]->pri = p1;
		t[0]->qty = q1;
	}
	if (LIKELY(t[1] != NULL)) {
		t[1]->tstt = tstt(ts, tt2);
		t[1]->pri = p2;
		t[1]->qty = q2;
	}
	if (LIKELY(t[2] != NULL)) {
		t[2]->tstt = tstt(ts, tt3);
		t[2]->pri = p3;
		t[2]->qty = q3;
	}
	return;
}


static inline void
__chandler_1(sushi_cmbcdl_t cdl, tick_t t)
{
	switch (tstt_tt(t->tstt)) {
	case TT_BID:
		guts_improve_shncdl(&cdl->bid, t);
		break;
	case TT_ASK:
		guts_improve_shncdl(&cdl->ask, t);
		break;
	case TT_TRA:
		guts_improve_shncdl(&cdl->tra, t);
		cdl->vol += t->qty;
		break;
	case TT_BIDASK:
	case TT_UNKNOWN:
	default:
		abort();
		break;
	}
	return;
}

#if OPTIM_LEVEL < 2
static sushi_cmbcdl_t
sushi_chandler(tbucket_t b)
{
	sushi_cmbcdl_t cdl = tbucket_candle(b);

	for (index_t i = 0; i < b->nticks; i++) {
		__chandler_1(cdl, &b->ticks[i]);
	}
	return tbucket_candle(b);
}

#else  /* OPTIM_LEVEL >= 2 */

static sushi_cmbcdl_t
sushi_chandler(tbucket_t b)
{
	sushi_cmbcdl_t cdl = tbucket_candle(b);

	for (index_t o = 0 * b->sz, i = 0; i < o + b->nbids; i++) {
		(void)guts_improve_shncdl(&cdl->bid, &b->ticks[i]);
	}
	for (index_t o = 1 * b->sz, i = o; i < o + b->nasks; i++) {
		(void)guts_improve_shncdl(&cdl->ask, &b->ticks[i]);
	}
	for (index_t o = 2 * b->sz, i = o; i < o + b->ntras; i++) {
		(void)guts_improve_shncdl(&cdl->tra, &b->ticks[i]);
	}
	return tbucket_candle(b);
}
#endif	/* OPTIM_LEVEL */

#undef TBUCKET

/* tbucket.c ends here */
