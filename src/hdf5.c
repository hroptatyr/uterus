/*** hdf5.c -- hdf5 file muxer
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
#include <sys/stat.h>
#include <sys/mman.h>

/* hdf5 glue */
#include <hdf5.h>

#include "utefile.h"
#include "ute-print.h"

/* so we know about ticks, candles and snapshots */
#include "sl1t.h"
#include "scdl.h"
#include "ssnp.h"

/* we're just as good as rudi, aren't we? */
#if defined DEBUG_FLAG
# include <assert.h>
# define UDEBUG(args...)	fprintf(stderr, args)
#else  /* !DEBUG_FLAG */
# define assert(args...)
# define UDEBUG(args...)
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
#if !defined countof
# define countof(x)	(sizeof(x) / sizeof(*x))
#endif	/* !countof */


/* hdf5 helpers */
typedef struct mctx_s *mctx_t;
typedef struct atom_s *atom_t;
typedef struct cache_s *cache_t;

struct mctx_s {
	/* hdf5 specific */
	hid_t fil;
	hid_t spc;
	hid_t mem;
	hid_t plist;
	hid_t fty;
	hid_t mty;
	cache_t cch;

	size_t nidxs;
	utectx_t u;
};

struct atom_s {
	union {
		time_t ts;
		int64_t pad;
	};
	union {
		double d[4];
		struct {
			double o;
			double h;
			double l;
			double c;
		};
	};
};

static struct {
	const char *nam;
	const off_t off;
} atom_desc[] = {
	{"timestamp (s)", offsetof(struct atom_s, ts)},
	{"open", offsetof(struct atom_s, o)},
	{"high", offsetof(struct atom_s, h)},
	{"low", offsetof(struct atom_s, l)},
	{"close", offsetof(struct atom_s, c)},
};


/* aux helpers */
static inline bool
mmapable(int fd)
{
	struct stat st;
	if (fstat(fd, &st) < 0) {
		return false;
	} else if (st.st_size < 0) {
		return false;
	} else if (!S_ISREG(st.st_mode) || !S_ISLNK(st.st_mode)) {
		return false;
	}
	return true;
}

/* code dupe! */
static char*
__tmpnam(void)
{
/* return a template for mkstemp(), malloc() it. */
	static const char tmpnam_dflt[] = "/hdf5.XXXXXX";
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

static void*
__resize(void *p, size_t nol, size_t nnu, size_t blsz)
{
	const size_t ol = nol * blsz;
	const size_t nu = nnu * blsz;
	void *res = realloc(p, nu);
	if (nu > ol) {
		memset((char*)res + ol, 0, nu - ol);
	}
	return res;
}


#define CHUNK_INC	(1024)

struct cache_s {
	size_t nbang;
	hid_t dat;
	struct atom_s vals[CHUNK_INC];
};

static hid_t
make_dat(mctx_t ctx, const char *nam)
{
	const hid_t fil = ctx->fil;
	const hid_t spc = ctx->spc;
	const hid_t ds_ty = ctx->fty;
	const hid_t ln_crea = H5P_DEFAULT;
	const hid_t ds_crea = ctx->plist;
	const hid_t ds_acc = H5P_DEFAULT;

	return H5Dcreate(fil, nam, ds_ty, spc, ln_crea, ds_crea, ds_acc);
}

static hid_t
make_fil_type()
{
	hid_t res = H5Tcreate(H5T_COMPOUND, sizeof(struct atom_s));

	H5Tinsert(res, atom_desc[0].nam, atom_desc[0].off, H5T_UNIX_D64LE);
	H5Tinsert(res, atom_desc[1].nam, atom_desc[1].off, H5T_IEEE_F64LE);
	H5Tinsert(res, atom_desc[2].nam, atom_desc[2].off, H5T_IEEE_F64LE);
	H5Tinsert(res, atom_desc[3].nam, atom_desc[3].off, H5T_IEEE_F64LE);
	H5Tinsert(res, atom_desc[4].nam, atom_desc[4].off, H5T_IEEE_F64LE);
	return res;
}

static hid_t
make_mem_type()
{
	hid_t res = H5Tcreate(H5T_COMPOUND, sizeof(struct atom_s));

	H5Tinsert(res, atom_desc[0].nam, atom_desc[0].off, H5T_UNIX_D64LE);
	H5Tinsert(res, atom_desc[1].nam, atom_desc[1].off, H5T_NATIVE_DOUBLE);
	H5Tinsert(res, atom_desc[2].nam, atom_desc[2].off, H5T_NATIVE_DOUBLE);
	H5Tinsert(res, atom_desc[3].nam, atom_desc[3].off, H5T_NATIVE_DOUBLE);
	H5Tinsert(res, atom_desc[4].nam, atom_desc[4].off, H5T_NATIVE_DOUBLE);
	return res;
}

static hid_t
get_dat(mctx_t ctx, uint16_t idx)
{
	if (ctx->cch[idx].dat == 0) {
		/* singleton */
		const char *dnam = ute_idx2sym(ctx->u, idx);
		ctx->cch[idx].dat = make_dat(ctx, dnam);
	}
	return ctx->cch[idx].dat;
}

static void
hdf5_open(mctx_t ctx, const char *fn)
{
	hsize_t dims[] = {1, 1, 0};
	hsize_t maxdims[] = {1, 1, H5S_UNLIMITED};

	/* generate the handle */
        ctx->fil = H5Fcreate(fn, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
	/* generate the data space */
	ctx->spc = H5Screate_simple(countof(dims), dims, maxdims);

	/* create a plist */
	dims[countof(dims) - 1] = CHUNK_INC;
	ctx->plist = H5Pcreate(H5P_DATASET_CREATE);
	H5Pset_chunk(ctx->plist, countof(dims), dims);
	/* just one more for slabbing later on */
	ctx->mem = H5Screate_simple(countof(dims), dims, dims);

	/* generate the types we're one about */
	ctx->fty = make_fil_type();
	ctx->mty = make_mem_type();
	return;
}

static void
hdf5_close(mctx_t ctx)
{
	(void)H5Tclose(ctx->fty);
	(void)H5Tclose(ctx->mty);
	(void)H5Pclose(ctx->plist);
	(void)H5Sclose(ctx->mem);
	(void)H5Sclose(ctx->spc);
	(void)H5Fclose(ctx->fil);
	return;
}

static void
cache_init(mctx_t ctx)
{
	static const char ute_dsnam[] = "/default";

	ctx->cch = malloc(sizeof(*ctx->cch));
	ctx->cch->nbang = 0UL;
	/* generate the catch-all data set */
	ctx->cch->dat = make_dat(ctx, ute_dsnam);
	ctx->nidxs = 0UL;
	return;
}

static void
cache_flush(mctx_t ctx, size_t idx, const cache_t cch)
{
	const hid_t dat = get_dat(ctx, idx);
	const hid_t mem = ctx->mem;
	const hsize_t nbang = cch->nbang;
	hsize_t ol_dims[] = {0, 0, 0};
	hsize_t nu_dims[] = {0, 0, 0};
	hsize_t sta[] = {0, 0, 0};
	hsize_t cnt[] = {1, 1, -1};
	hid_t spc;

	/* get current dimensions */
	spc = H5Dget_space(dat);
	H5Sget_simple_extent_dims(spc, ol_dims, NULL);

	/* nbang more rows, make sure we're at least as wide as we say */
	nu_dims[0] = ol_dims[0];
	nu_dims[1] = ol_dims[1];
	nu_dims[2] = ol_dims[2] + nbang;
	H5Dset_extent(dat, nu_dims);
	spc = H5Dget_space(dat);

	/* select a hyperslab in the mem space */
	cnt[2] = nbang;
	H5Sselect_hyperslab(mem, H5S_SELECT_SET, sta, NULL, cnt, NULL);

	/* select a hyperslab in the file space */
	sta[2] = ol_dims[2];
	H5Sselect_hyperslab(spc, H5S_SELECT_SET, sta, NULL, cnt, NULL);

	/* final flush */
	H5Dwrite(dat, ctx->mty, mem, spc, H5P_DEFAULT, cch->vals);
	cch->nbang = 0UL;
	return;
}

static void
cache_fini(mctx_t ctx)
{
	for (size_t i = 0; i <= ctx->nidxs; i++) {
		if (ctx->cch[i].nbang) {
			UDEBUG("draining %zu: %zu\n", i, ctx->cch[i].nbang);
			cache_flush(ctx, i, ctx->cch + i);
		}
		if (ctx->cch[i].dat) {
			(void)H5Dclose(ctx->cch[i].dat);
			ctx->cch[i].dat = 0;
		}
	}
	free(ctx->cch);
	return;
}

static cache_t
get_cch(mctx_t ctx, uint16_t idx)
{
	/* check for resize */
	if (idx > ctx->nidxs) {
		const size_t ol = ctx->nidxs + 1;
		const size_t nu = idx + 1;
		ctx->cch = __resize(ctx->cch, ol, nu, sizeof(*ctx->cch));
		ctx->nidxs = idx;
	}
	/* for the side-effect */
	(void)get_dat(ctx, idx);
	return ctx->cch + idx;
}

static void
bang5idx(
	mctx_t ctx, uint16_t idx, time_t ts, uint16_t UNUSED(msec),
	double d1, double d2, double d3, double d4)
{
	const cache_t cch = get_cch(ctx, idx);
	const size_t nbang = cch->nbang;
	atom_t vals = cch->vals;

	vals[nbang].ts = ts;
	vals[nbang].o = d1;
	vals[nbang].h = d2;
	vals[nbang].l = d3;
	vals[nbang].c = d4;
	/* check if we need to flush the whole shebang */
	if (++cch->nbang < countof(cch->vals)) {
		/* yay, we're safe */
		return;
	}
	/* oh oh oh */
	cache_flush(ctx, idx, cch);
	return;
}


/* public demuxer */
static struct mctx_s __gmctx[1];

void
init(pr_ctx_t pctx)
{
	bool my_fn;
	char *fn;

	/* set up our context */
	if ((my_fn = !mmapable(pctx->outfd))) {
		/* great we need a new file descriptor now
		 * generate a new one, mmapable this time */
		int fd;

		fn = __tmpnam();
		fd = mkstemp(fn);
		puts(fn);
		close(fd);
	} else {
		/* file name from descriptor, linuxism! */
		static char buf[1024];
		ssize_t bsz;
		snprintf(buf, sizeof(buf), "/proc/self/fd/%d", pctx->outfd);
		if ((bsz = readlink(buf, buf, sizeof(buf) - 1)) < 0) {
			fn = NULL;
		} else {
			(fn = buf)[bsz] = '\0';
		}
		puts(fn);
	}

	/* let hdf deal with this */
	hdf5_open(__gmctx, fn);
	/* also get the cache ready */
	cache_init(__gmctx);

	if (my_fn) {
		free(fn);
	}
	return;
}

void
fini(pr_ctx_t UNUSED(pctx))
{
	cache_fini(__gmctx);
	hdf5_close(__gmctx);
	return;
}

ssize_t
pr(pr_ctx_t pctx, scom_t st)
{
	uint32_t sec = scom_thdr_sec(st);
	uint16_t msec = scom_thdr_msec(st);
	uint16_t ttf = scom_thdr_ttf(st);
	uint16_t idx = scom_thdr_tblidx(st);

	/* bang new ute context */
	__gmctx->u = pctx->uctx;

	switch (ttf) {
		/* we only process shnots here */
	case SSNP_FLAVOUR: {
		const_ssnp_t snp = (const void*)st;
		double bp;
		double ap;
		double bq;
		double aq;

		bp = ffff_m30_d((m30_t)snp->bp);
		ap = ffff_m30_d((m30_t)snp->ap);
		bq = ffff_m30_d((m30_t)snp->bq);
		aq = ffff_m30_d((m30_t)snp->aq);

		bang5idx(__gmctx, idx, sec, msec, bp, ap, bq, aq);
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

		bang5idx(__gmctx, idx, sec, msec, o, h, l, c);
		break;
	}
	default:
		break;
	}
	return 0;
}

/* hdf5.c ends here */
