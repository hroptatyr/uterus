/*** ute-print.c -- ute tick printer
 *
 * Copyright (C) 2010 - 2012 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@ga-group.nl>
 *
 * This file is part of sushi/uterus.
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
#include <stdio.h>
#include <fcntl.h>
#include "utefile.h"

/* we're just as good as rudi, aren't we? */
#if defined DEBUG_FLAG
# include <assert.h>
#else  /* !DEBUG_FLAG */
# define assert(args...)
#endif	/* DEBUG_FLAG */

#define DEFINE_GORY_STUFF
#include "m30.h"
#include "m62.h"

#include "timegm.c"
#include "strftime.c"
#include "module.h"

#include "ute-print.h"

#include "scommon.h"
#include "sl1t.h"
#include "scdl.h"

#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif	/* !UNLIKELY */
#if !defined UNUSED
# define UNUSED(_x)	__attribute__((unused)) _x
#endif	/* !UNUSED */
#if !defined countof
# define countof(x)	(sizeof(x) / sizeof(*x))
#endif	/* !countof */


static ute_dso_t pr_dso;

static ssize_t
(*find_printer(const char *opt))(pr_ctx_t, scom_t)
{
	ute_dso_sym_t res;
	if ((pr_dso = open_aux(opt)) == NULL) {
		return NULL;
	} else if ((res = find_sym(pr_dso, "pr")) == NULL) {
		return NULL;
	}
	return (ssize_t(*)(pr_ctx_t, scom_t))res;
}

static void
unfind_printer(UNUSED(ssize_t(*prf)(pr_ctx_t, scom_t)))
{
	if (pr_dso) {
		close_aux(pr_dso);
		pr_dso = NULL;
	}
	return;
}

static int
print_mudem(const char *fname, void *UNUSED(clo))
{
	static const char nono[] = "ute";
	ssize_t(*prf)(pr_ctx_t, scom_t);

	/* basename-ify */
	if ((fname = strrchr(fname, '/'))) {
		fname++;
	}
	/* check for the forbidden words */
	if (!strstr(fname, nono) && (prf = find_printer(fname))) {
		putchar('*');
		putchar(' ');
		puts(fname);
		unfind_printer(prf);
	}
	return 0;
}

static void
print_mudems(void)
{
/* bit of layer leak here */
	/* initialise the module system */
	ute_module_init();

	puts("Supported formats:");
	trav_dso(print_mudem, NULL);
	return;
}

static void
pr1(pr_ctx_t ctx, const char *f, ssize_t(*prf)(pr_ctx_t, scom_t))
{
	void *hdl;

	if ((hdl = ute_open(f, UO_RDONLY)) == NULL) {
		return;
	}
	/* otherwise print all them ticks */
	ctx->uctx = hdl;
	/* check for ute version */
	if (UNLIKELY(ute_version(hdl) == UTE_VERSION_01)) {
		/* we need to flip the ti */
		for (size_t i = 0; i < ute_nticks(hdl);) {
			char buf[64];
			scom_thdr_t nu_ti = AS_SCOM_THDR(buf);
			scom_t ti = ute_seek(hdl, i);
			size_t tsz;

			/* promote the old header, copy to tmp buffer BUF */
			scom_promote_v01(nu_ti, ti);
			tsz = scom_thdr_size(nu_ti);
			/* copy the rest of the tick into the buffer */
			memcpy(buf + sizeof(*nu_ti), ti + 1, tsz - sizeof(*ti));
			/* now to what we always do */
			prf(ctx, nu_ti);
			i += tsz / sizeof(struct sndwch_s);
		}
	} else {
		/* no flips in this one */
		for (size_t i = 0; i < ute_nticks(hdl);) {
			scom_t ti = ute_seek(hdl, i);
			prf(ctx, ti);
			i += scom_thdr_size(ti) / sizeof(struct sndwch_s);
		}
	}
	/* oh right, close the handle */
	ute_close(hdl);
	return;
}

static ssize_t
rdpg(char *restrict tgt, size_t tsz, int fd)
{
	ssize_t tot = 0;

	for (ssize_t n; (n = read(fd, tgt + tot, tsz - tot)) > 0; tot += n);
	return tot;
}

static ssize_t
prpg(pr_ctx_t ctx, char *tpg, size_t tsz, ssize_t(*prf)(pr_ctx_t, scom_t))
{
	char *eot = tpg + tsz;
	ssize_t res = 0;

	while (tpg < eot) {
		scom_t ti = (void*)tpg;
		prf(ctx, ti);
		tpg += scom_thdr_size(ti);
	}
	return res;
}


#if defined STANDALONE
#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#endif	/* __INTEL_COMPILER */
#include "ute-print-clo.h"
#include "ute-print-clo.c"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
# pragma warning (default:181)
#endif	/* __INTEL_COMPILER */

int
main(int argc, char *argv[])
{
	struct print_args_info argi[1];
	struct pr_ctx_s ctx[1] = {{0}};
	struct pr_opt_s opt[1] = {{0}};
	ssize_t(*prf)(pr_ctx_t, scom_t) = NULL;
	const char *fmt;
	int res = 0;

	if (print_parser(argc, argv, argi)) {
		res = 1;
		goto out;
	} else if (argi->help_given) {
		print_parser_print_help();
		fputs("\n", stdout);
		print_mudems();
		res = 0;
		goto out;
	}

	if (!argi->format_given) {
		/* superseding rudi's fave format */
		fmt = "uta";
	} else {
		fmt = argi->format_arg;
	}

	if (argi->output_given) {
		opt->outfile = argi->output_arg;
	}

	/* initialise the module system */
	ute_module_init();

	if (UNLIKELY((prf = find_printer(fmt)) == NULL)) {
		/* we need a printer, so piss off here */
		fputs("printer format unknown\n", stderr);
		res = 1;
		goto out;
	}
	if (argi->output_given) {
		const int oflags = O_CREAT | O_TRUNC | O_RDWR;

		/* store a copy of the file name */
		opt->outfile = argi->output_arg;
		if ((ctx->outfd = open(argi->output_arg, oflags, 0644)) < 0) {
			res = 1;
			fputs("cannot open output file\n", stderr);
			goto out;
		}
	} else {
		ctx->outfd = STDOUT_FILENO;
	}

	/* we pass on the option structure so modules can have a finer
	 * control over things, i.e. mmap the output file and whatnot */
	ctx->opts = opt;

	/* check and call initialiser if any */
	{
		void(*initf)(pr_ctx_t);
		if ((initf = (void(*)(pr_ctx_t))find_sym(pr_dso, "init"))) {
			initf(ctx);
		}
	}

	if (argi->inputs_num == 0 && !isatty(STDIN_FILENO)) {
		/* operate on pages */
		char pg[4096];
		/* slut size */
		size_t slsz;
		ssize_t nrd;

		/* first page is ute header, just read him off */
		if (rdpg(pg, sizeof(pg), STDIN_FILENO) < (ssize_t)sizeof(pg)) {
			/* oh oh oh, do nothing, aye? */
			goto fina;
		} else if (memcmp(pg, "UTE+", 4)) {
			/* not the ute header, so fuck of too? */
			goto fina;
		}
		/* otherwise try our harderst */
		slsz = ((utehdr2_t)pg)->slut_sz;
		while ((nrd = rdpg(pg, 4096, STDIN_FILENO)) > 0) {
			if (UNLIKELY((size_t)nrd < sizeof(pg))) {
				/* last page then? */
				nrd -= slsz;
			}
			prpg(ctx, pg, nrd, prf);
		}

	} else {
		for (unsigned int j = 0; j < argi->inputs_num; j++) {
			pr1(ctx, argi->inputs[j], prf);
		}
	}

	/* check and call finaliser if any */
fina:
	{
		void(*finif)(pr_ctx_t);
		if ((finif = (void(*)(pr_ctx_t))find_sym(pr_dso, "fini"))) {
			finif(ctx);
		}
	}
	/* close the output file */
	close(ctx->outfd);

out:
	unfind_printer(prf);
	ute_module_fini();
	print_parser_free(argi);
	return res;
}
#endif	/* STANDALONE */

/* ute-print.c ends here */
