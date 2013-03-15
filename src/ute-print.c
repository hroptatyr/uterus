/*** ute-print.c -- ute tick printer
 *
 * Copyright (C) 2010-2013 Sebastian Freundt
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
	/* check for ute version */
	if (UNLIKELY(ute_version(hdl) == UTE_VERSION_01)) {
		UTE_ITER_CUST(ti, tsz, hdl) {
			/* we need to flip the ti */
			char buf[64];
			scom_thdr_t nu_ti = AS_SCOM_THDR(buf);
			size_t bsz;

			if (UNLIKELY(ti == NULL)) {
				tsz = 1;
				continue;
			}

			/* promote the old header, copy to tmp buffer BUF */
			scom_promote_v01(nu_ti, ti);
			tsz = scom_tick_size(nu_ti);
			bsz = scom_byte_size(nu_ti);
			/* copy the rest of the tick into the buffer */
			memcpy(buf + sizeof(*nu_ti), ti + 1, bsz - sizeof(*ti));
			/* now to what we always do */
			prf(ctx, nu_ti);
		}
	} else if (UNLIKELY(ute_check_endianness(hdl) < 0)) {
		/* properly padded for big-e and little-e */
#define AS_GEN(x)	((const struct gen_s*)(x))
		struct gen_s {
			union scom_thdr_u scom[1];
			uint32_t v[14];
		};

		/* transparent flipping */
		UTE_ITER_CUST(ti, tsz, hdl) {
			/* tmp storage for the flip */
			struct gen_s tmp;

			if (UNLIKELY(ti == NULL)) {
				tsz = 1;
				continue;
			}

			/* swap ti into buf */
			tmp.scom->u = htooe64(ti->u);
			switch ((tsz = scom_tick_size(tmp.scom))) {
			case 2:
				tmp.v[2] = htooe32(AS_GEN(ti)->v[2]);
				tmp.v[3] = htooe32(AS_GEN(ti)->v[3]);
				tmp.v[4] = htooe32(AS_GEN(ti)->v[4]);
				tmp.v[5] = htooe32(AS_GEN(ti)->v[5]);
			case 1:
				tmp.v[0] = htooe32(AS_GEN(ti)->v[0]);
				tmp.v[1] = htooe32(AS_GEN(ti)->v[1]);
				break;
			case 4:
			default:
				tsz = 1;
				continue;
			}
			/* now to what we always do */
			prf(ctx, tmp.scom);
		}
	} else {
		/* no flips in this one */
		UTE_ITER(ti, hdl) {
			prf(ctx, ti);
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
	struct print_parser_params parm = {
		.override = 1,
		.initialize = 1,
		.check_required = 1,
		.check_ambiguity = 0,
		.print_errors = 0,
	};
	struct pr_ctx_s ctx[1] = {{0}};
	struct pr_opt_s opt[1] = {{0}};
	struct printer_s prer;
	int printer_specific_options_p = 0;
	const char *fmt;
	int res = 0;

	if (print_parser_ext(argc, argv, argi, &parm)) {
		/* maybe we've got as far as to parse --format already */
		if (argi->format_arg == NULL) {
			res = 1;
			goto out;
		}
		/* otherwise try the printer initialiser */
		printer_specific_options_p = 1;
	} else if (argi->help_given && argi->format_arg == NULL) {
		print_parser_print_help();
		fputc('\n', stdout);
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

	if (UNLIKELY((prer = find_printer(fmt), prer.prf == NULL))) {
		/* we need a printer, so piss off here */
		fputs("printer format unknown\n", stderr);
		res = 1;
		goto out;
	} else if (printer_specific_options_p && prer.init_main_f == NULL) {
		fputs("\
printer specific options given but cannot find printer\n", stderr);
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
	if (prer.init_main_f != NULL) {
		if ((res = prer.init_main_f(ctx, argc, argv))) {
			if (res < 0) {
				res = 1;
			} else {
				/* we don't consider it an error */
				res = 0;
			}
			goto clo_out;
		}
	} else if (prer.initf != NULL) {
		prer.initf(ctx);
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
		slsz = get_slut_sz((void*)pg);
		while ((nrd = rdpg(pg, 4096, STDIN_FILENO)) > 0) {
			if (UNLIKELY((size_t)nrd < sizeof(pg))) {
				/* last page then? */
				nrd -= slsz;
			}
			prpg(ctx, pg, nrd, prer.prf);
		}

	} else {
		for (unsigned int j = 0; j < argi->inputs_num; j++) {
			pr1(ctx, argi->inputs[j], prer.prf);
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
	print_parser_free(argi);
	return res;
}
#endif	/* STANDALONE */

/* ute-print.c ends here */
