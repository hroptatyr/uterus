/*** ute-print.c -- ute tick printer
 *
 * Copyright (C) 2010-2014 Sebastian Freundt
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
#if defined HAVE_SYS_TYPES_H
/* for ssize_t */
# include <sys/types.h>
#endif	/* HAVE_SYS_TYPES_H */
#include <fcntl.h>
#include "utefile.h"
#include "boobs.h"

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

/* we're just as good as rudi, aren't we? */
#if defined DEBUG_FLAG
# include <assert.h>
# define MAYBE_NOINLINE		__attribute__((noinline))
#else  /* !DEBUG_FLAG */
# define assert(args...)
# define MAYBE_NOINLINE
#endif	/* DEBUG_FLAG */

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

struct printer_s {
	int(*prf)(pr_ctx_t, scom_t);
	int(*init_main_f)(pr_ctx_t, int, char*[]);
	void(*initf)(pr_ctx_t);
	void(*finif)(pr_ctx_t);
};

static struct printer_s
find_printer(const char opt[static 1])
{
	struct printer_s res = {NULL, NULL, NULL, NULL};
	ute_dso_sym_t sym;

	if ((pr_dso = open_aux(opt)) == NULL) {
		return res;
	}
	if ((sym = find_sym(pr_dso, "pr")) != NULL) {
		res.prf = (int(*)())sym;
	}
	if ((sym = find_sym(pr_dso, "init")) != NULL) {
		res.initf = (void(*)())sym;
	}
	if ((sym = find_sym(pr_dso, "fini")) != NULL) {
		res.finif = (void(*)())sym;
	}
	if ((sym = find_sym(pr_dso, "init_main")) != NULL) {
		res.init_main_f = (int(*)())sym;
	}
	return res;
}

static void
unfind_printer(UNUSED(struct printer_s prer))
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
	struct printer_s tmp;

	/* basename-ify */
	if ((fname = strrchr(fname, '/'))) {
		fname++;
	}
	/* check for the forbidden words */
	if (!strstr(fname, nono) &&
	    (tmp = find_printer(fname), tmp.prf != NULL)) {
		putchar('*');
		putchar(' ');
		puts(fname);
		unfind_printer(tmp);
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

static void MAYBE_NOINLINE
pr1(pr_ctx_t ctx, const char *f, int(*prf)(pr_ctx_t, scom_t))
{
	utectx_t hdl;

	if ((hdl = ute_open(f, UO_RDONLY)) == NULL) {
		fprintf(stderr, "cannot open file '%s'\n", f);
		return;
	}
	/* otherwise print all them ticks */
	ctx->uctx = hdl;

	/* use the co-routine iterator */
	for (scom_t ti; (ti = ute_iter(hdl)) != NULL;) {
		/* now to what we always do */
		prf(ctx, ti);
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
prpg(pr_ctx_t ctx, char *tpg, size_t tsz, int(*prf)(pr_ctx_t, scom_t))
{
	char *eot = tpg + tsz;
	ssize_t res = 0;

	while (tpg < eot) {
		scom_t ti = (void*)tpg;
		prf(ctx, ti);
		tpg += scom_byte_size(ti);
	}
	return res;
}

static size_t
get_slut_sz(utehdr2_t p)
{
	return p->slut_sz;
}


#if defined STANDALONE
#define yuck_post_help	yuck_post_help
#include "ute-print.yucc"

static void yuck_post_help(const yuck_t UNUSED(src[static 1U]))
{
	print_mudems();
	return;
}

int
main(int argc, char *argv[])
{
	yuck_t argi[1U];
	struct pr_ctx_s ctx[1] = {{0}};
	struct pr_opt_s opt[1] = {{0}};
	struct printer_s prer;
	int printer_specific_options_p = 0;
	const char *fmt;
	int rc = 0;

	if (yuck_parse(argi, argc, argv)) {
		rc = 1;
		goto out;
	}

	if (!argi->format_arg) {
		/* superseding rudi's fave format */
		fmt = "uta";
	} else {
		fmt = argi->format_arg;
	}

	if (argi->output_arg) {
		opt->outfile = argi->output_arg;
	}

	/* initialise the module system */
	ute_module_init();

	if (UNLIKELY((prer = find_printer(fmt), prer.prf == NULL))) {
		/* we need a printer, so piss off here */
		fputs("printer format unknown\n", stderr);
		rc = 1;
		goto out;
	} else if (printer_specific_options_p && prer.init_main_f == NULL) {
		fputs("\
printer specific options given but cannot find printer\n", stderr);
		rc = 1;
		goto out;
	}
	if (argi->output_arg) {
		const int oflags = O_CREAT | O_TRUNC | O_RDWR;

		/* store a copy of the file name */
		opt->outfile = argi->output_arg;
		if ((ctx->outfd = open(argi->output_arg, oflags, 0644)) < 0) {
			rc = 1;
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
	if (prer.init_main_f != NULL) {
		if ((rc = prer.init_main_f(ctx, argi->nargs + 1U, argi->args - 1))) {
			if (rc < 0) {
				rc = 1;
			} else {
				/* we don't consider it an error */
				rc = 0;
			}
			goto clo_out;
		}
	} else if (prer.initf != NULL) {
		prer.initf(ctx);
	}

	if (argi->nargs == 0U && !isatty(STDIN_FILENO)) {
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
		slsz = get_slut_sz((void*)pg);
		while ((nrd = rdpg(pg, 4096, STDIN_FILENO)) > 0) {
			if (UNLIKELY((size_t)nrd < sizeof(pg))) {
				/* last page then? */
				nrd -= slsz;
			}
			prpg(ctx, pg, nrd, prer.prf);
		}

	} else {
		for (size_t j = 0U; j < argi->nargs; j++) {
			pr1(ctx, argi->args[j], prer.prf);
		}
	}

	/* check and call finaliser if any */
fina:
	if (prer.finif != NULL) {
		prer.finif(ctx);
	}
clo_out:
	/* close the output file */
	close(ctx->outfd);

out:
	unfind_printer(prer);
	ute_module_fini();
	yuck_free(argi);
	return rc;
}
#endif	/* STANDALONE */

/* ute-print.c ends here */
