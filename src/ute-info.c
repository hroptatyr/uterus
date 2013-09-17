/*** ute-info.c -- ute file checker
 *
 * Copyright (C) 2012 Sebastian Freundt
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

/** test client for libuterus */
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <fcntl.h>
#include "utefile-private.h"
#include "utefile.h"
#include "scommon.h"
#include "sl1t.h"
#include "scdl.h"
#include "ssnp.h"
#include "mem.h"
#include "boobs.h"
/* for pr_tsmstz() */
#include "date.h"
#include "ute-print.h"
/* for bitsets */
#undef DECLF
#define DECLF		static
#include "gbs.h"

#include "cmd-aux.c"

#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif	/* !UNLIKELY */
#if !defined UNUSED
# define UNUSED(_x)	__attribute__((unused)) _x
#endif	/* !UNUSED */
#if !defined countof
# define countof(x)	(sizeof(x) / sizeof(*x))
#endif	/* !countof */

/* one day verbpr() might become --verbose */
#if defined DEBUG_FLAG
# include <assert.h>
# define UDEBUG(args...)	fprintf(stderr, args)
# define UNUSED_dbg(x)		UNUSED(x)
# define UNUSED_nodbg(x)	x
#else
# define UDEBUG(args...)
# define UNUSED_dbg(x)		x
# define UNUSED_nodbg(x)	UNUSED(x)
#endif	/* DEBUG_FLAG */

typedef const struct info_ctx_s *info_ctx_t;

struct info_ctx_s {
	bool verbp:1;
	bool guessp:1;
	bool filesp:1;

	int intv;
	int modu;

	utectx_t u;
	const char *fn;
};

/* holds the last time stamp */
static time_t stmp;


static void
pr_intv(int i)
{
	if (i <= 0 || i > 86400) {
		fputs("???", stdout);
	} else if (i == 86400) {
		fputs("eod", stdout);
	} else if (i >= 3600 && !(i % 3600)) {
		printf("%02dh", i / 3600);
	} else if (i >= 60 && !(i % 60)) {
		printf("%02dm", i / 60);
	} else {
		printf("%02ds", i);
	}
}

static void
pr_ttfs(int intv[static 16], uint32_t ttfs)
{
	if (ttfs & (1U << SCOM_TTF_UNK)) {
		fputs("\tunk", stdout);
	}
	if (ttfs & (1U << SL1T_TTF_BID)) {
		fputs("\ttick_b", stdout);
	}
	if (ttfs & (1U << SL1T_TTF_ASK)) {
		fputs("\ttick_a", stdout);
	}
	if (ttfs & (1U << SL1T_TTF_TRA)) {
		fputs("\ttick_t", stdout);
	}
	if (ttfs & (1U << SL1T_TTF_BIDASK)) {
		/* quote stands for full b/a quote */
		fputs("\ttick_q", stdout);
	}
	if (ttfs & (1U << SSNP_FLAVOUR)) {
		fputs("\ts", stdout);
		pr_intv(intv[5U]);
	}
	if (ttfs & (1U << (SCDL_FLAVOUR | SL1T_TTF_BID))) {
		fputs("\tc", stdout);
		pr_intv(intv[SL1T_TTF_BID]);
		fputs("_b", stdout);
	}
	if (ttfs & (1U << (SCDL_FLAVOUR | SL1T_TTF_ASK))) {
		fputs("\tc", stdout);
		pr_intv(intv[SL1T_TTF_ASK]);
		fputs("_a", stdout);
	}
	if (ttfs & (1U << (SCDL_FLAVOUR | SL1T_TTF_TRA))) {
		fputs("\tc", stdout);
		pr_intv(intv[SL1T_TTF_TRA]);
		fputs("_t", stdout);
	}
	if (ttfs & (1U << (SCDL_FLAVOUR | SL1T_TTF_VOL))) {
		fputs("\tc", stdout);
		pr_intv(intv[SL1T_TTF_VOL]);
		fputs("_v", stdout);
	}
	return;
}


/* our intervals tracker */
#define INTV_BLOB	(16U)

struct __iv_s {
	size_t nsyms;
	size_t nintv;
	int *intv;
};

static struct __iv_s iv[1] = {{0U}};

static int init_intv(size_t nsyms);
static void fini_intv(void);

static int
init_intv(size_t nsyms)
{
	if (UNLIKELY(iv->intv != NULL)) {
		fini_intv();
	}

	iv->intv = mmap(NULL, 4096U, PROT_MEM, MAP_MEM, -1, 0);
	if (UNLIKELY(iv->intv == MAP_FAILED)) {
		iv->intv = NULL;
		return -1;
	}
	iv->nintv = 4096U / (INTV_BLOB * sizeof(*iv->intv));
	iv->nsyms = nsyms;
	return 0;
}

static void
fini_intv(void)
{
	if (LIKELY(iv->intv != NULL)) {
		size_t mpsz = iv->nintv * INTV_BLOB * sizeof(*iv->intv);

		munmap(iv->intv, mpsz);
		iv->nintv = 0UL;
		iv->intv = NULL;
	}
	return;
}

static void
intv_stretch(void)
{
	size_t olsz = iv->nintv * INTV_BLOB * sizeof(*iv->intv);
	size_t nusz = 2 * olsz;

#if defined MREMAP_MAYMOVE
	iv->intv = mremap(iv->intv, olsz, nusz, MREMAP_MAYMOVE);
#else  /* !MREMAP_MAYMOVE */
	{
		void *nu = mmap(NULL, nusz, PROT_MEM, MAP_MEM, -1, 0);
		mempcy(nu, iv->intv, olsz);
		munmap(iv->intv, olsz);
		iv->intv = nu;
	}
#endif	/* MREMAP_MAYMOVE */

	/* stretch the whole thing,
	 * double the space means holes after each element */
	for (size_t i = iv->nintv - 1; i > 0; i--) {
		int *restrict tgt = iv->intv + INTV_BLOB * 2 * i;
		const int *src = iv->intv + INTV_BLOB * i;

		memcpy(tgt, src, INTV_BLOB * sizeof(*src));
		memset(tgt + INTV_BLOB, 0, INTV_BLOB * sizeof(*src));
	}
	/* naught-th element won't be moved but we need to zero out
	 * the slot after it */
	memset(iv->intv + INTV_BLOB, 0, INTV_BLOB * sizeof(*iv->intv));

	/* and last, keep track of the new number of intv blocks */
	iv->nintv *= 2;
	return;
}

static int*
__intv_get(unsigned int symi)
{
/* retrieve a block of 15 ints matching symbol index SYMI */
	size_t cand = (iv->nintv * symi) / (iv->nintv + iv->nsyms);

	for (size_t i = cand; i < iv->nintv; i++) {
		int *res = iv->intv + INTV_BLOB * i;

		if (res[0] == (int)symi) {
			return res;
		} else if (!res[0]) {
			/* empty slot */
			return res;
		}
	}
	return NULL;
}

static int*
intv_get(unsigned int symi)
{
	int *res;

	while ((res = __intv_get(symi)) == NULL) {
		intv_stretch();
	}
	return res;
}

static int*
intv_ref(unsigned int symi)
{
/* retrieve a block of 15 ints matching symbol index SYMI */
	size_t cand = (iv->nintv * symi) / (iv->nintv + iv->nsyms);

	for (size_t i = cand; i < iv->nintv; i++) {
		int *res = iv->intv + INTV_BLOB * i;

		if (res[0] == (int)symi) {
			return res;
		} else if (!res[0]) {
			/* empty slot */
			break;
		}
	}
	return NULL;
}


/* our bitset impl */
#undef DEFUN
#define DEFUN		static __attribute__((unused))
#include "gbs.c"

static struct gbs_s bs[1] = {{0U}};

static int
init_bset(size_t nsyms)
{
	init_gbs(bs, (nsyms + 1U) * 2U * 16U);
	return bs->nbits > 0 ? 0 : -1;
}

static void
free_bset(void)
{
	fini_gbs(bs);
	return;
}

static void
bset_clear(void)
{
	gbs_shift_lsb(bs, bs->nbits);
	return;
}

static void
bset_set(unsigned int symi, unsigned int ttf)
{
	gbs_set(bs, symi * 2U * 16U + (ttf % 32U));
	return;
}

static void
bset_pr(info_ctx_t ctx)
{
	unsigned int i = 0;

	for (const uint32_t *p = bs->bits,
		     *ep = p + (bs->nbits / 8U) / sizeof(*ep);
	     p < ep; p++, i++) {
		/* we're using a feature of little endians, namely that
		 * bitsets can be accessed through growing pointers without
		 * changing their meaning */
		uint32_t bits = htole32(*p);

		if (bits) {
			int *intv = NULL;

			if (ctx->filesp) {
				fputs(ctx->fn, stdout);
				putchar('\t');
			}
			with (const char *sym = ute_idx2sym(ctx->u, i)) {
				if (LIKELY(sym != NULL)) {
					fputs(sym, stdout);
				} else {
					fputs("0", stdout);
				}
			}

			if (ctx->intv) {
				/* print interval too */
				static char buf[64];
				char *q = buf;

				*q++ = '\t';
				q += pr_tsmstz(q, stmp - ctx->intv, 0, NULL, 'T');
				*q++ = '\t';
				q += pr_tsmstz(q, stmp, 0, NULL, 'T');
				*q = '\0';
				fputs(buf, stdout);
			}
			if ((bits >> 16U) &&
			    UNLIKELY((intv = intv_ref(i)) == NULL)) {
				/* we SHOULD have had an interval */
				static int fallback[16] = {0};
				intv = fallback;
			}
			pr_ttfs(intv, bits);
			putchar('\n');
		}
	}
	return;
}

static int
check_stmp(info_ctx_t ctx, scom_t ti)
{
	time_t ts = scom_thdr_sec(ti);
	time_t last = (ts - ctx->modu) % ctx->intv;

	if (ts - last >= stmp + ctx->intv) {
		stmp = ts - last;
		return 1;
	}
	return 0;
}

static int
round_to_seq(int x)
{
/* round x up onto our sequence 1,5,10,30,60,... */
	if (UNLIKELY(x <= 0)) {
		return x;
	}

	switch ((unsigned int)x) {
	case 1:
		return 1;
	case 2 ... 5:
		return 5;
	case 6 ... 10:
		return 10;
	case 11 ... 15:
		return 15;
	case 16 ... 30:
		return 30;
	case 31 ... 60:
		return 60;
	case 61 ... 300:
		return 300;
	case 301 ... 600:
		return 600;
	case 601 ... 900:
		return 900;
	case 901 ... 1800:
		return 1800;
	default:
		return 3600;
	}
}

/* marking corus, this is mostly interval guessing or recording */
static void
mark_ssnp(info_ctx_t UNUSED(ctx), int intv[static restrict 1], scom_t ti)
{
/* snapshots have got no meaningful `interval length'
 * but, conversely, you can snapshoot a timeseries at a fixed frequencyh
 * here's how we guess this frequency (and we call it intervals later on) */
	time_t ts = scom_thdr_sec(ti);

	if (!intv[4]) {
		;
	} else if (!intv[5] || intv[5] > ts - intv[4]) {
		intv[5] = ts - intv[4];
	}
	/* always keep track of current time */
	intv[4] = ts;
	return;
}

static void
mark_scdl(info_ctx_t ctx, int intv[static restrict 1], scom_t ti)
{
/* candles have a natural width, and if all candles are connected (as
 * opposed to overlapped or gapped) they give a complete picture of the
 * market, we assume the width of the candle coincides with the frequency
 * of candles snaps, if guessing is required we can also have a rough
 * guess, but we consider the interval sequence 1,5,10,30,... only */
	time_t ts = scom_thdr_sec(ti);
	const_scdl_t x = AS_CONST_SCDL(ti);
	unsigned int ttf = scom_thdr_ttf(ti) & 0x0fU;
	int this;

	if (!x->sta_ts && !ctx->guessp) {
		/* do nothing really */
		goto out;

	} else if (!x->sta_ts) {
		/* try and guess
		 * we can use tick types >= 12 (SL1T_TTF_BIDASK, ...) for now
		 * because no meaningful candle is defined over these */
		unsigned int tt2;

		switch (ttf) {
		case SL1T_TTF_BID:
			tt2 = 12U;
			break;
		case SL1T_TTF_ASK:
			tt2 = 13U;
			break;
		case SL1T_TTF_TRA:
			tt2 = 14U;
			break;
		case SL1T_TTF_VOL:
			tt2 = 15U;
			break;
		default:
			goto out;
		}

		/* do the guessing, only if we know of prior timestamps */
		if (intv[tt2] > 0) {
			if (!(this = ts - intv[tt2])) {
				/* dont bother */
				;
			} else if (!intv[ttf] || this < intv[ttf]) {
				intv[ttf] = round_to_seq(this);
			}
		}
		/* keep track of current time */
		intv[tt2] = ts;
		goto out;

	} else if (ts >= 100000 && x->sta_ts < 100000) {
		/* must be a candle spec */
		this = x->sta_ts;
	} else {
		this = ts - x->sta_ts;
	}

	if (!intv[ttf]) {
		/* store */
		intv[ttf] = this;
	} else if (intv[ttf] != this) {
		/* sort of reset */
		intv[ttf] = -1;
	}
out:
	return;
}


/* the actual infoing */
/* we're info'ing single scom's, much like a prf() in ute-print */
static int
mark(info_ctx_t ctx, scom_t ti)
{
	unsigned int tidx = scom_thdr_tblidx(ti);
	unsigned int ttf = scom_thdr_ttf(ti);

	if (ctx->intv && UNLIKELY(check_stmp(ctx, ti))) {
		/* new candle it is */
		bset_pr(ctx);
		bset_clear();
	}

	bset_set(tidx, ttf);

	/* do some interval tracking */
	if (ttf == SSNP_FLAVOUR || ttf > SCDL_FLAVOUR) {
		int *intv = intv_get(tidx);

		/* always bang the index */
		intv[0] = tidx;

		if (ttf == SSNP_FLAVOUR) {
			mark_ssnp(ctx, intv, ti);
		} else if (ttf > SCDL_FLAVOUR) {
			mark_scdl(ctx, intv, ti);
		}
	}
	return 0;
}

/* file wide operations */
static int
info1(info_ctx_t ctx, const char *UNUSED(fn))
{
	utectx_t hdl = ctx->u;
	size_t nsyms = ute_nsyms(hdl);

	if (UNLIKELY(init_bset(nsyms) < 0)) {
		return -1;
	} else if (UNLIKELY(init_intv(nsyms) < 0)) {
		free_bset();
		return -1;
	}

	/* reset the global stamp */
	stmp = 0U;

	/* go through the pages manually */
	if (ctx->verbp) {
		if (ctx->filesp) {
			fputs(ctx->fn, stdout);
			putchar('\t');
		}
		printf("pages\t%zu\n", ute_npages(hdl));
	}

	for (scom_t ti; (ti = ute_iter(ctx->u)) != NULL;) {
		/* now to what we always do */
		mark(ctx, ti);
	}
	/* last candle (or the first ever if no intv is set) */
	stmp += ctx->intv;
	bset_pr(ctx);

	/* and finalise */
	free_bset();
	fini_intv();
	return 0;
}


#if defined STANDALONE
#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#endif	/* __INTEL_COMPILER */
#include "ute-info.xh"
#include "ute-info.x"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
# pragma warning (default:181)
#endif	/* __INTEL_COMPILER */

int
main(int argc, char *argv[])
{
	struct info_args_info argi[1];
	struct info_ctx_s ctx[1] = {0};
	int res = 0;

	if (info_parser(argc, argv, argi)) {
		res = 1;
		goto out;
	} else if (argi->help_given) {
		info_parser_print_help();
		res = 0;
		goto out;
	}

	if (argi->interval_given) {
		ctx->intv = argi->interval_arg;
		if (argi->modulus_given) {
			ctx->modu = argi->modulus_arg;
		}
	} else if (argi->modulus_given) {
		fputs("\
warning: --modulus without --interval is not meaningful, ignored\n", stderr);
	}

	/* copy interesting stuff into our own context */
	if (argi->verbose_given) {
		ctx->verbp = true;
	}
	if (argi->guess_given) {
		ctx->guessp = true;
	}
	if (argi->files_given) {
		ctx->filesp = true;
	}

	for (unsigned int j = 0; j < argi->inputs_num; j++) {
		const char *fn = argi->inputs[j];
		const int fl = UO_RDONLY | UO_NO_LOAD_TPC;
		utectx_t hdl;

		if ((hdl = ute_open(fn, fl)) == NULL) {
			error("cannot open file `%s'", fn);
			res = 1;
			continue;
		}

		/* the actual checking */
		ctx->u = hdl;
		ctx->fn = fn;
		if (info1(ctx, fn)) {
			res = 1;
		}

		/* and that's us */
		ute_close(hdl);
	}
out:
	info_parser_free(argi);
	return res;
}
#endif	/* STANDALONE */

/* ute-info.c ends here */
