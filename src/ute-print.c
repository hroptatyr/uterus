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
#include <assert.h>
#include <fcntl.h>
#include "utefile.h"
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
	ute_dso_sym_t pr_sym;
	if ((pr_dso = open_aux(opt)) == NULL) {
		return NULL;
	} else if ((pr_sym = find_sym(pr_dso, "pr")) == NULL) {
		return NULL;
	}
	return (ssize_t(*)(pr_ctx_t, scom_t))pr_sym;
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
	ssize_t(*pr_sym)(pr_ctx_t, scom_t);

	/* basename-ify */
	if ((fname = strrchr(fname, '/'))) {
		fname++;
	}
	/* check for the forbidden words */
	if (!strstr(fname, nono) && (pr_sym = find_printer(fname))) {
		putchar('*');
		putchar(' ');
		puts(fname);
		unfind_printer(pr_sym);
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

	for (unsigned int j = 0; j < argi->inputs_num; j++) {
		const char *f = argi->inputs[j];
		void *hdl;

		if ((hdl = ute_open(f, UO_RDONLY)) == NULL) {
			continue;
		}
		/* otherwise print all them ticks */
		ctx->uctx = hdl;
		for (size_t i = 0; i < ute_nticks(hdl);) {
			scom_t ti = ute_seek(hdl, i);
			if (ti) {
				prf(ctx, ti);
			}
			i += scom_thdr_size(ti) / sizeof(struct sl1t_s);
		}
		/* oh right, close the handle */
		ute_close(hdl);
	}

	/* check and call finaliser if any */
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
