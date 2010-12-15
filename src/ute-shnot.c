/*** shnot.c -- ute snapshots
 *
 * Copyright (C) 2008 - 2010 Sebastian Freundt
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

#define UTE_SHNOT_C
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#if defined HAVE_POPT_H
# include <popt.h>
#endif	/* HAVE_POPT_H */
#include <math.h>
#include <time.h>
#include <sys/mman.h>
#if defined USE_ASSERTIONS
#include <assert.h>
#endif	/* USE_ASSERTIONS */

#define DEFINE_GORY_STUFF	1
#include "sl1t.h"
#include "utefile.h"
#include "mem.h"
#include "date.h"
#include "scdl.h"
#include "m30.h"
#include "m62.h"

#include "ute-shnot.h"
#include "ute-print.h"
#include "ute-opt.h"

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

typedef struct shnot_ctx_s *shnot_ctx_t;
typedef struct shnot_opt_s *shnot_opt_t;

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

struct shnot_ctx_s {
	/* contains the currently processed ute file */
	void *rdr;
	/* contains the ute output file */
	void *wrr;

	/* our buckets */
	struct bkts_s bkt[1];

	/* our options */
	shnot_opt_t opts;
};

struct shnot_opt_s {
	const char **infiles;
	const char *outfile;
	const char *badfile;
	const char *sname;

	/* global options */
	ute_opt_t octx;

	/* time zone info */
	zif_t z;

	int interval;
	int offset;
	int dryp;
};

/* ssnap fiddlers */
static inline void
bkts_cleanse(bkts_t b)
{
	memset(b->snap, 0, b->nsyms * sizeof(*b->snap));
	return;
}

static bool
xsnap_empty_p(xsnap_t sn)
{
	return sn->nt.mant == 0 && sn->sn->bp == 0 && sn->sn->ap == 0;
}

static void
xsnap_push(xsnap_t s, const_sl1t_t t)
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

static inline void
set_buckets_time(bkts_t bctx, time_t new_st)
{
	bctx->cur_ts = new_st;
	return;
}


static inline time_t
aligned_stamp(shnot_ctx_t ctx, time_t ts)
{
	return (time_t)(ts - ((ts - ctx->opts->offset) % ctx->opts->interval));
}

static bool
new_candle_p(shnot_ctx_t ctx, const_sl1t_t t)
{
	time_t t1 = ctx->bkt->cur_ts;
	time_t t2 = aligned_stamp(ctx, sl1t_stmp_sec(t));
	return t1 < t2;
}

static void __attribute__((unused))
write_snap(shnot_ctx_t ctx, uint16_t cidx)
{
	xsnap_t xn;
	ssnap_t sn;

	if (xsnap_empty_p(ctx->bkt->snap + cidx)) {
		return;
	}

	sn = (xn = ctx->bkt->snap + cidx)->sn;
	scom_thdr_set_tblidx(sn->hdr, cidx);
	scom_thdr_set_sec(sn->hdr, ctx->bkt->cur_ts + ctx->opts->interval);
	scom_thdr_set_ttf(sn->hdr, SSNP_FLAVOUR);

	sn->tvpr = ffff_m30_vwap(xn->qpri, xn->nt).v;
	sn->tq = ffff_m30_from_m62(xn->nt).v;

	/* kick off */
	//sl1t_fio_write_ticks(ctx->wrr, (void*)sn, 2);
	return;
}

static void
print_snap_rTI(utectx_t uctx, scom_t st, zif_t z)
{
	char tl[MAX_LINE_LEN];
	const_ssnap_t t = (const void*)st;
	uint16_t si = scom_thdr_tblidx(st);
	uint32_t sec = scom_thdr_sec(st);
	uint16_t msec = scom_thdr_msec(st);
	char *p;

	if ((p = tl + pr_sym(uctx, tl, si)) == tl) {
		*p++ = '0';
	}
	*p++ = '\t';
	p += pr_tsmstz(p, sec, msec, z);
	*p++ = '\t';
	/* sequence is always 0 */
	*p++ = '0';
	*p++ = '\t';

	p += ffff_m30_s(p, (m30_t)t->bp);
	*p++ = '\t';
	p += ffff_m30_s(p, (m30_t)t->bq);
	*p++ = '\t';
	p += ffff_m30_s(p, (m30_t)t->ap);
	*p++ = '\t';
	p += ffff_m30_s(p, (m30_t)t->aq);
	*p++ = '\t';
	p += ffff_m30_s(p, (m30_t)t->tvpr);
	*p++ = '\t';
	p += ffff_m30_s(p, (m30_t)t->tq);
	*p++ = '\t';

	/* tick type is not printed in tty mode */
	*p++ = '\n';
	*p++ = '\0';

	fputs(tl, stdout);
	return;
}

static void
write_snap_fd(shnot_ctx_t ctx, uint16_t cidx)
{
	m62_t qpri;
	m62_t nt;

	if (xsnap_empty_p(ctx->bkt->snap + cidx)) {
		return;
	}

	ctx->bkt->snap[cidx].sn->hdr->tblidx = cidx;
	ctx->bkt->snap[cidx].sn->hdr->sec = ctx->bkt->cur_ts;
	ctx->bkt->snap[cidx].sn->hdr->msec = 0;

	/* compute the tvpr and the tq */
	qpri = ctx->bkt->snap[cidx].qpri;
	nt = ctx->bkt->snap[cidx].nt;
	ctx->bkt->snap[cidx].sn->tvpr = ffff_m30_vwap(qpri, nt).v;
	ctx->bkt->snap[cidx].sn->tq = ffff_m30_from_m62(nt).v;

	/* and off to the generic printer */
	print_snap_rTI(ctx->rdr, (scom_t)ctx->bkt->snap[cidx].sn, ctx->opts->z);
	return;
}

static void
new_candle(shnot_ctx_t ctx)
{
	if (UNLIKELY(ctx->opts->dryp)) {
		return;
	}

	/* write all the snapshots so far */
	for (uint16_t i = 0; i < ctx->bkt->nsyms; i++) {
		write_snap_fd(ctx, i);
	}
	return;
}

static void
check_candle(shnot_ctx_t ctx, const_sl1t_t t)
{
	if (UNLIKELY(new_candle_p(ctx, t))) {
		time_t new_ts = aligned_stamp(ctx, sl1t_stmp_sec(t));
		set_buckets_time(ctx->bkt, new_ts);
		new_candle(ctx);
		bkts_cleanse(ctx->bkt);
	}
	return;
}

static void
bucketiser(shnot_ctx_t ctx, const_sl1t_t t)
{
	uint16_t i = sl1t_tblidx(t);
	xsnap_t b = ctx->bkt->snap + i;

	check_candle(ctx, t);
	xsnap_push(b, t);
	return;
}


/* simple bucket sort */
static void
init(shnot_ctx_t ctx, shnot_opt_t opts)
{
	ctx->opts = opts;
	return;
}

static void
deinit(shnot_ctx_t UNUSED(ctx))
{
	xsnap_t snap = ctx->bkt->snap;
	if (snap) {
		munmap(snap, ctx->bkt->nsyms * sizeof(*snap));
	}
	return;
}

static void
init_buckets(shnot_ctx_t ctx, utectx_t hdl)
{
#define PMEM	(PROT_READ | PROT_WRITE)
#define FMEM	(MAP_ANONYMOUS | MAP_PRIVATE)
	size_t nsyms = ute_nsyms(hdl);

	if (ctx->bkt->nsyms < nsyms) {
		xsnap_t snap = ctx->bkt->snap;
		size_t sz = nsyms * sizeof(*snap);
		ctx->bkt->snap = mmap(snap, sz, PMEM, FMEM, 0, 0);
		ctx->bkt->nsyms = nsyms;
	}
	/* assign the reader */
	ctx->rdr = hdl;
	/* scrub the buckets in b thoroughly */
	bkts_cleanse(ctx->bkt);
	return;
}

static void
fini_buckets(shnot_ctx_t UNUSED(ctx))
{
	return;
}


/* ute shnot FILE [...] */
static const char ute_cmd_shnot_help[] =
	"usage: ute shnot [-i INTERVAL] [-m OFFSET] [-o OUTFILE] FILE ...\n"
	"\n\
Draw market snapshots from given FILEs.\n\
A snapshot at a time T comprises the quotes that last occurred before T.\n\
\n\
-i, --interval SECS  Make a snapshot every SECS seconds (default 300).\n\
-m, --modulus SECS   Start SECS seconds past midnight (default 0).\n\
-z, --zone NAME      Use time zone NAME for tty output (default UTC).\n\
";

static void __attribute__((noreturn))
__shnot_fuck_off_and_die(void)
{
	fputs(ute_cmd_shnot_help, stdout);
	exit(1);
}

static void
ute_cmd_shnot_popt(shnot_opt_t opts, int argc, const char *argv[])
{
	/* input file index */
	int ifi = 0;

	/* first of all make a infiles array as large as argv */
	opts->infiles = malloc(argc * sizeof(char*));

	/* set default values */
	opts->interval = 300;
	opts->offset = 0;
	opts->dryp = 0;
	opts->z = NULL;

	for (int i = 1; i < argc; i++) {
		if (0) {
			;
		} else if (!strcmp(argv[i], "--help") ||
			   !strcmp(argv[i], "-h")) {
			/* --help */
			free(opts->infiles);
			__shnot_fuck_off_and_die();

		} else if (!strcmp(argv[i], "--interval") ||
			   !strcmp(argv[i], "-i")) {
			/* --interval NUM */
			opts->interval = strtol(argv[++i], NULL, 10);

		} else if (argv[i][0] == '-' && argv[i][1] == 'i') {
			/* -iNUM (sans the space) */
			opts->interval = strtol(argv[i] + 2, NULL, 10);

		} else if (!strcmp(argv[i], "--modulus") ||
			   !strcmp(argv[i], "-m")) {
			/* --offset NUM */
			opts->offset = strtol(argv[++i], NULL, 10);

		} else if (argv[i][0] == '-' && argv[i][1] == 'm') {
			/* -mNUM (sans the space) */
			opts->offset = strtol(argv[i] + 2, NULL, 10);

		} else if (!strcmp(argv[i], "--output") ||
			   !strcmp(argv[i], "-o")) {
			/* --output FILE */
			opts->outfile = argv[++i];

		} else if (!strcmp(argv[i], "--zone") ||
			   !strcmp(argv[i], "-z")) {
			/* open our timezone definition */
			opts->z = zif_read_inst(argv[++i]);

		} else {
			/* must be a file to process */
			opts->infiles[ifi++] = argv[i];
		}
	}
	/* finalise the infile list */
	opts->infiles[ifi] = NULL;
	return;
}

static void
ute_cmd_shnot_unpopt(shnot_opt_t opts)
{
	/* just that inline thing */
	free(opts->infiles);
	if (opts->z != NULL) {
		zif_free(opts->z);
	}
	return;
}

static void
ute_cmd_shnot(shnot_opt_t opts)
{
	struct shnot_ctx_s ctx[1] = {{0}};

	/* initialise the buckets */
	init(ctx, opts);
	for (const char **p = opts->infiles; *p; p++) {
		void *hdl;

		if ((hdl = ute_open(*p, UO_RDONLY)) == NULL) {
			continue;
		}
		/* (re)initialise our buckets */
		init_buckets(ctx, hdl);
		/* otherwise print all them ticks */
		for (size_t i = 0; i < ute_nticks(hdl); i++) {
			scom_t ti = ute_seek(hdl, i);
			bucketiser(ctx, (void*)ti);
		}
		/* last round */
		new_candle(ctx);
		/* finish our buckets */
		fini_buckets(ctx);
		/* oh right, close the handle */
		ute_close(hdl);
	}
	/* leave a footer and finish the shnot series */
	deinit(ctx);
	return;
}

int
ute_cmd_shnot_args(ute_opt_t octx, int argc, const char *argv[])
{
	struct shnot_opt_s opts[1] = {{0}};

	/* assign global opts */
	opts->octx = octx;
	/* parse options */
	ute_cmd_shnot_popt(opts, argc, argv);
	/* now call the actual mux command */
	ute_cmd_shnot(opts);
	/* clear our resources */
	ute_cmd_shnot_unpopt(opts);
	return 0;
}

/* ute-shnot.c ends here */
