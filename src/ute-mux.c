/*** ute-mux.c -- muxing external sources
 *
 * Copyright (C) 2009, 2010 Sebastian Freundt
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
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <math.h>
#include <string.h>
#include "sumux.h"

#if !defined UNUSED
# define UNUSED(x)	x __attribute__((unused))
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


static char fmt_hlp[256];

static struct muxer_s supported_muxers[] = {
#if 0
	{.opt = "sl1t", .muxf = sl1t_mux},
#endif
	{.opt = "ariva", .muxf = ariva_slab},
	{.opt = "ibrti", .muxf = ibrti_slab},
	{.opt = "dukasq", .muxf = dukasq_slab},
	{.opt = "dukasa", .muxf = dukasa_slab},
	{.opt = "dukasb", .muxf = dukasb_slab},
#if 0
	{.opt = "gesmes", .muxf = gesmes_slab},
#endif
	{.opt = "tenfore", .muxf = tfraw_slab},
};

/* ute mux FILE [...] */
static const char ute_cmd_mux_help[] =
	"usage: ute mux [-o OUTFILE] [-f FORMAT] FILE ...\n"
	"\n\
Convert FILE to a ute file, write to OUTFILE (mux.ute by default).\n\
\n\
Further options:\n\
-f, --format FORMAT  Use the specified parser, see below for a list.\n\
-n, --name NAME      For single-security files use NAME as security symbol.\n\
-o, --output FILE    Write result to specified output file.\n\
-z, --zone NAME      Treat date/time strings as in time zone NAME.\n\
\n\
";

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

static void __attribute__((noreturn))
fuck_off_and_die(void)
{
	fputs(ute_cmd_mux_help, stdout);
	build_fmt_hlp();
	fputs(fmt_hlp, stdout);
	fputc('\n', stdout);
	exit(1);
}

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
		ctx->wrr = ute_open("workload.ute", UO_CREAT | UO_TRUNC);

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
	ute_close(ctx->wrr);
	return;
}

static void
ute_cmd_mux_popt(sumux_opt_t opts, int argc, const char *argv[])
{
	/* input file index */
	int ifi = 0;

	/* first of all make a infiles array as large as argv */
	opts->infiles = malloc(argc * sizeof(char*));
	/* parse options */
	for (int i = 1; i < argc; i++) {
		if (argv[i] == NULL) {
			/* global options are set to NULL */
			continue;

		} else if (!strcmp(argv[i], "--format") ||
		    !strcmp(argv[i], "-f")) {
			/* --format FMT */
			opts->muxf = find_muxer(argv[++i]);

		} else if (!strcmp(argv[i], "--name") ||
			   !strcmp(argv[i], "-n")) {
			/* --name SYM */
			opts->sname = argv[++i];

		} else if (!strcmp(argv[i], "--output") ||
			   !strcmp(argv[i], "-o")) {
			/* --output FILE */
			opts->outfile = argv[++i];

		} else if (!strcmp(argv[i], "--badfile") ||
			   !strcmp(argv[i], "-b")) {
			/* --badfile FILE */
			opts->badfile = argv[++i];

		} else if (!strcmp(argv[i], "-")) {
			/* --help */
			opts->infiles[ifi++] = "/dev/stdin";

		} else if (!strcmp(argv[i], "--help") ||
			   !strcmp(argv[i], "-h")) {
			/* --help */
			fuck_off_and_die();

		} else {
			/* must be a file to process */
			opts->infiles[ifi++] = argv[i];
		}
	}
	if (ifi == 0) {
		/* weird, should we assume stdin? */
		opts->infiles[ifi++] = "/dev/stdin";
	}
	/* finalise the infile list */
	opts->infiles[ifi] = NULL;
	return;
}

static void
ute_cmd_mux_unpopt(sumux_opt_t opts)
{
	/* just that inline thing */
	free(opts->infiles);
	return;
}

static void
ute_cmd_mux(sumux_opt_t opts)
{
	struct mux_ctx_s ctx[1] = {{0}};

	if (UNLIKELY(opts->muxf == NULL)) {
		/* piss off, we need a mux function */
		fputs("format not specified\n", stderr);
		return;
	}

	init_ticks(ctx, opts);
	for (const char **p = opts->infiles; *p; p++) {
		/* open the infile ... */
		int fd = open(*p, 0);
		if (fd >= 0) {
			ctx->infd = fd;
		} else {
			fprintf(stderr, "couldn't open file '%s'\n", *p);
			/* just try the next bloke */
			continue;
		}
		/* ... and now mux it */
		opts->muxf(ctx);
		/* close the infile */
		close(fd);
	}
	deinit_ticks(ctx);
	return;
}

static int
ute_cmd_mux_args(ute_opt_t octx, int argc, const char *argv[])
{
	struct sumux_opt_s opts[1] = {{0}};

	/* get globally specified options */
	opts->octx = octx;
	/* parse options */
	ute_cmd_mux_popt(opts, argc, argv);
	/* now call the actual mux command */
	ute_cmd_mux(opts);
	/* clear our resources */
	ute_cmd_mux_unpopt(opts);
	return 0;
}

/* ute-mux.c ends here */
