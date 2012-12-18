/*** ute-chndl.c -- ute file chandlery
 *
 * Copyright (C) 2008-2012 Sebastian Freundt
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

#define UTE_CHNDL_C
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
#include "ute-chndl.h"

/* we need to look into ticks and tick packets */
#include "sl1t.h"
#include "ssnp.h"
#include "scdl.h"

#define SUSHI_MODE		1
#define TRUE_OPEN		1
#define OPTIM_LEVEL		2

typedef struct bkts_s *bkts_t;
typedef void(*bkt_f)(bkts_t, const_sl1t_t, void *clo);
typedef struct xcand_s *xcand_t;

/* extended ssnap */
struct xcand_s {
	/* bid candle */
	struct scdl_s bc[1];
	/* ask candle */
	struct scdl_s ac[1];
	/* tra candle */
	struct scdl_s tc[1];
	uint32_t bcnt;
	uint32_t acnt;
	uint32_t tcnt;
};

struct bkts_s {
	uint32_t nsyms;
	time_t cur_ts;
	xcand_t cand;
};

/* scand fiddlers */
static inline void
bkts_cleanse(bkts_t b)
{
	memset(b->cand, 0, (b->nsyms + 1) * sizeof(*b->cand));
	return;
}

static bool
xcand_empty_p(xcand_t c)
{
	return c->bcnt == 0U && c->acnt == 0U;
}

static bool
xcand_trades_p(xcand_t c)
{
	return c->tcnt != 0U;
}

static void
xcand_push_l1t(xcand_t c, const_sl1t_t t)
{
	switch (sl1t_ttf(t)) {
	case SL1T_TTF_BID:
		c->bc->c = t->bid;
		if (c->bcnt == 0U) {
			c->bc->o = t->bid;
			c->bc->h = t->bid;
			c->bc->l = t->bid;
		} else if (t->bid > c->bc->h) {
			c->bc->h = t->bid;
		} else if (t->bid < c->bc->l) {
			c->bc->l = t->bid;
		}
		c->bcnt++;
		break;
	case SL1T_TTF_ASK:
		c->ac->c = t->ask;
		if (c->acnt == 0U) {
			c->ac->o = t->ask;
			c->ac->h = t->ask;
			c->ac->l = t->ask;
		} else if (t->ask > c->ac->h) {
			c->ac->h = t->ask;
		} else if (t->ask < c->ac->l) {
			c->ac->l = t->ask;
		}
		c->acnt++;
		break;
	case SL1T_TTF_TRA:
		c->tc->c = t->tra;
		c->tcnt++;
		if (c->tcnt == 0U) {
			c->tc->o = t->tra;
			c->tc->h = t->tra;
			c->tc->l = t->tra;
		} else if (t->tra > c->tc->h) {
			c->tc->h = t->tra;
		} else if (t->tra < c->tc->l) {
			c->tc->l = t->tra;
		}
		break;
	case SL1T_TTF_BIDASK:
		c->bc->c = t->bp;
		if (c->bcnt == 0U) {
			c->bc->o = t->bp;
			c->bc->h = t->bp;
			c->bc->l = t->bp;
		} else if (t->bp > c->bc->h) {
			c->bc->h = t->bp;
		} else if (t->bp < c->bc->l) {
			c->bc->l = t->bp;
		}
		c->bcnt++;

		c->ac->c = t->ap;
		if (c->acnt == 0U) {
			c->ac->o = t->ap;
			c->ac->h = t->ap;
			c->ac->l = t->ap;
		} else if (t->ap > c->ac->h) {
			c->ac->h = t->ap;
		} else if (t->ap < c->ac->l) {
			c->ac->l = t->ap;
		}
		c->acnt++;
		break;
	default:
		break;
	}
	return;
}

static void
xcand_push_snp(xcand_t c, const_ssnp_t snp)
{
	c->bc->c = snp->bp;
	if (c->bcnt == 0U) {
		c->bc->h = snp->bp;
		c->bc->l = snp->bp;
		c->bc->o = snp->bp;
	} else if (snp->bp > c->bc->h) {
		c->bc->h = snp->bp;
	} else if (snp->bp < c->bc->l) {
		c->bc->l = snp->bp;
	}
	c->bcnt++;

	c->ac->c = snp->ap;
	if (c->acnt == 0U) {
		c->ac->h = snp->ap;
		c->ac->l = snp->ap;
		c->ac->o = snp->ap;
	} else if (snp->ap > c->ac->h) {
		c->ac->h = snp->ap;
	} else if (snp->ap < c->ac->l) {
		c->ac->l = snp->ap;
	}
	c->acnt++;
	return;
}

static void
xcand_push_cdl(xcand_t c, const_scdl_t cdl)
{
	switch (scdl_ttf(cdl)) {
	case SL1T_TTF_BID:
		c->bc->c = cdl->c;
		if (c->bcnt == 0U) {
			*c->bc = *cdl;
		} else if (cdl->h > c->bc->h) {
			c->bc->h = cdl->h;
		} else if (cdl->l < c->bc->l) {
			c->bc->l = cdl->l;
		}
		c->bcnt++;
		break;
	case SL1T_TTF_ASK:
		c->ac->c = cdl->c;
		if (c->acnt == 0U) {
			*c->ac = *cdl;
		} else if (cdl->h > c->ac->h) {
			c->ac->h = cdl->h;
		} else if (cdl->l < c->ac->l) {
			c->ac->l = cdl->l;
		}
		c->acnt++;
		break;
	case SL1T_TTF_TRA:
		c->tc->c = cdl->c;
		if (c->tcnt == 0U) {
			*c->tc = *cdl;
		} else if (cdl->h > c->tc->h) {
			c->tc->h = cdl->h;
		} else if (cdl->l < c->tc->l) {
			c->tc->l = cdl->l;
		}
		c->tcnt++;
		break;
	default:
		break;
	}
	return;
}

static void
xcand_push(xcand_t c, scom_t t)
{
	switch (scom_thdr_ttf(t)) {
	case SL1T_TTF_BID:
	case SL1T_TTF_ASK:
	case SL1T_TTF_TRA:
	case SL1T_TTF_BIDASK:
		xcand_push_l1t(c, (const void*)t);
		break;
	case SSNP_FLAVOUR:
		xcand_push_snp(c, (const void*)t);
		break;
	case SCDL_FLAVOUR | SL1T_TTF_BID:
	case SCDL_FLAVOUR | SL1T_TTF_ASK:
	case SCDL_FLAVOUR | SL1T_TTF_TRA:
		xcand_push_cdl(c, (const void*)t);
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
aligned_stamp(chndl_ctx_t ctx, time_t ts)
{
	return (time_t)(ts - ((ts - ctx->opts->offset) % ctx->opts->interval));
}

static inline time_t
next_stamp(chndl_ctx_t ctx, time_t ts)
{
	return ts + ctx->opts->interval;
}

static inline time_t
next_aligned_stamp(chndl_ctx_t ctx, time_t ts)
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
new_candle_p(chndl_ctx_t ctx, scom_t t)
{
	time_t t1 = get_buckets_time(ctx->bkt);
	time_t t2 = scom_thdr_sec(t);

	if (UNLIKELY(t1 == 0)) {
		/* make sure we enter the branch for new candles to
		 * set the bucket time (for the first time) */
		return true;
	} else if (UNLIKELY(cdl_snp_p(t))) {
		/* if a candle or a cand allow on-the-dot stamps too */
		return t2 > t1;
	}
	/* if it's a genuine tick be strict about the times */
	return t2 >= t1;
}

static uint16_t
copy_sym(chndl_ctx_t ctx, uint16_t cidx)
{
	const char *cur_sym = ute_idx2sym(ctx->rdr, cidx);
	if (UNLIKELY(cur_sym == NULL)) {
		return 0;
	}
	return ute_sym2idx(ctx->wrr, cur_sym);
}

static void
write_cand(chndl_ctx_t ctx, uint16_t cidx)
{
	scdl_t c[3];
	uint16_t nidx;
	time_t ts;

	if (xcand_empty_p(ctx->bkt->cand + cidx)) {
		goto check_trades;
	}

	c[0] = ctx->bkt->cand[cidx].bc;
	c[1] = ctx->bkt->cand[cidx].ac;
	nidx = copy_sym(ctx, cidx);
	ts = get_buckets_time(ctx->bkt);

	/* bid candle */
	scom_thdr_set_tblidx(c[0]->hdr, nidx);
	scom_thdr_set_sec(c[0]->hdr, ts);
	scom_thdr_set_msec(c[0]->hdr, 0);
	scom_thdr_set_ttf(c[0]->hdr, SCDL_FLAVOUR | SL1T_TTF_BID);
	/* ask candle */
	scom_thdr_set_tblidx(c[1]->hdr, nidx);
	scom_thdr_set_sec(c[1]->hdr, ts);
	scom_thdr_set_msec(c[1]->hdr, 0);
	scom_thdr_set_ttf(c[1]->hdr, SCDL_FLAVOUR | SL1T_TTF_ASK);

	/* set cnt and sta_ts */
	c[0]->sta_ts = ts - ctx->opts->interval;
	c[0]->cnt = ctx->bkt->cand[cidx].bcnt;
	c[1]->sta_ts = ts - ctx->opts->interval;
	c[1]->cnt = ctx->bkt->cand[cidx].acnt;

	/* kick off */
	ute_add_tick(ctx->wrr, AS_SCOM(c[0]));
	ute_add_tick(ctx->wrr, AS_SCOM(c[1]));

check_trades:
	/* tra candle */
	if (xcand_trades_p(ctx->bkt->cand + cidx)) {
		c[2] = ctx->bkt->cand[cidx].tc;

		scom_thdr_set_tblidx(c[2]->hdr, nidx);
		scom_thdr_set_sec(c[2]->hdr, ts);
		scom_thdr_set_msec(c[2]->hdr, 0);
		scom_thdr_set_ttf(c[2]->hdr, SCDL_FLAVOUR | SL1T_TTF_TRA);

		c[2]->sta_ts = ts - ctx->opts->interval;
		c[2]->cnt = ctx->bkt->cand[cidx].tcnt;

		ute_add_tick(ctx->wrr, AS_SCOM(c[2]));
	}
	return;
}

static void
new_candle(chndl_ctx_t ctx)
{
	if (UNLIKELY(ctx->opts->dryp)) {
		return;
	}

	/* write all the candshots so far */
	for (uint16_t i = 0; i <= ctx->bkt->nsyms; i++) {
		write_cand(ctx, i);
	}
	return;
}

static void
check_candle(chndl_ctx_t ctx, scom_t t)
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
			/* in the candle/cand case we allow the first time to
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
bucketiser(chndl_ctx_t ctx, scom_t t)
{
	uint16_t i = scom_thdr_tblidx(t);
	xcand_t b = ctx->bkt->cand + i;

	check_candle(ctx, t);
	xcand_push(b, t);
	return;
}


/* simple bucket sort */
#include <stdio.h>

static void
init(chndl_ctx_t ctx, chndl_opt_t opt)
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
deinit(chndl_ctx_t ctx)
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
	} else if (ctx->bkt->cand) {
		xcand_t cand = ctx->bkt->cand;
		munmap(cand, (ctx->bkt->nsyms + 1) * sizeof(*cand));
	}
	return;
}

static void
init_buckets(chndl_ctx_t ctx, utectx_t hdl, bkts_t bkt)
{
	size_t nsyms_hdl = ute_nsyms(hdl);
	size_t nsyms_bkt = bkt->nsyms;

	if (nsyms_bkt < nsyms_hdl || (bkt->cand == NULL && nsyms_hdl == 0)) {
		xcand_t cand = bkt->cand;
		size_t sz = (nsyms_hdl + 1) * sizeof(*cand);

		if (cand == NULL) {
			bkt->cand = mmap(cand, sz, PROT_MEM, MAP_MEM, -1, 0);
		} else {
			size_t old = (nsyms_bkt + 1) * sizeof(*cand);
			bkt->cand = mremap(cand, old, sz, MREMAP_MAYMOVE);
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
fini_buckets(chndl_ctx_t ctx)
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
#include "ute-chndl-clo.h"
#include "ute-chndl-clo.c"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
# pragma warning (default:181)
#endif	/* __INTEL_COMPILER */

int
main(int argc, char *argv[])
{
	struct chndl_args_info argi[1];
	struct chndl_ctx_s ctx[1] = {{0}};
	struct chndl_opt_s opt[1] = {{0}};
	struct bkts_s bkt[1] = {{0}};
	int res = 0;

	/* set default values */
	argi->interval_arg = 300;
	argi->modulus_arg = 0;
	argi->zone_arg = NULL;

	if (chndl_parser(argc, argv, argi)) {
		res = 1;
		goto out;
	} else if (argi->help_given) {
		chndl_parser_print_help();
		res = 0;
		goto out;
	}

	/* set up our option struct, just to decouple from gengetopt's struct */
	if (argi->output_given) {
		opt->outfile = argi->output_arg;
	}

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
		UTE_ITER(ti, hdl) {
			bucketiser(ctx, ti);
		}
		/* last round, just emit what we've got */
		new_candle(ctx);

		/* finish our buckets */
		fini_buckets(ctx);
		/* oh right, close the handle */
		ute_close(hdl);
	}
	/* leave a footer and finish the chndl series */
	deinit(ctx);

out:
	if (opt->z != NULL) {
		zif_free(opt->z);
	}
	chndl_parser_free(argi);
	return res;
}
#endif	/* STANDALONE */

/* ute-chndl.c ends here */
