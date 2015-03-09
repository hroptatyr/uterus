/*** ute-fsck.c -- ute file checker
 *
 * Copyright (C) 2012-2015 Sebastian Freundt
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
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#if defined(HAVE_FUTIMES) || defined(HAVE_FUTIMESAT) || defined(HAVE_UTIMES)
# include <sys/time.h>
#elif defined(HAVE_UTIME)
# include <utime.h>
#endif
#include "utefile-private.h"
#include "utefile.h"
#include "scommon.h"
#include "boobs.h"

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
	/* dryp from command line */
	bool a_dryp:1;
	/* compress or decompress using temporaries */
	bool zp:1;
	ute_end_t tgtend;
	ute_end_t natend;
	utectx_t outctx;
};


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
conv_1_swap(scom_thdr_t ti, size_t tz)
{
/* convert the tick in TI to opposite endianness */
	uint32_t *sndwch = (uint32_t*)ti;
	uint64_t *sndw64 = (uint64_t*)ti;

	/* header is always 64b */
	sndw64[0] = swap64(sndw64[0]);
	switch (tz) {
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
conv_sk_swap(fsck_ctx_t ctx, uteseek_t sk, bool src_is_native_endian_p)
{
/* only inplace (in situ) conversion is supported */

	if (ctx->dryp) {
		return;
	}
	for (struct sndwch_s *sp = sk->sp + sk->si,
		     *ep = sk->sp + seek_tick_size(sk);
	     sp < ep;) {
		scom_thdr_t ti = AS_SCOM_THDR(sp);
		size_t tz;

		if (src_is_native_endian_p) {
			tz = scom_tick_size(ti);
		} else {
			uint64_t x = swap64(ti->u);
			tz = scom_tick_size(AS_SCOM(&x));
		}

		/* just do the conversion */
		conv_1_swap(ti, tz);
		sp += tz;
	}
	return;
}

static void
conv_sk_betole(fsck_ctx_t ctx, uteseek_t sk, bool src_is_native_endian_p)
{
	conv_sk_swap(ctx, sk, src_is_native_endian_p);
	return;
}

static void
conv_sk_letobe(fsck_ctx_t ctx, uteseek_t sk, bool src_is_native_endian_p)
{
	conv_sk_swap(ctx, sk, src_is_native_endian_p);
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
#if defined WORDS_BIGENDIAN
			conv_sk_betole(ctx, sk, true);
#else  /* !WORDS_BIGENDIAN */
			conv_sk_betole(ctx, sk, false);
#endif	/* WORDS_BIGENDIAN */
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
#if defined WORDS_BIGENDIAN
			conv_sk_letobe(ctx, sk, false);
#else  /* !WORDS_BIGENDIAN */
			conv_sk_letobe(ctx, sk, true);
#endif	/* WORDS_BIGENDIAN */
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
	return;
}

static void
ute_decompress(utectx_t hdl)
{
	hdl->hdrc->flags &= ~UTEHDR_FLAG_COMPRESSED;
	hdl->hdrc->flags |= UTEHDR_FLAG_DIRTY;
	return;
}

#else  /* !HAVE_LZMA_H */

static void
ute_compress(utectx_t hdl)
{
	const char *fn = ute_fn(hdl);

	error("compression requested for '%s' but no lzma support", fn);
	return;
}

static void
ute_decompress(utectx_t hdl)
{
	const char *fn = ute_fn(hdl);

	error("decompression requested for '%s' but no lzma support", fn);
	return;
}
#endif	/* HAVE_LZMA_H */

static int
file_flags(fsck_ctx_t ctx, const char *fn)
{
	struct stat st;

	if (ctx->outctx != NULL || ctx->zp) {
		return UO_RDONLY;
	} else if (UNLIKELY(stat(fn, &st) < 0)) {
		/* we don't want to know what's wrong here */
		error("cannot process file '%s'", fn);
		return -1;
	} else if (UNLIKELY(!ctx->a_dryp && !(st.st_mode & S_IWUSR)) ||
		   LIKELY(ctx->a_dryp)) {
		/* user didn't request creation, so fuck off here */
		ctx->dryp = true;
		return UO_RDONLY;
	}
	return UO_RDWR;
}

static char*
file_tmpnam(const char *fn)
{
/* generate a temporary file name that looks like FN */
	size_t fz = strlen(fn);
	char *res = malloc(fz + 3U/*.xz*/ + 1U/*\0*/);

	memcpy(res, fn, fz);
	memcpy(res + fz, ".xz", sizeof(".xz"));
	return res;
}

static int
touchfn(const char *fn, struct stat st[static 1U])
{
/* copy mtime and atime */
	long atime_nsec;
	long mtime_nsec;
	int rc;

	/* first off all, get the highres stamp (if any) */
#if defined HAVE_STRUCT_STAT_ST_ATIM_TV_NSEC
	atime_nsec = st->st_atim.tv_nsec;
	mtime_nsec = st->st_mtim.tv_nsec;

#elif defined HAVE_STRUCT_STAT_ST_ATIMESPEC_TV_NSEC
	atime_nsec = st->st_atimespec.tv_nsec;
	mtime_nsec = st->st_mtimespec.tv_nsec;

#elif defined(HAVE_STRUCT_STAT_ST_ATIMENSEC)
	atime_nsec = st->st_atimensec;
	mtime_nsec = st->st_mtimensec;

#elif defined(HAVE_STRUCT_STAT_ST_UATIME)
	atime_nsec = st->st_uatime * 1000;
	mtime_nsec = st->st_umtime * 1000;

#elif defined(HAVE_STRUCT_STAT_ST_ATIM_ST__TIM_TV_NSEC)
	atime_nsec = st->st_atim.st__tim.tv_nsec;
	mtime_nsec = st->st_mtim.st__tim.tv_nsec;

#else  /* none of the above */
	atime_nsec = 0L;
	mtime_nsec = 0L;
#endif

	/* now set */
#if defined HAVE_UTIMENS || defined HAVE_UTIMENSAT
	{
		struct timespec tv[2] = {
			[0] = {.tv_sec = st->st_atime, .tv_nsec = atime_nsec},
			[1] = {.tv_sec = st->st_mtime, .tv_nsec = mtime_nsec},
		};

# if defined HAVE_UTIMENS
		rc = utimens(fn, tv);
# elif defined HAVE_UTIMENSAT
		rc = utimensat(-1, fn, tv, 0);
# endif
	}
#elif defined HAVE_UTIMES
	/* use microsecond precision */
	{
		struct timeval tv[2] = {
			[0] = {
				.tv_sec = st->st_atime,
				.tv_usec = atime_nsec / 1000
			},
			[1] = {
				.tv_sec = st->st_mtime,
				.tv_usec = mtime_nsec / 1000
			}
		};

		rc = utimes(fn, tv);
	}

#elif defined HAVE_UTIME
	/* no high-res timestamps here */
	{
		struct utimbuf buf = {
			.actime = st->st_atime,
			.modtime = st->st_mtime,
		};

		/* -warnings */
		(void)atime_nsec;
		(void)mtime_nsec;

		rc = utime(fn, &buf);
	}

#else  /* just indicate failure */
	errno = 0, rc = -1;

#endif
	return rc;
}


#if defined STANDALONE
#include "ute-fsck.yucc"

int
main(int argc, char *argv[])
{
	yuck_t argi[1U];
	struct fsck_ctx_s ctx[1U] = {0};
	size_t nsucc;
	int rc = 0;

	if (yuck_parse(argi, argc, argv)) {
		rc = 1;
		goto out;
	}

	/* copy intercting stuff into our own context */
	if (argi->dry_run_flag) {
		ctx->a_dryp = true;
	}
	if (argi->verbose_flag) {
		ctx->verbp = true;
	}

	/* determine native endianness */
#if defined WORDS_BIGENDIAN
	ctx->natend = UTE_ENDIAN_BIG;
#else  /* !WORDS_BIGENDIAN */
	ctx->natend = UTE_ENDIAN_LITTLE;
#endif	/* WORDS_BIGENDIAN */
	if (argi->little_endian_flag) {
		ctx->tgtend = UTE_ENDIAN_LITTLE;
	} else if (argi->big_endian_flag) {
		ctx->tgtend = UTE_ENDIAN_BIG;
	} else {
		/* use native endianness */
		ctx->tgtend = ctx->natend;
	}

	/* set the comprcsion level in either case */
	if (argi->compression_level_arg) {
		ute_encode_clevel =
			strtoul(argi->compression_level_arg, NULL, 10);
	} else {
		/* default value */
		ute_encode_clevel = 6;
	}

	if (!argi->dry_run_flag && argi->output_arg) {
		const int fl = UO_RDWR | UO_CREAT | UO_TRUNC;
		const char *fn = argi->output_arg;

		if ((ctx->outctx = ute_open(fn, fl)) == NULL) {
			error("cannot open output file `%s'", fn);
			rc = 1;
			goto out;
		}
	}

	if (argi->compress_flag && argi->decompress_flag) {
		fputs("\
only one of -z|--compress and -d|--decompress can be given\n", stderr);
		rc = 1;
		goto out;
	} else if (!argi->output_arg &&
		   (argi->compress_flag || argi->decompress_flag)) {
		ctx->zp = true;
	}

	for (size_t j = 0U; j < argi->nargs; j++) {
		const char *fn = argi->args[j];
		const int fl = file_flags(ctx, fn);
		const int opfl = UO_NO_LOAD_TPC;
		utectx_t hdl;
		char *zfn;

		if (UNLIKELY(fl < 0)) {
			/* error got probably printed already */
			continue;
		} else if (UNLIKELY((hdl = ute_open(fn, fl | opfl)) == NULL)) {
			error("cannot open file `%s'", fn);
			rc = 1;
			continue;
		}

		if (ctx->zp) {
			/* we'll pretend we've passed -o|--output
			 * lest we disrupt the original */
			const int zfl = UO_RDWR | UO_CREAT | UO_TRUNC;
			struct stat st;

			if ((zfn = file_tmpnam(fn)) == NULL) {
				errno = 0, error("\
cannot construct temporary file name for `%s'", fn);
				rc = 1;
				continue;
			} else if (stat(zfn, &st) != -1) {
				errno = 0, error("\
temporary file name `%s' already exists", zfn);
				rc = 1;
				continue;
			} else if (errno != ENOENT) {
				errno = 0, error("\
cannot stat temporary file name `%s'", zfn);
				rc = 1;
				continue;
			} else if ((ctx->outctx = ute_open(zfn, zfl)) == NULL) {
				error("\
cannot open temporary file `%s'", zfn);
				rc = 1;
				continue;
			}
		}

		/* the actual checking */
		if (fsck1(ctx, hdl, fn)) {
			rc = 1;

			if (argi->little_endian_flag) {
				error("\
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

			if (argi->compress_flag) {
				ute_compress(hdl);
			} else if (argi->decompress_flag) {
				ute_decompress(hdl);
			}
		}

		/* and that's us */
		ute_close(hdl);

		/* move temporaries */
		if (ctx->zp) {
			struct stat st;

			/* make sure we set the new endianness */
			ute_set_endianness(ctx->outctx, ctx->tgtend);

			if (argi->compress_flag) {
				ute_compress(ctx->outctx);
			} else if (argi->decompress_flag) {
				ute_decompress(ctx->outctx);
			}

			/* store timestamps in question for later use */
			if (UNLIKELY(stat(fn, &st) < 0)) {
				error("\
warning: cannot preserve original time stamps");
			}
			/* now close and rename the guy */
			ute_close(ctx->outctx);
			/* make the original timestamps stick */
			if (UNLIKELY(touchfn(zfn, &st) < 0)) {
				error("\
warning: cannot preserve original time stamps");
			}
			if (rename(zfn, fn) < 0) {
				error("\
temporary file cannot be renamed to `%s'", fn);
			}
			ctx->outctx = NULL;
			free(zfn);
		}

		/* count this as success */
		nsucc++;
	}

	if (ctx->outctx != NULL && !nsucc) {
		/* none succeeded? short-circuit here */
		(void)unlink(argi->output_arg);
	} else if (ctx->outctx != NULL) {
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
		} else if (!argi->little_endian_flag &&
			   !argi->big_endian_flag &&
			   !argi->compress_flag) {
			/* nothing to do */
			goto out;
		} else if (ctx->dryp) {
			/* dry mode, do fuck all */
			goto out;
		} else if ((hdl = ute_open(fn, fl | opfl)) == NULL) {
			error("cannot open file `%s'", fn);
			rc = 1;
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

		if (argi->compress_flag) {
			ute_compress(hdl);
		} else if (argi->decompress_flag) {
			ute_decompress(hdl);
		}
		/* and close the whole shebang again */
		ute_close(hdl);
	}

	if (argi->compress_flag || argi->decompress_flag) {
		ute_encode_free();
		ute_decode_free();
	}
out:
	yuck_free(argi);
	return rc;
}
#endif	/* STANDALONE */

/* ute-fsck.c ends here */
