/*** mat.c -- matlab mat file muxer
 *
 * Copyright (C) 2012  Sebastian Freundt
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

#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>

#include "utefile.h"
#include "ute-print.h"

/* so we know about ticks, candles and snapshots */
#include "sl1t.h"
#include "scdl.h"

/* we're just as good as rudi, aren't we? */
#if defined DEBUG_FLAG
# include <assert.h>
#else  /* !DEBUG_FLAG */
# define assert(args...)
#endif	/* DEBUG_FLAG */

#undef DEFINE_GORY_STUFF
#include "m30.h"
#include "m62.h"

#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif	/* !UNLIKELY */
#if !defined UNUSED
# define UNUSED(_x)	__attribute__((unused)) _x
#endif	/* !UNUSED */

#define MAT_VERSION	(0x100)

typedef struct mat_s *mat_t;
typedef struct matdat_s *matdat_t;
typedef struct matarr_s *matarr_t;

struct mathdr_s {
	char desc[116];
	uint32_t:32;
	uint32_t:32;
	uint16_t version;
	uint16_t endian;
};

struct mat_s {
	struct mathdr_s hdr;
	void *data[];
};

#define miINT8		(1)
#define miUINT8		(2)
#define miINT16		(3)
#define miUINT16	(4)
#define miINT32		(5)
#define miUINT32	(6)
#define miSINGLE	(7)
#define miDOUBLE	(9)
#define miINT64		(12)
#define miUINT64	(13)
#define miMATRIX	(14)
#define miCOMPRESSED	(15)
#define miUTF8		(16)
#define miUTF16		(17)
#define miUTF32		(18)

#define mxCELL_CLASS	(1)
#define mxSTRUCT_CLASS	(2)
#define mxOBJECT_CLASS	(3)
#define mxCHAR_CLASS	(4)
#define mxSPARSE_CLASS	(5)
#define mxDOUBLE_CLASS	(6)
#define mxSINGLE_CLASS	(7)
#define mxINT8_CLASS	(8)
#define mxUINT8_CLASS	(9)
#define mxINT16_CLASS	(10)
#define mxUINT16_CLASS	(11)
#define mxINT32_CLASS	(12)
#define mxUINT32_CLASS	(13)
#define mxINT64_CLASS	(14)
#define mxUINT64_CLASS	(15)

struct matdat_s {
	uint32_t dty;
	uint32_t nby;
	void *data[];
};

struct matarr_flag_s {
	uint32_t dty/*uint32*/;
	uint32_t flgsz;
	struct {
		/* from LSB to MSB */
		uint32_t class:8;
		uint32_t:1;
		uint32_t logical:1;
		uint32_t global:1;
		uint32_t complex:1;
		uint32_t:4;
		uint32_t:16;
	};
	uint32_t:32;
};

struct matarr_dim_s {
	uint32_t dty/*int32*/;
	uint32_t dimasz/*8, we're 2x2*/;
	uint32_t rows;
	uint32_t cols;
};

struct matarr_nam_s {
	uint32_t dty/*int8*/;
	uint32_t nsz/*max 8 bytes*/;
	char data[8];
};

struct matarr_s {
	struct matdat_s dathdr;
	struct matarr_flag_s flags;
	struct matarr_dim_s dim;
	struct matarr_nam_s nam;
};


/* mmap helpers */
typedef struct mctx_s *mctx_t;

struct mctx_s {
	size_t flen;
	unsigned int pgsz;
	int fd;
	int prot;
	int flags;
};

static size_t
ftruncate_algn(int fd, size_t len)
{
/* like ftruncate, but round up to multiple of FLEN_ALGN */
#define FLEN_ALGN	(16U)
	len += FLEN_ALGN - (len - 1) % FLEN_ALGN - 1;
	ftruncate(fd, len);
	return len;
}

static void
check_trunc(mctx_t ctx, size_t len)
{
	if (len <= ctx->flen) {
		return;
	}
	/* otherwise care about truncation, round up to FLEN_ALGN */
	ctx->flen = ftruncate_algn(ctx->fd, len);
	return;
}

static void*
mmap_any(mctx_t ctx, off_t off, size_t len)
{
	sidx_t ofp = off / ctx->pgsz, ofi = off % ctx->pgsz;
	off_t actoff = ofp * ctx->pgsz;
	size_t actlen = len + ofi;
	char *p;

	/* check for truncation */
	check_trunc(ctx, off + len);
	/* do the actual mapping */
	p = mmap(NULL, actlen, ctx->prot, ctx->flags, ctx->fd, actoff);
	if (LIKELY(p != MAP_FAILED)) {
		return p + ofi;
	}
	return NULL;
}

static void
munmap_any(mctx_t ctx, void *map, off_t off, size_t len)
{
	sidx_t ofi = off % ctx->pgsz;
	munmap((char*)map - ofi, len + ofi);
	return;
}


#define VALS_PER_IDX	(4U)
#define STATIC_VALS	(2U)

static const struct {
	struct mathdr_s hdr;
	struct matarr_s arr;
} __dflt = {
	.hdr = {
		  .desc = "ute print output",
		  .version = MAT_VERSION,
		  .endian = ('M' << 8) + 'I',
	  },
	.arr = {
		 .dathdr = {
			 .dty = miMATRIX,
			 .nby = sizeof(__dflt.arr) - sizeof(__dflt.arr.dathdr),
		 },
		 .flags = {
			  .dty = miUINT32,
			  .flgsz = 8,
#if defined HAVE_ANON_STRUCTS
			  .complex = 0,
			  .global = 0,
			  .logical = 0,
			  .class = mxDOUBLE_CLASS,
#endif	/* HAVE_ANON_STRUCTS */
		  },
		 .dim = {
			  .dty = miINT32,
			  .dimasz = 8,
			  .rows = 0,
			  .cols = 0,
		  },
		 .nam = {
			  .dty = miINT8,
			  .nsz = 7,
			  .data = "ute_out",
		  },
	 },
};

static matarr_t
get_mat_arr_hdr(mctx_t ctx)
{
	mat_t res = mmap_any(ctx, 0, sizeof(__dflt));
	matarr_t arr = (void*)res->data;

	if (arr->dathdr.dty != miMATRIX) {
		memcpy(res, &__dflt, sizeof(__dflt));
#if !defined HAVE_ANON_STRUCTS
		arr->flags.complex = 0;
		arr->flags.global = 0;
		arr->flags.logical = 0;
		arr->flags.class = mxDOUBLE_CLASS;
#endif	/* HAVE_ANON_STRUCTS */
	}
	return arr;
}

static void
put_mat_arr_hdr(mctx_t ctx, matarr_t arr)
{
	char *data = (void*)arr;
	munmap_any(ctx, data - sizeof(struct mathdr_s), 0, sizeof(__dflt));
	return;
}

static void
reshape(double *arr, size_t ncols, size_t old_nr, size_t new_nr)
{
/* stretch the array ARR */
	if (old_nr == 0) {
		/* no reshape needed */
		;
	} else if (new_nr > old_nr) {
		/* A A A B B B C C C -> A A A _ _ _ B B B _ _ _ C C C _ _ _ */
		while (--ncols > 0) {
			double *nu_pos = arr + new_nr * ncols;
			double *ol_pos = arr + old_nr * ncols;
			memmove(nu_pos, ol_pos, old_nr * sizeof(*arr));
		}
	} else if (new_nr < old_nr) {
		/* A A A _ _ B B B _ _ C C C _ _ -> A A A B B B C C C */
		for (size_t i = 1; i < ncols; i++) {
			double *nu_pos = arr + new_nr * i;
			double *ol_pos = arr + old_nr * i;
			memmove(nu_pos, ol_pos, new_nr * sizeof(*arr));
		}
	}
	return;
}

static matdat_t
get_mat_arr_dat(mctx_t ctx, matarr_t arr, size_t nrows, size_t ncols)
{
	size_t nby = nrows * ncols * sizeof(double);
	matdat_t res = mmap_any(ctx, sizeof(__dflt), nby);

	if (res->dty != miDOUBLE) {
		res->dty = miDOUBLE;
		res->nby = nby;
	}
	if (arr->dim.rows < nrows) {
		arr->dim.rows = nrows;
	}
	if (arr->dim.cols < ncols) {
		arr->dim.cols = ncols;
	}
	return res;
}

static void
put_mat_arr_dat(mctx_t ctx, matarr_t arr, matdat_t dat, size_t nr, size_t nc)
{
	size_t origsz = dat->nby;
	size_t newflen;

	reshape((void*)dat->data, nc, arr->dim.rows, nr);
	dat->nby = nc * nr * sizeof(double);
	arr->dim.cols = nc;
	arr->dim.rows = nr;
	arr->dathdr.nby = __dflt.arr.dathdr.nby + dat->nby;
	/* trunc to new size */
	newflen = arr->dathdr.nby + sizeof(__dflt.hdr) + sizeof(*dat);
	ctx->flen = ftruncate_algn(ctx->fd, newflen);
	/* adapt total len, matlab is quite cunty about this */
	arr->dathdr.nby += ctx->flen - newflen;
	/* munmap the old guy */
	munmap_any(ctx, dat, sizeof(__dflt), origsz);
	return;
}


/* aux helpers */
static double
stmp_to_matdt(uint32_t sec, uint16_t msec)
{
	double res;
	res = (double)sec / 86400. + 719529.;
	res += (double)msec / 1000. / 86400.;
	return res;
}

static inline bool
mmapable(int fd)
{
	return fd > STDERR_FILENO;
}

/* code dupe! */
static char*
mat_tmpnam(void)
{
/* return a template for mkstemp(), malloc() it. */
	static const char tmpnam_dflt[] = "/mat.XXXXXX";
	static const char tmpdir_var[] = "TMPDIR";
	static const char tmpdir_pfx[] = "/tmp";
	const char *tmpdir;
	size_t tmpdln;
	char *res;

	if ((tmpdir = getenv(tmpdir_var))) {
		tmpdln = strlen(tmpdir);
	} else {
		tmpdir = tmpdir_pfx;
		tmpdln = sizeof(tmpdir_pfx) - 1;
	}
	res = malloc(tmpdln + sizeof(tmpnam_dflt));
	memcpy(res, tmpdir, tmpdln);
	memcpy(res + tmpdln, tmpnam_dflt, sizeof(tmpnam_dflt));
	return res;
}


/* public demuxer */
static struct mctx_s __gmctx[1];
static matarr_t frag_hdr = NULL;
static matdat_t frag_dat = NULL;
static size_t nrows = 0UL;
static size_t nidxs = 1UL;
static size_t vals_per_idx = VALS_PER_IDX;
static char *tmpfn = NULL;

void
init(pr_ctx_t pctx)
{
	/* set up our context */
	__gmctx->flen = 0;
	__gmctx->pgsz = sysconf(_SC_PAGESIZE);
	if (!mmapable(__gmctx->fd = pctx->outfd)) {
		/* great we need a new file descriptor now
		 * generate a new one, mmapable this time */
		tmpfn = mat_tmpnam();
		__gmctx->fd = mkstemp(tmpfn);
		puts(tmpfn);
	}
	__gmctx->prot = PROT_READ | PROT_WRITE;
	__gmctx->flags = MAP_SHARED;

	/* start out with an array */
	frag_hdr = get_mat_arr_hdr(__gmctx);
	/* get ourselves 256 rows */
	{
		const size_t ini_nr = 256U;
		const size_t ini_nc = STATIC_VALS + vals_per_idx * nidxs;
		frag_dat = get_mat_arr_dat(__gmctx, frag_hdr, ini_nr, ini_nc);
	}
	return;
}

void
fini(pr_ctx_t UNUSED(pctx))
{
	size_t nc = frag_hdr->dim.cols;
	size_t nr = frag_hdr->dim.rows;

	/* reshape to smaller size */
	reshape((void*)frag_dat->data, nc, nr, nrows);
	/* update matarr */
	put_mat_arr_dat(__gmctx, frag_hdr, frag_dat, nrows, nc);
	put_mat_arr_hdr(__gmctx, frag_hdr);

	if (tmpfn) {
		close(__gmctx->fd);
		free(tmpfn);
	}
	return;
}

/* must be here as it uses local vars */
static void
bang5idx(
	size_t row, size_t col, uint16_t idx,
	double ts, double d1, double d2, double d3, double d4)
{
	double *d = (void*)frag_dat->data;
	size_t arows = frag_hdr->dim.rows;

	d[0 * arows + row] = ts;
	d[1 * arows + row] = idx;
	d[(STATIC_VALS + 0 + vals_per_idx * col) * arows + row] = d1;
	d[(STATIC_VALS + 1 + vals_per_idx * col) * arows + row] = d2;
	d[(STATIC_VALS + 2 + vals_per_idx * col) * arows + row] = d3;
	d[(STATIC_VALS + 3 + vals_per_idx * col) * arows + row] = d4;
	return;
}

ssize_t
pr(pr_ctx_t UNUSED(pctx), scom_t st)
{
	uint32_t sec = scom_thdr_sec(st);
	uint16_t msec = scom_thdr_msec(st);
	uint16_t ttf = scom_thdr_ttf(st);
	uint16_t idx = scom_thdr_tblidx(st);
	/* for the time stamp check */
	static uint32_t ol_sec = 0U;
	static uint16_t ol_msec = 0U;
	static double ts = 0.0;
	static uint16_t ol_idx = 0U;
	/* for dimen checks */
	size_t arows = frag_hdr->dim.rows;
	size_t acols = frag_hdr->dim.cols;

	if (STATIC_VALS + vals_per_idx * (ttf & 0xf) > acols) {
		put_mat_arr_dat(__gmctx, frag_hdr, frag_dat, arows, acols);
		acols = STATIC_VALS + vals_per_idx * (nidxs = (ttf & 0xf));
		frag_dat = get_mat_arr_dat(__gmctx, frag_hdr, arows, acols);
		/* reshape not necessary, this is just adding more space
		 * in a column-oriented data format which .mat is */
	}
	if (nrows >= arows) {
		put_mat_arr_dat(__gmctx, frag_hdr, frag_dat, arows, acols);
		frag_dat = get_mat_arr_dat(__gmctx, frag_hdr, arows * 2, acols);
		/* reshape */
		reshape((void*)frag_dat->data, acols, arows, arows * 2);
	}

	/* pre-compute time stamp in matlab format */
	if (sec > ol_sec || (sec == ol_sec && msec > ol_msec)) {
		/* only update if there's news */
		ts = stmp_to_matdt(sec, msec);
		ol_sec = sec;
		ol_msec = msec;
	}

	switch (ttf) {
		/* we only process shnots here */
	case SL1T_TTF_UNK | SCOM_FLAG_LM: {
		const_ssnap_t snp = (const void*)st;
		double bp;
		double ap;
		double bq;
		double aq;

		bp = ffff_m30_d((m30_t)snp->bp);
		ap = ffff_m30_d((m30_t)snp->ap);
		bq = ffff_m30_d((m30_t)snp->bq);
		aq = ffff_m30_d((m30_t)snp->aq);

		bang5idx(nrows, 0, idx, ts, bp, ap, bq, aq);
		break;
	}
	case SL1T_TTF_BID | SCOM_FLAG_LM:
	case SL1T_TTF_ASK | SCOM_FLAG_LM:
	case SL1T_TTF_TRA | SCOM_FLAG_LM:
	case SL1T_TTF_FIX | SCOM_FLAG_LM:
	case SL1T_TTF_STL | SCOM_FLAG_LM:
	case SL1T_TTF_AUC | SCOM_FLAG_LM: {
		const_scdl_t cdl = (const void*)st;
		double o;
		double h;
		double l;
		double c;

		o = ffff_m30_d((m30_t)cdl->o);
		h = ffff_m30_d((m30_t)cdl->h);
		l = ffff_m30_d((m30_t)cdl->l);
		c = ffff_m30_d((m30_t)cdl->c);

		bang5idx(nrows, (ttf & 0xf) - 1, idx, ts, o, h, l, c);
		break;
	}
	default:
		break;
	}

	if (sec > ol_sec || (sec == ol_sec && msec > ol_msec)) {
		/* update */
		ol_sec = sec;
		ol_msec = msec;
		ol_idx = idx;
		nrows++;
	} else if (idx != ol_idx) {
		ol_idx = idx;
		nrows++;
	}
	return 0;
}

/* mat.c ends here */
