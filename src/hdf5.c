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
#if !defined countof
# define countof(x)	(sizeof(x) / sizeof(*x))
#endif	/* !countof */


/* hdf5 helpers */
typedef struct mctx_s *mctx_t;

struct mctx_s {
	/* hdf5 specific */
	hid_t fil;
	hid_t spc;
	hid_t dat;
	hid_t plist;
};


/* aux helpers */
#define VALS_PER_IDX	(4U)
#define STATIC_VALS	(2U)

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
hdf5_tmpnam(void)
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

static void
hdf5_open(mctx_t ctx, const char *fn)
{
	static const char ute_dsnam[] = "ute_out";
	hsize_t dims[] = {STATIC_VALS + VALS_PER_IDX, 1};
	hsize_t maxdims[] = {STATIC_VALS + VALS_PER_IDX, H5S_UNLIMITED};

	/* generate the handle */
        ctx->fil = H5Fcreate(fn, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
	/* generate the data space */
	ctx->spc = H5Screate_simple(2, dims, maxdims);

	/* create a plist */
	ctx->plist = H5Pcreate(H5P_DATASET_CREATE);
	H5Pset_chunk(ctx->plist, 2, dims);
	/* generate the data set */
	ctx->dat = H5Dcreate(
		ctx->fil, ute_dsnam, H5T_IEEE_F64LE,
		ctx->spc, H5P_DEFAULT, ctx->plist, H5P_DEFAULT);
	return;
}

static void
hdf5_close(mctx_t ctx)
{
	(void)H5Pclose(ctx->plist);
	(void)H5Dclose(ctx->dat);
	(void)H5Sclose(ctx->spc);
	(void)H5Fclose(ctx->fil);
	return;
}

static void
bang5idx(
	mctx_t ctx, uint16_t idx, double ts,
	double d1, double d2, double d3, double d4)
{
	const hid_t dat = ctx->dat;
	const hid_t spc = ctx->spc;
	double d[6];

	d[0] = ts;
	d[1] = idx;
	d[2] = d1;
	d[3] = d2;
	d[4] = d3;
	d[5] = d4;

	H5Dwrite(dat, H5T_NATIVE_DOUBLE, H5S_ALL, spc, H5P_DEFAULT, d);
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

		fn = hdf5_tmpnam();
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

	if (my_fn) {
		free(fn);
	}
	return;
}

void
fini(pr_ctx_t UNUSED(pctx))
{
	hdf5_close(__gmctx);
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

#if 0
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
#endif

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

		bang5idx(__gmctx, idx, ts, bp, ap, bq, aq);
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

		bang5idx(__gmctx, idx, ts, o, h, l, c);
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
	} else if (idx != ol_idx) {
		ol_idx = idx;
	}
	return 0;
}

/* hdf5.c ends here */
