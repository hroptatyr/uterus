/*** ute-mux.c -- muxing external sources
 *
 * Copyright (C) 2009-2015 Sebastian Freundt
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

#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <limits.h>
#include <string.h>
#include "module.h"
#include "nifty.h"

#include "utefile-private.h"
#include "ute-mux.h"
#include "cmd-aux.c"

#if !defined _INDEXT
# define _INDEXT
typedef size_t index_t;
#endif	/* !_INDEXT */

typedef uint_fast16_t slutlut_t[65536U];


#define scom_scom_size(t)	(scom_byte_size(t) / sizeof(*t))

static bool
build_slutlut(slutlut_t x, utectx_t tgt, utectx_t src)
{
/* build a slut look-up table, to transition tblidxs from SRC to TGT. */
	const size_t src_nsyms = ute_nsyms(src);
	bool compatp = true;

	for (size_t i = 1UL; i <= src_nsyms; i++) {
		const char *sym = ute_idx2sym(src, i);

		if (UNLIKELY(sym == NULL)) {
			/* huh? */
			x[i] = 0U;
			compatp = false;
		} else if ((x[i] = ute_sym2idx(tgt, sym)) != i) {
			/* not coinciding */
			compatp = false;
		}
	}
	/* wipe the rest? */
	memset(x + src_nsyms + 1U, 0, (65536U - (src_nsyms + 1U)) * sizeof(*x));
	return compatp;
}

static void
add_ticks(utectx_t tgt, const void *t, size_t z)
{
	for (scom_t tp = t, et = tp + z / sizeof(*tp);
	     tp < et; tp += scom_scom_size(tp)) {
		ute_add_tick(tgt, tp);
	}
	return;
}

static void
add_ticks_trnsl(utectx_t tgt, const void *t, size_t z, slutlut_t x)
{
	scidx_t cch[1];

	for (scom_t tp = t, et = tp + z / sizeof(*tp);
	     tp < et; tp += scom_scom_size(tp)) {
		*cch = *AS_SCOM(tp);
		scom_thdr_set_tblidx(cch, x[scom_thdr_tblidx(tp)]);
		ute_add_tick_as(tgt, tp, cch);
	}
	return;
}

#undef scom_scom_size


/* standard mux function for ute files */
static void
ute_mux(mux_ctx_t ctx)
{
	static uint_fast16_t stt[65536U];
	const int fl = UO_RDONLY;
	const char *fn = ctx->infn;
	bool compatp = true;
	utectx_t hdl;

	if ((hdl = ute_open(fn, fl)) == NULL) {
		error("cannot open file '%s'", fn);
		return;
	}
	/* churn churn churn, steps here are
	 * 1. check if the sluts coincide
	 * 2.1. copy all tick pages
	 * 2.2. go through the ticks and adapt tbl idxs if need be  */
	if (!(compatp = build_slutlut(stt, ctx->wrr, hdl))) {
		errno = 0;
		error("sluts don't coincide '%s'", fn);
	}

	for (size_t i = 0; i < ute_npages(hdl); i++) {
		struct uteseek_s sk[2];
		size_t sk_sz;

		seek_page(sk, hdl, i);
		sk_sz = seek_byte_size(sk);
		/* half page, just add it to the tpc */
		if (!compatp) {
			add_ticks_trnsl(ctx->wrr, sk->sp, sk_sz, stt);
		} else {
			add_ticks(ctx->wrr, sk->sp, sk_sz);
		}
		flush_seek(sk);
	}

	/* and off we are */
	ute_close(hdl);
	return;
}


static ute_dso_t mux_dso;

struct muxer_s {
	void(*muxf)(mux_ctx_t);
	int(*mux_main_f)(mux_ctx_t, int, char*[]);
};

static struct muxer_s
find_muxer(const char *opt)
{
	struct muxer_s res = {NULL, NULL};
	ute_dso_sym_t sym;

	if (opt == NULL) {
		/* ah, default muxer is ute himself */
		return (struct muxer_s){ute_mux, NULL};
	} else if ((mux_dso = open_aux(opt)) == NULL) {
		return (struct muxer_s){NULL, NULL};
	}
	/* try and resolve at least the mux symbol */
	if ((sym = find_sym(mux_dso, "mux")) != NULL) {
		res.muxf = (void(*)())sym;
	}
	if ((sym = find_sym(mux_dso, "mux_main")) != NULL) {
		res.mux_main_f = (int(*)())sym;
	}
	return res;
}

static void
unfind_muxer(UNUSED(struct muxer_s mxer))
{
	if (mux_dso) {
		close_aux(mux_dso);
		mux_dso = NULL;
	}
	return;
}

static int
print_muxer(const char *fname, void *UNUSED(clo))
{
	static const char nono[] = "ute";
	struct muxer_s tmp;

	/* basename-ify */
	if ((fname = strrchr(fname, '/'))) {
		fname++;
	}
	/* check for the forbidden words */
	if (!strstr(fname, nono) &&
	    (tmp = find_muxer(fname),
	     tmp.muxf != NULL || tmp.mux_main_f != NULL)) {
		putchar('*');
		putchar(' ');
		puts(fname);
		unfind_muxer(tmp);
	}
	return 0;
}

static void
print_muxers(void)
{
	/* initialise the module system */
	ute_module_init();

	puts("Supported formats:");
	trav_dso(print_muxer, NULL);
	return;
}


static int
init_ticks(mux_ctx_t ctx, sumux_opt_t opts)
{
	const char *outf = opts->outfile;
	int res = 0;

	/* start with a rinse, keep our opts though */
	memset(ctx, 0, sizeof(*ctx));
	/* just so we know where our options are */
	ctx->opts = opts;

	if (outf == NULL) {
		ctx->wrr = ute_mktemp(0);

	} else if (outf[0] == '-' && outf[1] == '\0') {
		/* bad idea */
		ctx->wrr = NULL;
		fputs("This is binary data, cannot dump to stdout\n", stderr);
		res = -1;

	} else if ((opts->flags & OUTFILE_IS_INTO) &&
		   (ctx->wrr = ute_open(outf, UO_CREAT | UO_RDWR)) == NULL) {
		error("cannot open output file `%s' for appending", outf);
		res = -1;
	} else if (!(opts->flags & OUTFILE_IS_INTO) &&
		   (ctx->wrr = ute_open(outf, UO_CREAT | UO_TRUNC)) == NULL) {
		error("cannot open output file `%s'", outf);
		res = -1;
	}
	/* just make sure we dont accidentally use infd 0 (STDIN) */
	ctx->infd = -1;
	ctx->badfd = -1;
	return res;
}

static void
deinit_ticks(mux_ctx_t ctx, size_t nsucc)
{
	if (ctx->wrr) {
		/* check if we wrote to a tmp file */
		if (ctx->opts->outfile == NULL) {
			const char *fn;
			if ((fn = ute_fn(ctx->wrr)) && nsucc) {
				puts(fn);
			} else if (fn &&
				   !(ctx->opts->flags & OUTFILE_IS_INTO)) {
				/* better unlink outfile */
				(void)unlink(fn);
			}
		} else if (!nsucc && !(ctx->opts->flags & OUTFILE_IS_INTO)) {
			(void)unlink(ctx->opts->outfile);
		}
		ute_close(ctx->wrr);
	}
	return;
}


#if defined STANDALONE
#define yuck_post_help	yuck_post_help
#include "ute-mux.yucc"

static void yuck_post_help(const yuck_t UNUSED(src[static 1U]))
{
	print_muxers();
	return;
}

int
main(int argc, char *argv[])
{
	yuck_t argi[1U];
	struct sumux_opt_s opts[1U] = {{0}};
	struct mux_ctx_s ctx[1U] = {{0}};
	struct muxer_s mxer;
	int muxer_specific_options_p = 0;
	size_t nsucc = 0U;
	int rc = 0;

	if (yuck_parse(argi, argc, argv)) {
		/* maybe we've got as far as to parse --format|-f already */
		rc = 1;
		goto out;
	}

	if (argi->output_arg && argi->into_arg) {
		fputs("only one of --output and --into can be given\n", stderr);
		rc = 1;
		goto out;
	}

	/* initialise the module system */
	ute_module_init();

	if (UNLIKELY((mxer = find_muxer(argi->format_arg),
		      mxer.muxf == NULL && mxer.mux_main_f == NULL))) {
		/* piss off, we need a mux function */
		fputs("format unknown\n", stderr);
		rc = 1;
		goto out;
	} else if (muxer_specific_options_p && mxer.mux_main_f == NULL) {
		fputs("\
muxer specific options given but cannot find muxer\n", stderr);
		rc = 1;
		goto out;
	}

	if (argi->name_arg) {
		opts->sname = argi->name_arg;
	}

	if (argi->output_arg) {
		opts->outfile = argi->output_arg;
	} else if (argi->into_arg) {
		opts->outfile = argi->into_arg;
		opts->flags |= OUTFILE_IS_INTO;
	}

	if (argi->refdate_arg) {
		/* for simplicity expect unix epoch stamp for now */
		opts->tsoff = strtol(argi->refdate_arg, NULL, 10);
	}

	if (!argi->flavour_arg) {
		goto dflt_tt;
	}
	switch (*argi->flavour_arg) {
	default:
	dflt_tt:
		opts->tt = SCOM_TTF_UNK;
		break;
	case 'b':
	case 'B':
		opts->tt = SL1T_TTF_BID;
		break;
	case 'a':
	case 'A':
		opts->tt = SL1T_TTF_ASK;
		break;
	case 't':
	case 'T':
		opts->tt = SL1T_TTF_TRA;
		break;
	}

	if (!argi->multiplier_arg) {
		/* default value */
		opts->mul = 1;
	} else {
		opts->mul = strtoul(argi->multiplier_arg, NULL, 10);
	}
	if (!argi->magnifier_arg) {
		/* default value */
		opts->mag = 1;
	} else {
		opts->mag = strtoul(argi->magnifier_arg, NULL, 10);
	}

	/* the actual muxing step */
	if (init_ticks(ctx, opts) < 0) {
		rc = 1;
		goto out;
	}
	/* prefer the fully fledged version */
	if (mxer.mux_main_f != NULL) {
		rc = mxer.mux_main_f(ctx, argi->nargs + 1U, argi->args - 1);
		nsucc++;
	} else {
		for (size_t j = 0; j < argi->nargs; j++) {
			const char *f = argi->args[j];
			int fd;

			/* open the infile ... */
			if (f[0] == '-' && f[1] == '\0') {
				ctx->infd = fd = STDIN_FILENO;
				ctx->infn = NULL;
				ctx->badfd = STDERR_FILENO;
			} else if ((fd = open(f, 0)) >= 0) {
				ctx->infd = fd;
				ctx->infn = f;
				ctx->badfd = STDERR_FILENO;
			} else {
				error("cannot open file `%s'", f);
				rc = 2;
				/* just try the next bloke */
				continue;
			}
			/* ... and now mux it */
			mxer.muxf(ctx);
			/* close the infile */
			close(fd);
			/* great success innit? */
			nsucc++;
		}
	}
	deinit_ticks(ctx, nsucc);

out:
	unfind_muxer(mxer);
	ute_module_fini();
	yuck_free(argi);
	return rc;
}
#endif	/* STANDALONE */

/* ute-mux.c ends here */
