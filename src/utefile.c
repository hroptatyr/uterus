/*** utefile.c -- high level interface to ute files (r/w)
 *
 * Copyright (C) 2010 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <sebastian.freundt@ga-group.nl>
 *
 * This file is part of sushi/uterus.
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

#define UTEFILE_C
#include <stddef.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "utefile-private.h"
#include "utefile.h"
#include "utehdr.h"
#include "utetpc.h"
#include "mem.h"

/* only tick size we support atm */
#include "sl1t.h"

#define countof(x)	(sizeof(x) / sizeof(*x))

#define SMALLEST_LVTD	(0)

static const char ute_vers[][8] = {
	"UTE+v0.0",
	"UTE+v0.1",
	"UTE+v0.2",
};


/* aux */
static char*
mmap_any(int fd, int prot, int flags, off_t off, size_t len)
{
	int pgsz = sysconf(_SC_PAGESIZE);
	sidx_t ofp = off / pgsz, ofi = off % pgsz;
	char *p = mmap(NULL, len + ofi, prot, flags, fd, ofp * pgsz);
	return LIKELY(p != MAP_FAILED) ? p + ofi : NULL;
}

static void
munmap_any(char *map, off_t off, size_t len)
{
	int pgsz = sysconf(_SC_PAGESIZE);
	sidx_t ofi = off % pgsz;
	munmap(map - ofi, len + ofi);
	return;
}

static bool
__fwr_trunc(int fd, size_t sz)
{
	if ((fd < 0) || (ftruncate(fd, sz) < 0)) {
		return false;
	}
	return true;
}

static inline int
__pflags(utectx_t ctx)
{
	return PROT_READ | ((ctx->oflags & UO_RDWR) ? PROT_WRITE : 0);
}

static bool
ute_trunc(utectx_t ctx, size_t sz)
{
	if (!__fwr_trunc(ctx->fd, sz)) {
		return false;
	}
	ctx->fsz = sz;
	return true;
}

static bool
ute_extend(utectx_t ctx, ssize_t sz)
{
/* extend the ute file by SZ bytes */
	size_t tot = sz + ctx->fsz;
	if (!__fwr_trunc(ctx->fd, tot)) {
		return false;
	}
	ctx->fsz = tot;
	return true;
}

/* header caching, also probing */
static int
cache_hdr(utectx_t ctx)
{
	/* we just use max size here */
	const size_t sz = sizeof(struct utehdr2_s);
	struct utehdr2_s *res;
	int pflags = __pflags(ctx);

	/* just map the first sizeof(struct bla) bytes */
	res = mmap(NULL, sz, pflags, MAP_SHARED, ctx->fd, 0);
	if (UNLIKELY(res == MAP_FAILED)) {
		/* it failed, it failed, who cares why */
		goto err_out;
	}
	/* assign the header ... */
	ctx->slut_sz = (ctx->hdrp = res)->slut_sz;
	/* ... and take a probe, if it's not for creation */
	if (ctx->oflags & (UO_CREAT | UO_TRUNC)) {
		/* don't bother checking the header */
		return 0;
	} else if (!memcmp(res->magic, "UTE+", sizeof(res->magic))) {
		/* perfect */
		return 0;
	}
	/* otherwise something's fucked */
err_out:
	ctx->hdrp = NULL;
	ctx->slut_sz = 0;
	return -1;
}

static void
close_hdr(utectx_t ctx)
{
	/* munmap the header */
	if (ctx->hdrp != NULL) {
		munmap((void*)ctx->hdrp, sizeof(*ctx->hdrp));
	}
	ctx->hdrp = NULL;
	return;
}

static void
creat_hdr(utectx_t ctx)
{
	size_t sz = sizeof(struct utehdr2_s);

	/* trunc to sz */
	ute_trunc(ctx, sz);
	/* cache the header */
	(void)cache_hdr(ctx);
	/* set standard header payload offset, just to be sure it's sane */
	if (LIKELY(ctx->hdrp != NULL)) {
		const char *ver = ute_vers[UTE_VERSION_02];
		const size_t vsz = sizeof(ute_vers[UTE_VERSION_02]);
		memset((void*)ctx->hdrp, 0, sz);
		memcpy((void*)ctx->hdrp, ver, vsz);
	}
	/* file creation means new slut */
	ctx->slut_sz = 0;
	return;
}

/* seek reset */
void
flush_seek(uteseek_t sk)
{
	if (sk->mpsz > 0) {
		/* seek points to something => munmap first */
		munmap(sk->data, sk->mpsz);
	}
	sk->idx = -1;
	sk->mpsz = 0;
	sk->data = NULL;
	sk->tsz = 0;
	sk->page = -1U;
	return;
}

void
seek_page(uteseek_t sk, utectx_t ctx, uint32_t pg)
{
	size_t psz = page_size(ctx, pg);
	size_t off = page_offset(ctx, pg);
	void *tmp;
	int pflags = __pflags(ctx);

	/* trivial checks */
	if (UNLIKELY(off >= ctx->fsz)) {
		return;
	}
	/* create a new seek */
	tmp = mmap(NULL, psz, pflags, MAP_SHARED, ctx->fd, off);
	if (UNLIKELY(tmp == MAP_FAILED)) {
		return;
	}
	sk->data = tmp;
	sk->idx = 0;
	sk->mpsz = psz;
	sk->tsz = sizeof(struct sl1t_s);
	sk->page = pg;
	return;
}

static void
seek_tmppage(uteseek_t sk, utectx_t ctx, uint32_t pg)
{
	const int MAP_MEM = MAP_SHARED | MAP_ANONYMOUS;
	const int PROT_MEM = PROT_READ | PROT_WRITE;
	size_t psz = page_size(ctx, pg);
	void *tmp;

	/* create a new seek */
	tmp = mmap(NULL, psz, PROT_MEM, MAP_MEM, 0, 0);
	if (UNLIKELY(tmp == MAP_FAILED)) {
		return;
	}
	sk->data = tmp;
	sk->idx = 0;
	sk->mpsz = psz;
	sk->tsz = sizeof(struct sl1t_s);
	sk->page = pg;
	return;
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
	ctx->seek->idx = o;
	return;
}

/* seek to */
scom_t
ute_seek(utectx_t ctx, sidx_t i)
{
	uint32_t o;

	/* wishful thinking */
	if (UNLIKELY(index_past_eof_p(ctx, i))) {
		sidx_t new_i = index_to_tpc_index(ctx, i);
		return tpc_get_scom(ctx->tpc, new_i);
	} else if (UNLIKELY(!index_in_seek_page_p(ctx, i))) {
		reseek(ctx, i);
	}
	o = offset_of_index(ctx, i);
	return (scom_t)((char*)ctx->seek->data + o * ctx->seek->tsz);
}

static void
store_lvtd(utectx_t ctx)
{
	if (tpc_size(ctx->tpc) > 0 && ctx->tpc->last > ctx->lvtd) {
		ctx->lvtd = ctx->tpc->last;
	}
	ctx->tpc->lvtd = ctx->lvtd;
	return;
}

static void
store_slut(utectx_t ctx)
{
	struct utehdr2_s *h = (void*)ctx->hdrp;
	h->slut_sz = ctx->slut_sz;
	h->slut_nsyms = (uint16_t)ctx->slut->nsyms;
	return;
}

#define PROT_FLUSH	(PROT_READ | PROT_WRITE)
#define MAP_FLUSH	(MAP_SHARED)

static void
flush_tpc(utectx_t ctx)
{
	void *p;
	size_t sz = tpc_size(ctx->tpc);
	sidx_t off = ctx->fsz; 

	/* extend to take SZ additional bytes */
	if (ctx->oflags == UO_RDONLY || !ute_extend(ctx, sz)) {
		return;
	}
	p = mmap(NULL, sz, PROT_FLUSH, MAP_FLUSH, ctx->fd, off);
	if (p == MAP_FAILED) {
		return;
	}
	memcpy(p, ctx->tpc->tp, sz);
	munmap(p, sz);

	/* store the largest-value-to-date */
	store_lvtd(ctx);

	/* munmap the tpc? */
	clear_tpc(ctx->tpc);
	return;
}

static void
flush_slut(utectx_t ctx)
{
	char *p;
	void *stbl = NULL;
	size_t stsz = 0;
	sidx_t off = ctx->fsz;

	/* dont try at all in read-only mode */
	if (UNLIKELY(ctx->oflags == UO_RDONLY)) {
		return;
	}

	/* first of all serialise into memory */
	slut_seria(ctx->slut, &stbl, &stsz);

	if (UNLIKELY(stbl == NULL)) {
		return;
	} else if (UNLIKELY(stsz == 0)) {
		goto out;
	}
	/* extend to take STSZ additional bytes */
	if (!ute_extend(ctx, stsz)) {
		goto out;
	}
	/* align to multiples of page size */
	if ((p = mmap_any(ctx->fd, PROT_FLUSH, MAP_FLUSH, off, stsz)) == NULL) {
		goto out;
	}
	memcpy(p, stbl, stsz);
	munmap_any(p, off, stsz);

	/* store the size of the serialised slut */
	ctx->slut_sz = stsz;
	store_slut(ctx);

out:
	free(stbl);
	return;
}


/* tpc glue */
static inline void
unset_tpc_needmrg(utetpc_t tpc)
{
	tpc->flags &= ~TPC_FL_NEEDMRG;
	return;
}

#define DATA(p, i)	((void*)(((char*)(p)) + (i)))
#define DATD(p, q)	((size_t)(((const char*)(p)) - ((const char*)(q))))
#define ALGN(t, o)	((o / sizeof(t)) * sizeof(t))

static void*
algn_tick(void *tp, void *botp)
{
	const size_t probsz = sizeof(struct sl1t_s);
	void *prob;

	/* check tp - probsz */
	if (UNLIKELY((prob = DATA(tp, -probsz)) < botp)) {
		return tp;
	} else if (scom_thdr_ttf(prob) & SCOM_FLAG_LM) {
		return prob;
	}
	return tp;
}

static void*
find_scidx(uteseek_t sk, scidx_t key)
{
/* find a pointer into SK's data with the first tick that's bigger than KEY
 * or NULL otherwise, use a binary search */
	const size_t probsz = sizeof(struct sl1t_s);
	void *eosp = DATA(sk->data, sk->mpsz);
	void *sp;

	/* try the tail first */
	sp = algn_tick(DATA(sk->data, sk->mpsz - probsz), sk->data);
	if (make_scidx(sp).u > key.u) {
		goto binsrch;
	}
	return NULL;
binsrch:
	/* try the middle */
	for (void *bosp = sk->data; bosp < eosp; ) {
		size_t off = ALGN(struct sl1t_s, DATD(eosp, bosp) / 2);

		sp = algn_tick(DATA(bosp, off), sk->data);
		if (make_scidx(sp).u > key.u) {
			/* left half */
			eosp = sp;
		} else if (sp == bosp) {
			sp = eosp;
			break;
		} else {
			/* right half */
			bosp = sp;
		}
	}
	/* must be the offending tick */
	return sp;
}

static void
merge_tpc(utectx_t ctx, utetpc_t tpc)
{
/* assume all tick pages in CTX are sorted except for TPC
 * now merge-sort those with TPC */
	size_t npg = ute_npages(ctx);
	struct uteseek_s sk[2];
	/* offending ticks */
	void *sp;
	/* page indicator */
	size_t pg;
	/* index keys */
	scidx_t ti;

	/* set up seeker through tpc */
	ti = make_scidx(AS_SCOM(tpc->tp));

	for (pg = 0; pg < npg; pg++) {
		/* seek to the i-th page */
		seek_page(sk, ctx, pg);
		/* use bin-srch to find an offending tick */
		if ((sp = find_scidx(sk, ti))) {
			goto mrg;
		}
		/* no need for this page */
		flush_seek(sk);
	}
	return;

mrg:
	/* update seek */
	sk->idx = DATD(sp, sk->data) / sk->tsz;
	/* get a temporary tpc page (where the merges go) */
	seek_tmppage(sk + 1, ctx, pg);
	sk[1].idx = sk->idx;
	memcpy(sk[1].data, sk[0].data, sk->idx * sk->tsz);

	do {
		merge_2tpc(sk + 1, sk + 0, tpc);
		/* tpc might have become unsorted, sort it now */
		tpc_sort(tpc);
		/* copy the tmp page back */
		memcpy(sk[0].data, sk[1].data, sk[1].idx * sk[1].tsz);
		/* after this, sk[0] will be empty and we can proceed
		 * with another seek/page */
		flush_seek(sk);

		/* next page now */
		if (++pg < npg) {
			seek_page(sk, ctx, pg);
		} else {
			break;
		}
	} while (1);

	/* unset the need merge flag */
	unset_tpc_needmrg(tpc);	
	return;
}

static void
load_last_tpc(utectx_t ctx)
{
/* take the last page in CTX and make a tpc from it, trunc the file
 * accordingly, this is in a way a reverse flush_tpc() */
	size_t lpg = ute_npages(ctx);
	struct uteseek_s sk[1];

	if (UNLIKELY(lpg == 0)) {
		return;
	}

	/* seek to the last page */
	seek_page(sk, ctx, lpg - 1);
	/* create the tpc space */
	if (!tpc_active_p(ctx->tpc)) {
		make_tpc(ctx->tpc, UTE_BLKSZ(ctx), sizeof(struct sl1t_s));
	}
	/* copy the last page */
	memcpy(ctx->tpc->tp, sk->data, sk->mpsz);
	/* ... and set the new length */
	ctx->tpc->tidx = sk->mpsz;
	/* now munmap the seek */
	flush_seek(sk);
	/* also set the last and lvtd values */
	/* store the largest-value-to-date */
	store_lvtd(ctx);

	/* real shrinking was to dangerous without C-c handler,
	 * make fsz a multiple of page size */
	ctx->fsz -= tpc_size(ctx->tpc) + ctx->slut_sz;
	return;
}


static char*
mmap_slut(utectx_t ctx)
{
	size_t off = ctx->fsz - ctx->slut_sz;
	int pflags = __pflags(ctx);
	return mmap_any(ctx->fd, pflags, MAP_FLUSH, off, ctx->slut_sz);
}

static void
munmap_slut(utectx_t ctx, char *map)
{
	size_t off = ctx->fsz - ctx->slut_sz;
	munmap_any(map, off, ctx->slut_sz);
	return;
}

static void
load_slut(utectx_t ctx)
{
/* take the stuff past the last page in CTX and make a slut from it */
	char *slut;

	if (UNLIKELY(ctx->fsz == 0)) {
		return;
	}

	/* otherwise leap to behind the last tick and
	 * deserialise the look-up table */
	if ((slut = mmap_slut(ctx)) != NULL) {
		slut_deser(ctx->slut, slut, ctx->slut_sz);
		munmap_slut(ctx, slut);
	}

	/* real shrink is too dangerous, just adapt fsz instead */
	ctx->fsz -= ctx->slut_sz, ctx->slut_sz = 0;
	return;
}

static void
ute_init(utectx_t ctx)
{
	ctx->pgsz = (size_t)sysconf(_SC_PAGESIZE);
	flush_seek(ctx->seek);
	/* yikes, this is a bit confusing, we use the free here to
	 * initialise the tpc */
	free_tpc(ctx->tpc);
	return;
}

static void
ute_fini(utectx_t ctx)
{
	flush_seek(ctx->seek);
	free_tpc(ctx->tpc);
	free(ctx->fname);
	return;
}

static char*
ute_tmpnam(void)
{
/* return a template for mkstemp(), malloc() it. */
	static const char tmpnam_dflt[] = "/ute.XXXXXX";
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

static utectx_t
make_utectx(const char *fn, int fd, int oflags)
{
	struct stat st;
	utectx_t res;

	if (UNLIKELY(fstat(fd, &st) < 0)) {
		/* we don't want to know what's wrong here */
		return NULL;
	} else if (UNLIKELY(st.st_size <= 0 && !(oflags & UO_CREAT))) {
		/* user didn't request creation, so fuck off here */
		return NULL;
	}

	/* start creating the result */
	res = calloc(1, sizeof(*res));

	/* fill in some fields */
	res->fd = fd;
	res->fsz = st.st_size;
	/* flags used to open this file */
	res->oflags = (uint16_t)oflags;

	if ((oflags & UO_TRUNC) ||
	    (st.st_size == 0 && (oflags & UO_CREAT))) {
		/* user requested truncation, or creation */
		creat_hdr(res);
	} else if (res->fsz > 0 && cache_hdr(res) < 0) {
		/* cache_hdr() does probing for us, being < 0 means
		 * it's probably fucked */
		free(res);
		return NULL;
	}
	/* to avoid more complicated free'ing strdup strings here */
	if (!(oflags & O_EXCL)) {
		/* save a copy of the file name */
		res->fname = strdup(fn);
	} else {
		/* same but the name was already malloc'd */
		res->fname = snodup(fn);
	}

	/* initialise the rest */
	ute_init(res);
	/* initialise the tpc session */
	init_tpc();
	/* initialise the slut */
	init_slut();

	if ((oflags & UO_TRUNC) || res->fsz == 0) {
		/* set the largest-value to-date, which is pretty small */
		res->lvtd = SMALLEST_LVTD;
		make_slut(res->slut);
	} else {
		/* load the slut, must be first, as we need to shrink
		 * the file accordingly */
		load_slut(res);
		/* load the last page as tpc */
		load_last_tpc(res);
	}
	return res;
}


/* primitives to kick shit off */
utectx_t
ute_open(const char *path, int oflags)
{
	int resfd;

	/* massage the flags */
	if (UNLIKELY((oflags & UO_WRONLY))) {
		/* stupid nutters, we allow read/write, not write-only */
		oflags = (oflags & ~UO_WRONLY) | UO_RDWR;
	}
	/* we need to open the file RDWR at the moment, various
	 * mmap()s use PROT_WRITE */
	if (oflags != UO_RDONLY) {
		oflags |= UO_RDWR;
	}
	/* try and open the file first */
	if ((resfd = open(path, oflags, 0644)) < 0) {
		/* ooooh, leave with a big bang */
		return NULL;
	}
	return make_utectx(path, resfd, oflags);
}

utectx_t
ute_mktemp(int oflags)
{
	char *tmpnam;
	int resfd;

	/* get a tmp name template for mkstemp() */
	tmpnam = ute_tmpnam();
	/* now open it ... */
	resfd = mkstemp(tmpnam);
	/* wipe and check for anon */
	if ((oflags &= UO_ANON)) {
		/* ... and unlink it if ANON is set */
		unlink(tmpnam);
	}
	oflags |= UO_CREAT;
	oflags |= UO_TRUNC;
	oflags |= UO_RDWR;
	oflags |= O_EXCL;
	return make_utectx(tmpnam, resfd, oflags);
}

static void
ute_prep_sort(utectx_t ctx)
{
	/* delete the file */
	unlink(ctx->fname);
	return;
}

void
ute_close(utectx_t ctx)
{
	/* first make sure we write the stuff */
	ute_flush(ctx);
	if (!ute_sorted_p(ctx)) {
		ute_prep_sort(ctx);
		ute_sort(ctx);
	}
	/* serialse the slut */
	flush_slut(ctx);

	/* finish our tpc session */
	fini_tpc();
	/* finish our slut session */
	fini_slut();
	/* now proceed to closing and finalising */
	close_hdr(ctx);
	ute_fini(ctx);
	close(ctx->fd);
	return;
}

void
ute_flush(utectx_t ctx)
{
	if (!tpc_active_p(ctx->tpc)) {
		return;
	}
	/* also sort and diskify the currently active tpc */
	if (!tpc_sorted_p(ctx->tpc)) {
		tpc_sort(ctx->tpc);
	}
	if (tpc_needmrg_p(ctx->tpc)) {
		/* special case when the page cache has detected
		 * a major violation */
		merge_tpc(ctx, ctx->tpc);
	}
	flush_tpc(ctx);
	return;
}



/* accessor */
void
ute_add_tick(utectx_t ctx, scom_t t)
{
/* the big question here is if we want to allow arbitrary ticks as in
 * can T be of type scdl too? */
	if (!tpc_active_p(ctx->tpc)) {
		make_tpc(ctx->tpc, UTE_BLKSZ(ctx), sizeof(struct sl1t_s));
	} else if (tpc_full_p(ctx->tpc)) {
		/* oh current tpc is full, flush and start over */
		ute_flush(ctx);
	}
	/* we sort the tick question for now by passing on the size of T */
	tpc_add_tick(ctx->tpc, t, scom_thdr_size(t));
	return;
}

size_t
ute_nticks(utectx_t ctx)
{
/* for the moment just use the file size and number of pages
 * plus whats in the tpc */
	size_t aux_sz = sizeof(struct utehdr2_s) + ctx->slut_sz;
	size_t nticks = (ctx->fsz - aux_sz) / sizeof(struct sl1t_s);
	/* if there are non-flushed ticks, consider them */
	if (tpc_active_p(ctx->tpc)) {
		nticks += tpc_nticks(ctx->tpc);
	}
	return nticks;
}

size_t
ute_nsyms(utectx_t ctx)
{
/* return the number of symbols tracked in the ute file */
	size_t nsyms = ctx->hdrp->slut_nsyms;
	if (UNLIKELY(nsyms == 0)) {
		/* try the slut itself */
		nsyms = ctx->slut->nsyms;
	}
	return nsyms;
}

/* quick note one the actual cow stuff in ute:
 * - we keep ticks in pages, each 1024 sys pages wide.
 * - each tick page is ordered
 * - in the final file tick pages will be written so
 *   theyre ordered altogether
 * - there are scribble pages */


/* accessors */
size_t
ute_tick(utectx_t ctx, scom_t *tgt, sidx_t i)
{
	scom_t p = ute_seek(ctx, i);
	if (LIKELY(p != NULL)) {
		*tgt = p;
		return ctx->seek->tsz;
	}
	return 0;
}

size_t
ute_tick2(utectx_t ctx, void *tgt, size_t UNUSED(tsz), sidx_t i)
{
	scom_t p = ute_seek(ctx, i);
	if (LIKELY(p != NULL)) {
		memcpy(tgt, p, ctx->seek->tsz);
		return ctx->seek->tsz;
	}
	return 0;
}

/* slut accessors */
uint16_t
ute_sym2idx(utectx_t ctx, const char *sym)
{
	if (UNLIKELY(sym == NULL)) {
		abort();
	}
	return slut_sym2idx(ctx->slut, sym);
}

const char*
ute_idx2sym(utectx_t ctx, uint16_t idx)
{
	return slut_idx2sym(ctx->slut, idx);
}

const char*
ute_fn(utectx_t ctx)
{
	return ctx->fname;
}

ute_ver_t
ute_version(utectx_t ctx)
{
/* return the number of symbols tracked in the ute file */
	const size_t vsz = sizeof(ute_vers[0]);
	for (size_t i = countof(ute_vers); --i > 0; ) {
		const char *ver = ute_vers[i];
		if (memcmp(ctx->hdrp->magic, ver, vsz) == 0) {
			return (ute_ver_t)(i);
		}
	}
	return UTE_VERSION_UNK;
}

/* utefile.c ends here */
