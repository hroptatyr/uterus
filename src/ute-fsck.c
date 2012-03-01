/*** ute-fsck.c -- ute file checker
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
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <fcntl.h>
#include "utefile-private.h"
#include "utefile.h"
#include "scommon.h"

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
# define UDEBUG(args...)	fprintf(stderr, args)
#else
# define UDEBUG(args...)
#endif	/* DEBUG_FLAG */

typedef struct fsck_ctx_s *fsck_ctx_t;

struct fsck_ctx_s {
	bool dryp;
};


/* helper functions */
static void
__attribute__((format(printf, 1, 2)))
verbprf(const char *fmt, ...)
{
#if defined DEBUG_FLAG
	va_list vap;
	va_start(vap, fmt);
	vfprintf(stderr, fmt, vap);
	va_end(vap);
#endif	/* DEBUG_FLAG */
	return;
}


/* actual fscking */
enum {
	ISS_NO_ISSUES,
	ISS_OLD_VER,
	ISS_UNSORTED,
};

static int
fsckp(fsck_ctx_t ctx, uteseek_t sk, const char *fn, scidx_t last)
{
	const size_t ssz = sizeof(*sk->sp);
	int issues = 0;

	for (size_t i = 0, tsz; i < sk->sz; i += tsz) {
		char buf[64];
		scom_thdr_t nu_ti = AS_SCOM_THDR(buf);
		scom_thdr_t ti = AS_SCOM_THDR(sk->sp + i / ssz);

		if (issues & ISS_OLD_VER) {
			/* promote the old header
			 * copy to tmp buffer BUF */
			scom_promote_v01(nu_ti, ti);

			/* now to what we always do */
			if (!ctx->dryp) {
				/* flush back to our page ... */
				memcpy(ti, buf, sizeof(*ti));
			} else {
				/* pretend we changed it */
				ti = nu_ti;
			}
		}

		/* check for sortedness */
		if (last.u > ti->u) {
			verbprf("  tick p%u/%zu  %lx > %lx\n",
				sk->pg, i / ssz, last.u, ti->u);
			issues |= ISS_UNSORTED;
		}
		last.u = ti->u;

		/* determine the length for the increment */
		tsz = scom_thdr_size(ti);
	}
	/* deal with issues that need page-wise dealing */
	if (issues & ISS_UNSORTED) {
		printf("file `%s' needs sorting ...\n", fn);
	}
	return issues;
}

static int
fsck1(fsck_ctx_t ctx, const char *fn)
{
	utectx_t hdl;
	const int fl = ctx->dryp ? UO_RDONLY : UO_RDWR;
	size_t npg;
	scidx_t last = {
		.u = 0ULL,
	};
	int issues = 0;

	if ((hdl = ute_open(fn, fl)) == NULL) {
		fprintf(stderr, "cannot open file `%s'\n", fn);
		return -1;
	}
	/* check for ute version */
	if (UNLIKELY(ute_version(hdl) == UTE_VERSION_01)) {
		/* we need to flip the ti */
		printf("file `%s' needs upgrading (ute format 0.1) ...\n", fn);
		issues |= ISS_OLD_VER;
	}
	/* go through the pages manually */
	npg = ute_npages(hdl);
	for (size_t p = 0; p < npg + tpc_has_ticks_p(hdl->tpc); p++) {
		struct uteseek_s sk[1];

		/* create a new seek */
		seek_page(sk, hdl, p);
		/* fsck that one page */
		issues |= fsckp(ctx, sk, fn, last);
		/* flush the old seek */
		flush_seek(sk);
	}

	if ((issues & ISS_OLD_VER) && !ctx->dryp) {
		/* update the header version */
		const char *ver = hdl->hdrp->version;
		bump_header(hdl->hdrp);
		ute_flush(hdl);
		printf(" ... `%s' upgraded: %s\n", fn, ver);
	}
	if ((issues & ISS_UNSORTED) && !ctx->dryp) {
		/* just to be sure */
		printf(" ... `%s' sorting\n", fn);
		ute_set_unsorted(hdl);
	}
	/* oh right, close the handle */
	ute_close(hdl);
	if ((issues & ISS_UNSORTED) && !ctx->dryp && ute_sorted_p(hdl)) {
		printf(" ... `%s' sorted\n", fn);
	}
	return issues;
}


#if defined STANDALONE
#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#endif	/* __INTEL_COMPILER */
#include "ute-fsck-clo.h"
#include "ute-fsck-clo.c"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
# pragma warning (default:181)
#endif	/* __INTEL_COMPILER */

int
main(int argc, char *argv[])
{
	struct fsck_args_info argi[1];
	struct fsck_ctx_s ctx[1];
	int res = 0;

	if (fsck_parser(argc, argv, argi)) {
		res = 1;
		goto out;
	} else if (argi->help_given) {
		fsck_parser_print_help();
		res = 0;
		goto out;
	}

	/* copy interesting stuff into our own context */
	if (!argi->dry_run_given) {
		ctx->dryp = false;
	} else {
		ctx->dryp = true;
	}

	for (unsigned int j = 0; j < argi->inputs_num; j++) {
		if (fsck1(ctx, argi->inputs[j])) {
			res = 1;
		}
	}

out:
	fsck_parser_free(argi);
	return res;
}
#endif	/* STANDALONE */

/* ute-fsck.c ends here */
