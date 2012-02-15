/*** ute-mux.c -- muxing external sources
 *
 * Copyright (C) 2009 - 2012 Sebastian Freundt
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
#include <string.h>
#include "sl1t.h"
#include "utefile.h"
#include "ute-mux.h"

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

struct muxer_s {
	const char *opt;
	void(*muxf)(mux_ctx_t);
};


static struct muxer_s supported_muxers[] = {
#if 0
	{.opt = "sl1t", .muxf = sl1t_mux},
#endif
	{.opt = "ariva", .muxf = ariva_slab},
	{.opt = "ibrti", .muxf = ibrti_slab},
	{.opt = "dukas", .muxf = dukas_slab},
#if 0
	{.opt = "gesmes", .muxf = gesmes_slab},
#endif
	{.opt = "tenfore", .muxf = tfraw_slab},
};

#if 0
static void
build_fmt_hlp(void)
{
	char *p;
	p = stpcpy(fmt_hlp,
		   "Input file format, defaults to sl1t if omitted. "
		   "Supported formats:");
	for (index_t i = 0; i < countof(supported_muxers); i++) {
		*p++ = ' ';
		p = stpcpy(p, supported_muxers[i].opt);
	}
	return;
}
#endif

static void
(*find_muxer(const char *opt))(mux_ctx_t)
{
	for (index_t i = 0; i < countof(supported_muxers); i++) {
		/* at the moment we make use of the fact that
		 * all muxers begin with different letters. */
		if (opt[0] == supported_muxers[i].opt[0]) {
			return supported_muxers[i].muxf;
		}
	}
	return NULL;
}


static void
init_ticks(mux_ctx_t ctx, sumux_opt_t opts)
{
	const char *outf = opts->outfile;

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
		exit(1);

	} else {
		ctx->wrr = ute_open(outf, UO_CREAT | UO_TRUNC);
	}
	/* just make sure we dont accidentally use infd 0 (STDIN) */
	ctx->infd = -1;
	ctx->badfd = -1;
	return;
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
#include "ute-mux-clo.h"
#include "ute-mux-clo.c"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
# pragma warning (default:181)
#endif	/* __INTEL_COMPILER */

int
main(int argc, char *argv[])
{
	struct mux_args_info argi[1];
	struct sumux_opt_s opts[1] = {{0}};
	struct mux_ctx_s ctx[1] = {{0}};
	int res = 0;

	if (mux_parser(argc, argv, argi)) {
		res = 1;
		goto out;
	}

	if (argi->format_given &&
	    UNLIKELY((opts->muxf = find_muxer(argi->format_arg)) == NULL)) {
		/* piss off, we need a mux function */
		fputs("format unknown\n", stderr);
		res = 1;
		goto out;
	}


	if (argi->name_given) {
		opts->sname = argi->name_arg;
	}

	if (argi->output_given) {
		opts->outfile = argi->output_arg;
	}

	if (argi->badfile_given) {
		opts->badfile = argi->badfile_arg;
	}

	if (argi->refdate_given) {
		/* for simplicity expect unix epoch stamp for now */
		opts->tsoff = argi->refdate_arg;
	}

	switch (argi->flavour_arg) {
	default:
	case flavour__NULL:
		opts->tt = SL1T_TTF_UNK;
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
	init_ticks(ctx, opts);
	for (unsigned int j = 0; j < argi->inputs_num; j++) {
		const char *f = argi->inputs[j];
		int fd;

		/* open the infile ... */
		if ((fd = open(f, 0)) >= 0) {
			ctx->infd = fd;
		} else {
			fprintf(stderr, "couldn't open file '%s'\n", f);
			/* just try the next bloke */
			continue;
		}
		/* ... and now mux it */
		opts->muxf(ctx);
		/* close the infile */
		close(fd);
	}
	deinit_ticks(ctx);

out:
	mux_parser_free(argi);
	return res;
}
#endif	/* STANDALONE */

/* ute-mux.c ends here */
