/*** ute-shnot.c -- snapshooting from ute files
 *
 * Copyright (C) 2008-2015 Sebastian Freundt
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

/* we're just as good as rudi, aren't we? */
#if defined DEBUG_FLAG
# include <assert.h>
#else  /* !DEBUG_FLAG */
# define assert(args...)
#endif	/* DEBUG_FLAG */

#define DEFINE_GORY_STUFF	1
#include "utefile.h"
#include "date.h"
#include "m30.h"
#include "m62.h"
#include "nifty.h"
#include "mem.h"
/* our own goodness */
#include "ute-shnot.h"
#include "cmd-aux.c"

/* we need to look into ticks and tick packets */
#include "sl1t.h"
#include "scdl.h"
#include "ssnp.h"

#define SUSHI_MODE		1
#define TRUE_OPEN		1
#define OPTIM_LEVEL		2

typedef struct bkts_s *bkts_t;
typedef void(*bkt_f)(bkts_t, const_sl1t_t, void *clo);
typedef struct xsnap_s *xsnap_t;

/* extended ssnap */
struct xsnap_s {
	struct ssnp_s sn[1];
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
	return M62_MANT(sn->nt) == 0 && sn->sn->bp == 0 && sn->sn->ap == 0;
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
xsnap_push_q(xsnap_t s, const_sl1t_t t)
{
	switch (sl1t_ttf(t)) {
	case SL1T_TTF_BIDASK:
		s->sn->bp = t->bp;
		s->sn->ap = t->ap;
		break;
	default:
		break;
	}
	return;
}

static void
xsnap_push_snp(xsnap_t s, const_ssnp_t snp)
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
	case SL1T_TTF_BIDASK:
		xsnap_push_q(s, (const void*)t);
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
next_aligned_stamp(shnot_ctx_t ctx, time_t ts)
{
	return next_stamp(ctx, aligned_stamp(ctx, ts));
}

static inline bool
cdl_snp_p(scom_t t)
{
	return (scom_thdr_ttf(t) & SCDL_FLAVOUR) ||
		(scom_thdr_ttf(t) == SSNP_FLAVOUR);
}

static inline bool
new_candle_p(shnot_ctx_t ctx, scom_t t)
{
	time_t t1 = get_buckets_time(ctx->bkt);
	time_t t2 = scom_thdr_sec(t);

	if (UNLIKELY(t1 == 0)) {
		/* make sure we enter the branch for new candles to
		 * set the bucket time (for the first time) */
		return true;
	} else if (UNLIKELY(cdl_snp_p(t))) {
		/* if a candle or a snap allow on-the-dot stamps too */
		return t2 > t1;
	}
	/* if it's a genuine tick be strict about the times */
	return t2 >= t1;
}

static unsigned int
copy_sym(shnot_ctx_t ctx, unsigned int cidx)
{
	const char *cur_sym = ute_idx2sym(ctx->rdr, cidx);
	if (UNLIKELY(cur_sym == NULL)) {
		return 0;
	}
	return ute_sym2idx(ctx->wrr, cur_sym);
}

static void
write_snap(shnot_ctx_t ctx, unsigned int cidx)
{
	xsnap_t xn;
	ssnp_t sn;
	unsigned int nidx;
	time_t ts;

	if (xsnap_empty_p(ctx->bkt->snap + cidx)) {
		return;
	}

	sn = (xn = ctx->bkt->snap + cidx)->sn;
	nidx = copy_sym(ctx, cidx);
	ts = get_buckets_time(ctx->bkt);
	scom_thdr_set_tblidx(sn->hdr, nidx);
	scom_thdr_set_sec(sn->hdr, ts);
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
	for (unsigned int i = 0; i <= ctx->bkt->nsyms; i++) {
		write_snap(ctx, i);
	}
	return;
}

static void
check_candle(shnot_ctx_t ctx, scom_t t)
{
	if (UNLIKELY(new_candle_p(ctx, t))) {
		time_t tts = scom_thdr_sec(t);
		time_t new_ts;

		/* print what we've got */
		new_candle(ctx);

		/* determine and implant new bucket time */
		if (LIKELY(!cdl_snp_p(t) || tts != aligned_stamp(ctx, tts))) {
			new_ts = next_aligned_stamp(ctx, tts);
		} else {
			/* in the candle/snap case we allow the first time to
			 * be the bucket time already because they represent
			 * spans of times already */
			new_ts = tts;
		}
		set_buckets_time(ctx->bkt, new_ts);
		/* also rinse the buckets */
		bkts_cleanse(ctx->bkt);
	}
	return;
}

static void
bucketiser(shnot_ctx_t ctx, scom_t t)
{
	unsigned int i = scom_thdr_tblidx(t);
	xsnap_t b = ctx->bkt->snap + i;

	check_candle(ctx, t);
	xsnap_push(b, t);
	return;
}


/* simple bucket sort */
#include <stdio.h>

static int
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
		return -1;
	} else if ((ctx->wrr = ute_open(outf, UO_CREAT | UO_TRUNC)) == NULL) {
		error("cannot open `%s' for output", outf);
		return -1;
	}
	return 0;
}

static void
deinit(shnot_ctx_t ctx, size_t nsucc)
{
	if (ctx->wrr) {
		/* check if we wrote to a tmp file */
		if (ctx->opts->outfile == NULL) {
			const char *fn;
			if ((fn = ute_fn(ctx->wrr)) && nsucc) {
				puts(fn);
			} else if (fn) {
				(void)unlink(fn);
			}
		} else if (!nsucc) {
			/* better delete the outfile again */
			(void)unlink(ctx->opts->outfile);
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
	size_t nsyms_hdl = ute_nsyms(hdl);
	size_t nsyms_bkt = bkt->nsyms;

	if (nsyms_bkt < nsyms_hdl || (bkt->snap == NULL && nsyms_hdl == 0)) {
		xsnap_t snap = bkt->snap;
		size_t sz = (nsyms_hdl + 1) * sizeof(*snap);

		if (snap == NULL) {
			bkt->snap = mmap(snap, sz, PROT_MEM, MAP_MEM, -1, 0);
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
#include "ute-shnot.yucc"

int
main(int argc, char *argv[])
{
	yuck_t argi[1U];
	struct shnot_ctx_s ctx[1U] = {{0}};
	struct shnot_opt_s opt[1U] = {{0}};
	struct bkts_s bkt[1U] = {{0}};
	size_t nsucc = 0U;
	int rc = 0;

	if (yuck_parse(argi, argc, argv)) {
		rc = 1;
		goto out;
	}

	/* set up our option struct, just to decouple from gengetopt's struct */
	if (argi->output_arg) {
		opt->outfile = argi->output_arg;
	}

	if (argi->zone_arg) {
		opt->z = zif_open(argi->zone_arg);
	}
	if (argi->interval_arg) {
		opt->interval = strtoul(argi->interval_arg, NULL, 10);
	} else {
		opt->interval = 300;
	}
	if (argi->modulus_arg) {
		opt->offset = strtoul(argi->modulus_arg, NULL, 10);
	} else {
		opt->offset = 0;
	}

	/* initialise context */
	if (init(ctx, opt) < 0) {
		rc = 1;
		goto out;
	}

	for (size_t j = 0U; j < argi->nargs; j++) {
		const char *f = argi->args[j];
		void *hdl;

		if ((hdl = ute_open(f, UO_RDONLY)) == NULL) {
			rc = 2;
			continue;
		}
		/* (re)initialise our buckets */
		init_buckets(ctx, hdl, bkt);
		/* otherwise print all them ticks */
		for (scom_t ti; (ti = ute_iter(hdl)) != NULL;) {
			/* now to what we always do */
			bucketiser(ctx, ti);
		}
		/* last round, just emit what we've got */
		new_candle(ctx);

		/* finish our buckets */
		fini_buckets(ctx);
		/* oh right, close the handle */
		ute_close(hdl);
		/* count this run as success */
		nsucc++;
	}
	/* leave a footer and finish the shnot series */
	deinit(ctx, nsucc);

out:
	if (opt->z != NULL) {
		zif_close(opt->z);
	}
	yuck_free(argi);
	return rc;
}
#endif	/* STANDALONE */

/* ute-shnot.c ends here */
