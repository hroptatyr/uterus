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
#include <sys/mman.h>

#include "utefile.h"
#include "ute-print.h"

/* so we know about ticks, candles and snapshots */
#include "sl1t.h"
#include "scdl.h"

#if defined USE_DEBUGGING_ASSERTIONS
# include <assert.h>
#endif	/* USE_DEBUGGING_ASSERTIONS */
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

static void
check_trunc(mctx_t ctx, size_t len)
{
#define FLEN_ALGN	(16U)
	if (len <= ctx->flen) {
		return;
	}
	/* otherwise care about truncation, round up to FLEN_ALGN */
	len += FLEN_ALGN - (len - 1) % FLEN_ALGN - 1;
	ftruncate(ctx->fd, ctx->flen = len);
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
			  .complex = 0,
			  .global = 0,
			  .logical = 0,
			  .class = mxDOUBLE_CLASS,
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
put_mat_arr_dat(mctx_t ctx, matdat_t dat)
{
	munmap_any(ctx, dat, sizeof(__dflt), dat->nby);
	return;
}

static void
upd_mat_arr(mctx_t UNUSED(ctx), matarr_t arr, matdat_t dat)
{
	arr->dathdr.nby = __dflt.arr.dathdr.nby + dat->nby;
	return;
}


/* public demuxer */
static struct mctx_s __gmctx[1];
static matarr_t frag_hdr = NULL;
static matdat_t frag_dat = NULL;
static size_t nrows = 0UL;

void
init(pr_ctx_t pctx)
{
	/* set up our context */
	__gmctx->flen = 0;
	__gmctx->pgsz = sysconf(_SC_PAGESIZE);
	__gmctx->fd = pctx->outfd;
	__gmctx->prot = PROT_READ | PROT_WRITE;
	__gmctx->flags = MAP_SHARED;

	/* start out with an array */
	frag_hdr = get_mat_arr_hdr(__gmctx);
	return;
}

void
fini(pr_ctx_t UNUSED(pctx))
{
	/* update matarr */
	if (frag_dat) {
		upd_mat_arr(__gmctx, frag_hdr, frag_dat);
		put_mat_arr_dat(__gmctx, frag_dat);
	}
	put_mat_arr_hdr(__gmctx, frag_hdr);
	return;
}

ssize_t
pr(pr_ctx_t UNUSED(pctx), scom_t st)
{
	uint32_t sec = scom_thdr_sec(st);
	uint16_t msec = scom_thdr_msec(st);
	uint16_t ttf = scom_thdr_ttf(st);

	if (nrows >= frag_hdr->dim.rows) {
		frag_dat = get_mat_arr_dat(__gmctx, frag_hdr, nrows + 256, 5);
	}

	switch (ttf) {
		double ts;
		double bp;
		double ap;
		double bq;
		double aq;

		/* we only process shnots here */
	case SL1T_TTF_UNK | SCOM_FLAG_LM: {
		const_ssnap_t snp = (const void*)st;
		ts = (double)sec / 86400. + 719529.;
		ts += (double)msec / 1000. / 86400.;

		bp = ffff_m30_d((m30_t)snp->bp);
		ap = ffff_m30_d((m30_t)snp->ap);
		bq = ffff_m30_d((m30_t)snp->bq);
		aq = ffff_m30_d((m30_t)snp->aq);

		{
			double *d = (void*)frag_dat->data;
			size_t arows = frag_hdr->dim.rows;

			d[0 * arows + nrows] = ts;
			d[1 * arows + nrows] = bp;
			d[2 * arows + nrows] = ap;
			d[3 * arows + nrows] = bq;
			d[4 * arows + nrows] = aq;
			nrows++;
		}
		break;
	}
	default:
		break;
	}
	return 0;
}

/* mat.c ends here */
