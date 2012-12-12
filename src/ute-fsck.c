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
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include "utefile-private.h"
#include "utefile.h"
#include "scommon.h"
#include "boobs.h"

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
# include <assert.h>
# define UDEBUG(args...)	fprintf(stderr, args)
# define UNUSED_dbg(x)		UNUSED(x)
# define UNUSED_nodbg(x)	x
#else
# define UDEBUG(args...)
# define UNUSED_dbg(x)		x
# define UNUSED_nodbg(x)	UNUSED(x)
#endif	/* DEBUG_FLAG */

typedef struct fsck_ctx_s *fsck_ctx_t;

struct fsck_ctx_s {
	bool dryp:1;
	bool verbp:1;
	ute_end_t tgtend;
	ute_end_t natend;
	utectx_t outctx;
};


/* helper functions */
static void
__attribute__((format(printf, 1, 2)))
verbprf(const char *UNUSED_nodbg(fmt), ...)
{
#if defined DEBUG_FLAG
	va_list vap;
	va_start(vap, fmt);
	vfprintf(stderr, fmt, vap);
	va_end(vap);
#endif	/* DEBUG_FLAG */
	return;
}

static void
__attribute__((format(printf, 2, 3)))
error(int eno, const char *fmt, ...)
{
	va_list vap;
	va_start(vap, fmt);
	vfprintf(stderr, fmt, vap);
	va_end(vap);
	if (eno || errno) {
		fputc(':', stderr);
		fputc(' ', stderr);
		fputs(strerror(eno ?: errno), stderr);
	}
	fputc('\n', stderr);
	return;
}


/* converters */
#if defined WORDS_BIGENDIAN
# define letobe32	le32toh
# define letobe64	le64toh
# define betole32	htole32
# define betole64	htole64
# define swap32		le32toh
# define swap64		le64toh
#else  /* !WORDS_BIGENDIAN */
# define letobe32	htobe32
# define letobe64	htobe64
# define betole32	be32toh
# define betole64	be64toh
# define swap32		be32toh
# define swap64		be64toh
#endif	/* WORDS_BIGENDIAN */

static void
conv_1_swap(scom_thdr_t ti, size_t tbsz)
{
	uint32_t *sndwch = (uint32_t*)ti;
	uint64_t *sndw64 = (uint64_t*)ti;

	/* header is always 64b */
	sndw64[0] = swap64(sndw64[0]);
	switch (tbsz) {
	case 4:
		sndwch[8] = swap32(sndwch[8]);
		sndwch[9] = swap32(sndwch[9]);
		sndwch[10] = swap32(sndwch[10]);
		sndwch[11] = swap32(sndwch[11]);
		sndwch[12] = swap32(sndwch[12]);
		sndwch[13] = swap32(sndwch[13]);
		sndwch[14] = swap32(sndwch[14]);
		sndwch[15] = swap32(sndwch[15]);
	case 2:
		sndwch[4] = swap32(sndwch[4]);
		sndwch[5] = swap32(sndwch[5]);
		sndwch[6] = swap32(sndwch[6]);
		sndwch[7] = swap32(sndwch[7]);
	case 1:
		sndwch[2] = swap32(sndwch[2]);
		sndwch[3] = swap32(sndwch[3]);
	default:
		break;
	}
	return;
}

static void
__addconv_tick(utectx_t hdl, scom_t si, size_t tbsz)
{
/* do a conversion and then add the tick */
	uint32_t ALGN(ti[16], sizeof(uint64_t));
	const uint32_t *src_sndwch = (const uint32_t*)si;
	const uint64_t *src_sndw64 = (const uint64_t*)si;
	uint32_t *tgt_sndwch = (uint32_t*)ti;
	uint64_t *tgt_sndw64 = (uint64_t*)ti;

	/* header is always 64b */
	tgt_sndw64[0] = swap64(src_sndw64[0]);
	switch (tbsz) {
	case 4:
		tgt_sndwch[8] = swap32(src_sndwch[8]);
		tgt_sndwch[9] = swap32(src_sndwch[9]);
		tgt_sndwch[10] = swap32(src_sndwch[10]);
		tgt_sndwch[11] = swap32(src_sndwch[11]);
		tgt_sndwch[12] = swap32(src_sndwch[12]);
		tgt_sndwch[13] = swap32(src_sndwch[13]);
		tgt_sndwch[14] = swap32(src_sndwch[14]);
		tgt_sndwch[15] = swap32(src_sndwch[15]);
	case 2:
		tgt_sndwch[4] = swap32(src_sndwch[4]);
		tgt_sndwch[5] = swap32(src_sndwch[5]);
		tgt_sndwch[6] = swap32(src_sndwch[6]);
		tgt_sndwch[7] = swap32(src_sndwch[7]);
	case 1:
		tgt_sndwch[2] = swap32(src_sndwch[2]);
		tgt_sndwch[3] = swap32(src_sndwch[3]);
	default:
		break;
	}
	ute_add_tick(hdl, AS_SCOM(ti));
	return;
}


/* page wise operations */
enum {
	ISS_NO_ISSUES = 0,
	ISS_OLD_VER = 1,
	ISS_UNSORTED = 2,
	ISS_NO_ENDIAN = 4,
};

/* the actual fscking */
static int
fsckp(fsck_ctx_t ctx, uteseek_t sk, utectx_t hdl, int old_iss, scidx_t last)
{
	const size_t ssz = sizeof(*sk->sp);
	const size_t sk_sz = seek_byte_size(sk);
	bool same_end_p = ute_check_endianness(hdl) == 0;
	int issues = 0;

	for (size_t i = sk->si * ssz, tsz; i < sk_sz; i += tsz) {
		char ALGN(buf[64], 16);
		scom_thdr_t nu_ti = AS_SCOM_THDR(buf);
		scom_thdr_t ti = AS_SCOM_THDR(sk->sp + i / ssz);
		uint64_t x;

		if (UNLIKELY(old_iss & ISS_OLD_VER)) {
			/* promote the old header
			 * copy to tmp buffer BUF */
			scom_promote_v01(nu_ti, ti);

			/* now to what we always do */
			if (ctx->dryp) {
				ti = nu_ti;
			} else if (ctx->outctx) {
				tsz = scom_byte_size(nu_ti);
				memcpy(nu_ti + 1, ti + 1, tsz - sizeof(*ti));
				ti = nu_ti;
			} else /*if (ctx->outctx == NULL && !ctx->dryp)*/ {
				/* flush back to our page ... */
				*ti = *nu_ti;
			}
		}

		if (LIKELY(same_end_p)) {
			x = ti->u;
		} else {
			x = swap64(ti->u);
		}

		/* determine the length for the increment */
		tsz = scom_byte_size(AS_SCOM(&x));

		/* check for sortedness */
		if (last.u > x) {
			verbprf("  tick p%u/%zu  %lx > %lx\n",
				sk->pg, i / ssz, last.u, x);
			issues |= ISS_UNSORTED;
		}
		last.u = x;

		/* copy the whole shebang when -o|--output is given */
		if (!ctx->dryp && ctx->outctx && same_end_p) {
			/* just add the guy */
			ute_add_tick(ctx->outctx, ti);
		} else if (!ctx->dryp && ctx->outctx) {
			/* convert to native endianness and add */
			__addconv_tick(ctx->outctx, ti, tsz / sizeof(*sk->sp));
		}
	}
	/* deal with issues that need page-wise dealing */
	if (issues & ISS_UNSORTED) {
		const char *fn = ute_fn(hdl);
		printf("file `%s' page %u needs sorting ...\n", fn, sk->pg);
		if (!ctx->dryp && ctx->outctx == NULL) {
			/* we need to set seek's si accordingly */
			sk->si = sk_sz / ssz;
			seek_sort(sk);
		}
	}
	return issues;
}

static void
conv_sk_swap(fsck_ctx_t ctx, uteseek_t sk)
{
/* only inplace (in situ) conversion is supported */
	const size_t ssz = sizeof(*sk->sp);
	const size_t sk_sz = seek_byte_size(sk);

	if (ctx->dryp) {
		return;
	}
	for (size_t i = sk->si * ssz, tsz; i < sk_sz; i += tsz) {
		scom_thdr_t ti = AS_SCOM_THDR(sk->sp + i / ssz);
		size_t xsz;

		/* determine the length for the increment */
		tsz = scom_byte_size(ti);
		xsz = scom_tick_size(ti);

		conv_1_swap(ti, xsz);
	}
	return;
}

static void
conv_sk_betole(fsck_ctx_t ctx, uteseek_t sk)
{
	conv_sk_swap(ctx, sk);
	return;
}

static void
conv_sk_letobe(fsck_ctx_t ctx, uteseek_t sk)
{
	conv_sk_swap(ctx, sk);
	return;
}


/* file wide operations */
static int
fsck1(fsck_ctx_t ctx, utectx_t hdl, const char *fn)
{
	size_t npg;
	scidx_t last = {
		.u = 0ULL,
	};
	/* start off with no issues */
	int issues = 0;

	/* check for ute version */
	if (UNLIKELY(ute_version(hdl) == UTE_VERSION_01)) {
		/* we need to flip the ti,
		 * don't bother checking the endianness in this case as
		 * no UTEv0.1 files will have that */
		printf("file `%s' needs upgrading (ute format 0.1) ...\n", fn);
		issues |= ISS_OLD_VER;
	} else if (UNLIKELY(ute_endianness(hdl) == UTE_ENDIAN_UNK)) {
		/* great, we MUST assume it's little endian */
		printf("file `%s' has no endianness indicator ...\n", fn);
		issues |= ISS_NO_ENDIAN;
	}
	/* go through the pages manually */
	npg = ute_npages(hdl);
	if (ctx->verbp) {
		fprintf(stderr, "inspecting %zu pages\n", npg);
	}
	for (size_t p = 0; p < npg + tpc_has_ticks_p(hdl->tpc); p++) {
		struct uteseek_s sk[1];

		/* create a new seek */
		seek_page(sk, hdl, p);
		if (ctx->verbp) {
			fprintf(stderr, "page %zu ...\n", p);
		}
		/* fsck that one page */
		issues |= fsckp(ctx, sk, hdl, issues, last);
		/* flush the old seek */
		flush_seek(sk);
	}

#if defined DEBUG_FLAG
	/* second time lucky, there should be no page related issues anymore */
	for (size_t p = 0;
	     !ctx->dryp && ctx->outctx == NULL &&
		     p < npg + tpc_has_ticks_p(hdl->tpc);
	     p++) {
		struct uteseek_s sk[1];
		int iss = 0;

		/* create a new seek */
		seek_page(sk, hdl, p);
		/* fsck that one page */
		ctx->dryp = true;
		iss = fsckp(ctx, sk, hdl, 0, last);
		assert(!(iss & ISS_UNSORTED));
		ctx->dryp = false;
		/* flush the old seek */
		flush_seek(sk);
	}
#endif	/* DEBUG_FLAG */

	if (ctx->outctx) {
		hdl = ctx->outctx;
		fn = ute_fn(ctx->outctx);
	}
	/* print diagnostics */
	if ((issues & ISS_OLD_VER) && !ctx->dryp) {
		/* update the header version */
		const char *ver = hdl->hdrc->version;
		bump_header(hdl->hdrc);
		printf(" ... `%s' upgraded: %s\n", fn, ver);
	} else if ((issues & ISS_NO_ENDIAN) && !ctx->dryp) {
		/* just bump the header again */
		bump_header(hdl->hdrc);
		printf(" ... `%s' endian indicator added\n", fn);
	}
	if ((issues & ISS_UNSORTED) && !ctx->dryp) {
		/* just to be sure */
		printf(" ... `%s' sorting\n", fn);
		ute_set_unsorted(hdl);
	}
	if ((issues & ISS_UNSORTED) && !ctx->dryp && ute_sorted_p(hdl)) {
		printf(" ... `%s' sorted\n", fn);
	}
	return issues;
}

static void
conv_le(fsck_ctx_t ctx, utectx_t hdl)
{
	switch (ute_endianness(hdl)) {
	case UTE_ENDIAN_UNK:
	case UTE_ENDIAN_LITTLE:
		/* bingo, fuckall to do */
		break;
	case UTE_ENDIAN_BIG:
		/* go through them pages manually */
		for (size_t p = 0, npg = ute_npages(hdl);
		     p < npg + tpc_has_ticks_p(hdl->tpc);
		     p++) {
			struct uteseek_s sk[1];

			/* create a new seek */
			seek_page(sk, hdl, p);
			/* convert that one page */
			conv_sk_betole(ctx, sk);
			/* flush the old seek */
			flush_seek(sk);
		}
		break;
	default:
		break;
	}
	return;
}

static void
conv_be(fsck_ctx_t ctx, utectx_t hdl)
{
	switch (ute_endianness(hdl)) {
	case UTE_ENDIAN_UNK:
	case UTE_ENDIAN_LITTLE:
		/* go through them pages manually */
		for (size_t p = 0, npg = ute_npages(hdl);
		     p < npg + tpc_has_ticks_p(hdl->tpc);
		     p++) {
			struct uteseek_s sk[1];

			/* create a new seek */
			seek_page(sk, hdl, p);
			/* convert that one page */
			conv_sk_letobe(ctx, sk);
			/* flush the old seek */
			flush_seek(sk);
		}
	case UTE_ENDIAN_BIG:
		/* bingo, fuckall to do */
		break;
	default:
		break;
	}
	return;
}

#if defined HAVE_LZMA_H
static void
ute_compress(utectx_t hdl)
{
	hdl->hdrc->flags |= UTEHDR_FLAG_COMPRESSED;
	hdl->hdrc->flags |= UTEHDR_FLAG_DIRTY;
	/* we also go for the minimum header size */
	hdl->ploff = UTEHDR_MIN_SIZE;
	return;
}

#else  /* !HAVE_LZMA_H */

static void
ute_compress(utectx_t hdl)
{
	const char *fn = ute_fn(hdl);

	error(0, "compression requested for '%s' but no lzma support", fn);
	return;
}
#endif	/* HAVE_LZMA_H */


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
	struct fsck_ctx_s ctx[1] = {0};
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
	if (argi->dry_run_given) {
		ctx->dryp = true;
	}
	if (argi->verbose_given) {
		ctx->verbp = true;
	}

	/* determine native endianness */
#if defined WORDS_BIGENDIAN
	ctx->natend = UTE_ENDIAN_BIG;
#else  /* !WORDS_BIGENDIAN */
	ctx->natend = UTE_ENDIAN_LITTLE;
#endif	/* WORDS_BIGENDIAN */
	if (argi->little_endian_given) {
		ctx->tgtend = UTE_ENDIAN_LITTLE;
	} else if (argi->big_endian_given) {
		ctx->tgtend = UTE_ENDIAN_BIG;
	} else {
		/* use native endianness */
		ctx->tgtend = ctx->natend;
	}

	/* set the compression level in either case */
	ute_encode_clevel = argi->compression_level_arg;

	if (!argi->dry_run_given && argi->output_given) {
		const int fl = UO_RDWR | UO_CREAT | UO_TRUNC;
		const char *fn = argi->output_arg;

		if ((ctx->outctx = ute_open(fn, fl)) == NULL) {
			error(0, "cannot open output file `%s'", fn);
			res = -1;
			goto out;
		}
	}

	for (unsigned int j = 0; j < argi->inputs_num; j++) {
		const char *fn = argi->inputs[j];
		const int fl = (ctx->dryp || ctx->outctx ? UO_RDONLY : UO_RDWR);
		const int opfl = UO_NO_LOAD_TPC;
		utectx_t hdl;

		if ((hdl = ute_open(fn, fl | opfl)) == NULL) {
			error(0, "cannot open file `%s'", fn);
			res = 1;
			continue;
		}

		/* the actual checking */
		if (fsck1(ctx, hdl, fn)) {
			res = 1;

			if (argi->little_endian_given) {
				error(0, "\
cannot convert file with issues `%s', rerun conversion later", fn);
			}
		} else if (ctx->outctx != NULL) {
			/* no endian conversions in this case */
			;
		} else if (ctx->dryp) {
			/* do fuckall in dry mode */
			;
		} else if (ctx->tgtend == UTE_ENDIAN_LITTLE) {
			conv_le(ctx, hdl);
		} else if (ctx->tgtend == UTE_ENDIAN_BIG) {
			conv_be(ctx, hdl);
		}

		/* safe than sorry */
		if (ctx->outctx != NULL) {
			ute_clone_slut(ctx->outctx, hdl);
		} else if (ctx->dryp) {
			/* do fuckall in dry mode */
			;
		} else {
			/* make sure we set the new endianness */
			ute_set_endianness(hdl, ctx->tgtend);

			if (argi->compress_given) {
				ute_compress(hdl);
			}
		}

		/* and that's us */
		ute_close(hdl);
	}

	if (ctx->outctx != NULL) {
		/* re-open the file */
		const int fl = UO_RDWR;
		const int opfl = UO_NO_LOAD_TPC;
		const char *fn = argi->output_arg;
		utectx_t hdl;

		/* finalise the whole shebang */
		ute_close(ctx->outctx);

		/* care about conversions now */
		if (0) {
			/* cosmetics */
		} else if (!argi->little_endian_given &&
			   !argi->big_endian_given &&
			   !argi->compress_given) {
			/* nothing to do */
			goto out;
		} else if (ctx->dryp) {
			/* dry mode, do fuck all */
			goto out;
		} else if ((hdl = ute_open(fn, fl | opfl)) == NULL) {
			error(0, "cannot open file `%s'", fn);
			res = 1;
			goto out;
		} else if (ctx->tgtend == UTE_ENDIAN_LITTLE) {
			/* inplace little endian conversion */
			conv_le(ctx, hdl);
		} else if (ctx->tgtend == UTE_ENDIAN_BIG) {
			/* inplace big endian conversion */
			conv_be(ctx, hdl);
		}
		/* make sure it's the right endianness */
		ute_set_endianness(hdl, ctx->tgtend);

		if (argi->compress_given) {
			ute_compress(hdl);
		}
		/* and close the whole shebang again */
		ute_close(hdl);
	}

	if (argi->compress_given) {
		(void)ute_encode(NULL, NULL, 0);
		(void)ute_decode(NULL, NULL, 0);
	}
out:
	fsck_parser_free(argi);
	return res;
}
#endif	/* STANDALONE */

/* ute-fsck.c ends here */
