/*** ute-shnot.c -- snapshooting from ute files
 *
 * Copyright (C) 2008 - 2012 Sebastian Freundt
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

#define UTE_SHNOT_C
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#if defined USE_ASSERTIONS
#include <assert.h>
#endif	/* USE_ASSERTIONS */

#define DEFINE_GORY_STUFF	1
#include "utefile.h"
#include "date.h"
#include "m30.h"
#include "m62.h"
/* our own goodness */
#include "ute-shnot.h"

/* we need to look into ticks and tick packets */
#include "sl1t.h"
#include "scdl.h"

#define SUSHI_MODE		1
#define TRUE_OPEN		1
#define OPTIM_LEVEL		2

#if !defined countof
# define countof(x)		(sizeof(x) / sizeof(*x))
#endif	/* !countof */
#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif	/* !LIKELY */
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif	/* !UNLIKELY */
#if !defined UNUSED
# define UNUSED(_x)	__attribute__((unused)) _x
#endif	/* !UNUSED */

typedef struct bkts_s *bkts_t;
typedef void(*bkt_f)(bkts_t, const_sl1t_t, void *clo);
typedef struct xsnap_s *xsnap_t;

/* extended ssnap */
struct xsnap_s {
	struct ssnap_s sn[1];
	m62_t nt;
	m62_t qpri;
};

struct bkts_s {
	uint32_t nsyms;
	time_t cur_ts;
	xsnap_t snap;
};

/* ssnap fiddlers */
static inline void
bkts_cleanse(bkts_t b)
{
	memset(b->snap, 0, (b->nsyms + 1) * sizeof(*b->snap));
	return;
}

static bool
xsnap_empty_p(xsnap_t sn)
{
	return sn->nt.mant == 0 && sn->sn->bp == 0 && sn->sn->ap == 0;
}

static void
xsnap_push_l1t(xsnap_t s, const_sl1t_t t)
{
	switch (sl1t_ttf(t)) {
	case SL1T_TTF_BID:
		s->sn->bp = t->bid;
		s->sn->bq = t->bsz;
		break;
	case SL1T_TTF_ASK:
		s->sn->ap = t->ask;
		s->sn->aq = t->asz;
		break;
	case SL1T_TTF_TRA:
		/* redo me */
		s->nt = ffff_m62_add_m30(s->nt, (m30_t)t->tsz);
		s->qpri = ffff_m62_add_mul_2m30(
			s->qpri, (m30_t)t->tra, (m30_t)t->tsz);
		break;
	default:
		break;
	}
	return;
}

static void
xsnap_push_snp(xsnap_t s, const_ssnap_t snp)
{
	s->sn->bp = snp->bp;
	s->sn->ap = snp->ap;
	s->sn->bq = snp->bq;
	s->sn->aq = snp->aq;
	/* shouldn't we rescale this? */
	s->sn->tvpr = snp->tvpr;
	s->sn->tq = snp->tq;
	return;
}

static void
xsnap_push_cdl(xsnap_t s, const_scdl_t c)
{
	switch (scdl_ttf(c)) {
	case SL1T_TTF_BID:
		s->sn->bp = c->c;
		s->sn->bq = c->cnt;
		break;
	case SL1T_TTF_ASK:
		s->sn->ap = c->c;
		s->sn->aq = c->cnt;
		break;
	case SL1T_TTF_TRA:
		s->sn->tvpr = c->c;
		s->sn->tq = c->cnt;
		break;
	default:
		break;
	}
	return;
}

static void
xsnap_push(xsnap_t s, scom_t t)
{
	switch (scom_thdr_ttf(t)) {
	case SL1T_TTF_BID:
	case SL1T_TTF_ASK:
	case SL1T_TTF_TRA:
		xsnap_push_l1t(s, (const void*)t);
		break;
	case SSNP_FLAVOUR:
		xsnap_push_snp(s, (const void*)t);
		break;
	case SL1T_TTF_BID | SCOM_FLAG_LM:
	case SL1T_TTF_ASK | SCOM_FLAG_LM:
	case SL1T_TTF_TRA | SCOM_FLAG_LM:
		xsnap_push_cdl(s, (const void*)t);
		break;
	default:
		break;
	}
	return;
}

static inline void
set_buckets_time(bkts_t bctx, time_t new_st)
{
	bctx->cur_ts = new_st;
	return;
}

static inline time_t
get_buckets_time(bkts_t bctx)
{
	return bctx->cur_ts;
}


static inline time_t
aligned_stamp(shnot_ctx_t ctx, time_t ts)
{
	return (time_t)(ts - ((ts - ctx->opts->offset) % ctx->opts->interval));
}

static inline time_t
next_stamp(shnot_ctx_t ctx, time_t ts)
{
	return ts + ctx->opts->interval;
}

static inline time_t
__attribute__((unused))
next_aligned_stamp(shnot_ctx_t ctx, time_t ts)
{
	return next_stamp(ctx, aligned_stamp(ctx, ts));
}

static inline bool
new_candle_p(shnot_ctx_t ctx, scom_t t)
{
	time_t t1 = get_buckets_time(ctx->bkt);
	time_t t2 = scom_thdr_sec(t);
	time_t t3 = aligned_stamp(ctx, t2);

	if (UNLIKELY(t1 == 0)) {
		/* t's time becomes the bucket time
		 * well the aligned stamp of t does, this is not strictly
		 * correct but necessary to produce equally-sized candles */
		set_buckets_time(ctx->bkt, t3);
		return false;
	} else if (UNLIKELY(scom_thdr_ttf(t) >= SCOM_FLAG_LM)) {
		/* if a candle or a snap allow on-the-dot stamps too */
		return t1 < t3 && t2 > t3;
	}
	/* if it's a genuine tick be strict about the times */
	return t1 < t3;
}

static uint16_t
copy_sym(shnot_ctx_t ctx, uint16_t cidx)
{
	const char *cur_sym = ute_idx2sym(ctx->rdr, cidx);
	if (UNLIKELY(cur_sym == NULL)) {
		return 0;
	}
	return ute_sym2idx(ctx->wrr, cur_sym);
}

static void
write_snap(shnot_ctx_t ctx, uint16_t cidx)
{
	xsnap_t xn;
	ssnap_t sn;
	uint16_t nidx;

	if (xsnap_empty_p(ctx->bkt->snap + cidx)) {
		return;
	}

	sn = (xn = ctx->bkt->snap + cidx)->sn;
	nidx = copy_sym(ctx, cidx);
	scom_thdr_set_tblidx(sn->hdr, nidx);
	scom_thdr_set_sec(sn->hdr, ctx->bkt->cur_ts + ctx->opts->interval);
	scom_thdr_set_msec(sn->hdr, 0);
	scom_thdr_set_ttf(sn->hdr, SSNP_FLAVOUR);

	/* compute the tvpr and the tq */
	sn->tvpr = ffff_m30_vwap(xn->qpri, xn->nt).v;
	sn->tq = ffff_m30_from_m62(xn->nt).v;

	/* kick off */
	ute_add_tick(ctx->wrr, AS_SCOM(sn));
	return;
}

static void
new_candle(shnot_ctx_t ctx)
{
	if (UNLIKELY(ctx->opts->dryp)) {
		return;
	}

	/* write all the snapshots so far */
	for (uint16_t i = 0; i <= ctx->bkt->nsyms; i++) {
		write_snap(ctx, i);
	}
	return;
}

static void
check_candle(shnot_ctx_t ctx, scom_t t)
{
	if (UNLIKELY(new_candle_p(ctx, t))) {
		time_t new_ts = aligned_stamp(ctx, scom_thdr_sec(t));
		new_candle(ctx);
		set_buckets_time(ctx->bkt, new_ts);
		bkts_cleanse(ctx->bkt);
	}
	return;
}

static void
bucketiser(shnot_ctx_t ctx, scom_t t)
{
	uint16_t i = scom_thdr_tblidx(t);
	xsnap_t b = ctx->bkt->snap + i;

	check_candle(ctx, t);
	xsnap_push(b, t);
	return;
}


/* simple bucket sort */
#include <stdio.h>

static void
init(shnot_ctx_t ctx, shnot_opt_t opt)
{
	const char *outf = opt->outfile;

	/* start with a rinse, keep our opts though */
	memset(ctx, 0, sizeof(*ctx));
	/* just so we know where our options are */
	ctx->opts = opt;

	if (outf == NULL) {
		/* what if outfile == infile? */
		ctx->wrr = ute_mktemp(0);

	} else if (outf[0] == '-' && outf[1] == '\0') {
		/* bad idea */
		ctx->wrr = NULL;
		fputs("This is binary data, cannot dump to stdout\n", stderr);
		exit(1);
	} else {
		ctx->wrr = ute_open(outf, UO_CREAT | UO_TRUNC);
	}
	return;
}

static void
deinit(shnot_ctx_t ctx)
{
	if (ctx->wrr) {
		/* check if we wrote to a tmp file */
		if (ctx->opts->outfile == NULL) {
			const char *fn;
			if ((fn = ute_fn(ctx->wrr))) {
				puts(fn);
			}
		}
		/* writing those ticks to the disk is paramount */
		ute_close(ctx->wrr);
	}
	if (ctx->bkt == NULL) {
		;
	} else if (ctx->bkt->snap) {
		xsnap_t snap = ctx->bkt->snap;
		munmap(snap, (ctx->bkt->nsyms + 1) * sizeof(*snap));
	}
	return;
}

static void
init_buckets(shnot_ctx_t ctx, utectx_t hdl, bkts_t bkt)
{
#define PMEM	(PROT_READ | PROT_WRITE)
#define FMEM	(MAP_ANONYMOUS | MAP_PRIVATE)
	size_t nsyms_hdl = ute_nsyms(hdl);
	size_t nsyms_bkt = bkt->nsyms;

	if (nsyms_bkt < nsyms_hdl || (bkt->snap == NULL && nsyms_hdl == 0)) {
		xsnap_t snap = bkt->snap;
		size_t sz = (nsyms_hdl + 1) * sizeof(*snap);

		if (snap == NULL) {
			bkt->snap = mmap(snap, sz, PMEM, FMEM, 0, 0);
		} else {
			size_t old = (nsyms_bkt + 1) * sizeof(*snap);
			bkt->snap = mremap(snap, old, sz, MREMAP_MAYMOVE);
		}
		bkt->nsyms = nsyms_hdl;
	}
	/* assign the reader */
	ctx->rdr = hdl;
	/* scrub the buckets in b thoroughly */
	bkts_cleanse(bkt);
	/* and assign buckets to ctx */
	ctx->bkt = bkt;
	return;
}

static void
fini_buckets(shnot_ctx_t ctx)
{
	if (ctx->bkt) {
		/* reset the bucket time */
		set_buckets_time(ctx->bkt, 0);
	}
	return;
}


#if defined STANDALONE
#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#endif	/* __INTEL_COMPILER */
#include "ute-shnot-clo.h"
#include "ute-shnot-clo.c"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
# pragma warning (default:181)
#endif	/* __INTEL_COMPILER */

int
main(int argc, char *argv[])
{
	struct shnot_args_info argi[1];
	struct shnot_ctx_s ctx[1] = {{0}};
	struct shnot_opt_s opt[1] = {{0}};
	struct bkts_s bkt[1] = {{0}};
	int res = 0;

	/* set default values */
	argi->interval_arg = 300;
	argi->modulus_arg = 0;
	argi->zone_arg = NULL;

	if (shnot_parser(argc, argv, argi)) {
		res = 1;
		goto out;
	}

	/* set up our option struct, just to decouple from gengetopt's struct */
	if (argi->zone_given) {
		opt->z = zif_read_inst(argi->zone_arg);
	}
	opt->interval = argi->interval_arg;
	opt->offset = argi->modulus_arg;

	/* initialise context */
	init(ctx, opt);

	for (unsigned int j = 0; j < argi->inputs_num; j++) {
		const char *f = argi->inputs[j];
		void *hdl;

		if ((hdl = ute_open(f, UO_RDONLY)) == NULL) {
			continue;
		}
		/* (re)initialise our buckets */
		init_buckets(ctx, hdl, bkt);
		/* otherwise print all them ticks */
		for (size_t i = 0; i < ute_nticks(hdl);) {
			scom_t ti = ute_seek(hdl, i);
			bucketiser(ctx, ti);
			i += scom_thdr_size(ti) / sizeof(struct sl1t_s);
		}
		/* last round, just emit what we've got */
		new_candle(ctx);

		/* finish our buckets */
		fini_buckets(ctx);
		/* oh right, close the handle */
		ute_close(hdl);
	}
	/* leave a footer and finish the shnot series */
	deinit(ctx);

out:
	if (opt->z != NULL) {
		zif_free(opt->z);
	}
	shnot_parser_free(argi);
	return res;
}
#endif	/* STANDALONE */

/* ute-shnot.c ends here */
