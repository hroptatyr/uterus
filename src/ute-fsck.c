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

typedef struct fsck_ctx_s *fsck_ctx_t;

struct fsck_ctx_s {
	bool dryp;
};


static scom_thdr_t
tpc_get_scom_thdr(utetpc_t tpc, sidx_t i)
{
	if (UNLIKELY(i > tpc->sk.si)) {
		return NULL;
	}
	return AS_SCOM_THDR(tpc->sk.sp + i);
}

static scom_thdr_t
seek_get_scom_thdr(uteseek_t sk)
{
	if (UNLIKELY(sk->si * sizeof(*sk->sp) >= sk->sz)) {
		return NULL;
	}
	return AS_SCOM_THDR(sk->sp + sk->si);
}

static void
reseek(utectx_t ctx, sidx_t i)
{
	uint32_t p = page_of_index(ctx, i);
	uint32_t o = offset_of_index(ctx, i);

	/* flush the old seek */
	flush_seek(ctx->seek);
	/* create a new seek */
	seek_page(ctx->seek, ctx, p);
	ctx->seek->si = o;
	return;
}

static scom_thdr_t
__seek(utectx_t ctx, sidx_t i)
{
/* just like ute_seek() but returns a scom_thdr */
	if (UNLIKELY(index_past_eof_p(ctx, i))) {
		sidx_t new_i = index_to_tpc_index(ctx, i);
		return tpc_get_scom_thdr(ctx->tpc, new_i);
	} else if (UNLIKELY(!index_in_seek_page_p(ctx, i))) {
		reseek(ctx, i);
	}
	return seek_get_scom_thdr(ctx->seek);
}

static void
fsck1(fsck_ctx_t ctx, const char *fn)
{
	void *hdl;
	const int fl = ctx->dryp ? UO_RDONLY : UO_RDWR;

	if ((hdl = ute_open(fn, fl)) == NULL) {
		return;
	}
	/* check for ute version */
	if (UNLIKELY(ute_version(hdl) == UTE_VERSION_01)) {
		/* we need to flip the ti */
		printf("file `%s' is ute format 0.1, upgrading ...\n", fn);
		for (size_t i = 0; i < ute_nticks(hdl);) {
			char buf[64];
			scom_thdr_t nu_ti = AS_SCOM_THDR(buf);
			scom_thdr_t ti = __seek(hdl, i);
			size_t tsz;

			/* promote the old header, copy to tmp buffer BUF */
			scom_promote_v01(nu_ti, ti);
			tsz = scom_thdr_size(nu_ti);
			/* now to what we always do */
			if (!ctx->dryp) {
				/* flush back to our page ... */
				memcpy(ti, buf, sizeof(*ti));
			}
			i += tsz / sizeof(struct sndwch_s);
		}
		printf("file `%s' upgraded\n", fn);
	} else {
		/* no flips in this one */
		for (size_t i = 0; i < ute_nticks(hdl);) {
			;
		}
	}
	if (!ctx->dryp) {
		/* update the header version */
		bump_header(((utectx_t)hdl)->hdrp);
		ute_flush(hdl);
	}
	/* oh right, close the handle */
	ute_close(hdl);
	return;
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
		fsck1(ctx, argi->inputs[j]);
	}

out:
	fsck_parser_free(argi);
	return res;
}
#endif	/* STANDALONE */

/* ute-fsck.c ends here */
