/*** ute-mux.c -- muxing external sources
 *
 * Copyright (C) 2009-2013 Sebastian Freundt
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
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <math.h>
#include <limits.h>
#include <string.h>
#include "module.h"

#include "utefile-private.h"
#include "ute-mux.h"
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

#if !defined _INDEXT
# define _INDEXT
typedef size_t index_t;
#endif	/* !_INDEXT */


/* standard mux function for ute files */
static void
ute_mux(mux_ctx_t ctx)
{
	const int fl = UO_RDONLY;
	const char *fn = ctx->infn;
	size_t wrr_npg = ute_npages(ctx->wrr);
	utectx_t hdl;

	if ((hdl = ute_open(fn, fl)) == NULL) {
		error("cannot open file '%s'", fn);
		return;
	}
	/* churn churn churn, steps here are
	 * 1. check if the sluts coincide
	 * 2.1. copy all tick pages
	 * 2.2. go through the ticks and adapt tbl idxs if need be  */
	if (!1/*sluts_compat_p(ctx->wrr, hdl)*/) {
		error("sluts don't coincide '%s'", fn);
		goto out;
	}

	for (size_t i = 0; i < ute_npages(hdl); i++) {
		struct uteseek_s sk[2];
		const size_t pgsz = UTE_BLKSZ * sizeof(*sk->sp);
		size_t sk_sz;

		seek_page(sk, hdl, i);
		if ((sk_sz = seek_byte_size(sk)) < pgsz) {
			/* half page, just add it to the tpc */
			const size_t nticks = sk_sz / sizeof(*sk->sp);
			ute_add_ticks(ctx->wrr, sk->sp, nticks);
		} else if (ute_extend(ctx->wrr, sk_sz) < 0) {
			/* file extending fucked */
			error("cannot extend file");
		} else {
			/* just mmap into target space */
			seek_page(sk + 1, ctx->wrr, wrr_npg++);
			memcpy(sk[1].sp, sk[0].sp, sk_sz);
			flush_seek(sk + 1);
		}
		flush_seek(sk);
	}
out:
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
		   (ctx->wrr = ute_open(outf, UO_CREAT | UO_RDWR))) {
		;
	} else if ((ctx->wrr = ute_open(outf, UO_CREAT | UO_TRUNC))) {
		/* plain old mux */
		;
	} else {
		error("cannot open output file `%s'", outf);
		res = -1;
	}
	/* just make sure we dont accidentally use infd 0 (STDIN) */
	ctx->infd = -1;
	ctx->badfd = -1;
	return res;
}

static void
deinit_ticks(mux_ctx_t ctx)
{
	if (ctx->wrr) {
		/* check if we wrote to a tmp file */
		if (ctx->opts->outfile == NULL) {
			const char *fn;
			if ((fn = ute_fn(ctx->wrr))) {
				puts(fn);
			}
		}
		ute_close(ctx->wrr);
	}
	return;
}


#if defined STANDALONE
#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#endif	/* __INTEL_COMPILER */
#include "ute-mux.xh"
#include "ute-mux.x"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
# pragma warning (default:181)
#endif	/* __INTEL_COMPILER */

int
main(int argc, char *argv[])
{
	struct mux_args_info argi[1];
	struct mux_parser_params parm = {
		.override = 1,
		.initialize = 1,
		.check_required = 1,
		.check_ambiguity = 0,
		.print_errors = 0,
	};
	struct sumux_opt_s opts[1] = {{0}};
	struct mux_ctx_s ctx[1] = {{0}};
	struct muxer_s mxer;
	int muxer_specific_options_p = 0;
	int res = 0;

	if (mux_parser_ext(argc, argv, argi, &parm)) {
		/* maybe we've got as far as to parse --format|-f already */
		if (argi->format_arg == NULL) {
			res = 1;
			goto out;
		}
		/* otherwise we could try forming ute-mux-<MUXER> as cmd */
		muxer_specific_options_p = 1;
	} else if (argi->help_given && argi->format_arg == NULL) {
		mux_parser_print_help();
		fputs("\n", stdout);
		print_muxers();
		res = 0;
		goto out;
	}

	if (argi->output_given && argi->into_given) {
		fputs("only one of --output and --into can be given\n", stderr);
		res = 1;
		goto out;
	}

	/* initialise the module system */
	ute_module_init();

	if (UNLIKELY((mxer = find_muxer(argi->format_arg),
		      mxer.muxf == NULL && mxer.mux_main_f == NULL))) {
		/* piss off, we need a mux function */
		fputs("format unknown\n", stderr);
		res = 1;
		goto out;
	} else if (muxer_specific_options_p && mxer.mux_main_f == NULL) {
		fputs("\
muxer specific options given but cannot find muxer\n", stderr);
		res = 1;
		goto out;
	}

	if (argi->name_given) {
		opts->sname = argi->name_arg;
	}

	if (argi->output_given) {
		opts->outfile = argi->output_arg;
	} else if (argi->into_given) {
		opts->outfile = argi->into_arg;
		opts->flags |= OUTFILE_IS_INTO;
	}

	if (argi->refdate_given) {
		/* for simplicity expect unix epoch stamp for now */
		opts->tsoff = argi->refdate_arg;
	}

	switch (argi->flavour_arg) {
	default:
	case flavour__NULL:
		opts->tt = SCOM_TTF_UNK;
		break;
	case flavour_arg_b:
		opts->tt = SL1T_TTF_BID;
		break;
	case flavour_arg_a:
		opts->tt = SL1T_TTF_ASK;
		break;
	case flavour_arg_t:
		opts->tt = SL1T_TTF_TRA;
		break;
	}

	if (!argi->multiplier_given) {
		opts->mul = 1;
	} else {
		opts->mul = argi->multiplier_arg;
	}
	if (!argi->magnifier_given) {
		opts->mag = 1;
	} else {
		opts->mag = argi->magnifier_arg;
	}

	/* the actual muxing step */
	if (init_ticks(ctx, opts) < 0) {
		res = 1;
		goto out;
	}
	/* prefer the fully fledged version */
	if (mxer.mux_main_f != NULL) {
		res = mxer.mux_main_f(ctx, argc, argv);
	} else {
		for (unsigned int j = 0; j < argi->inputs_num; j++) {
			const char *f = argi->inputs[j];
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
				error("cannot open file '%s'", f);
				/* just try the next bloke */
				continue;
			}
			/* ... and now mux it */
			mxer.muxf(ctx);
			/* close the infile */
			close(fd);
		}
	}
	deinit_ticks(ctx);

out:
	unfind_muxer(mxer);
	ute_module_fini();
	mux_parser_free(argi);
	return res;
}
#endif	/* STANDALONE */

/* ute-mux.c ends here */
