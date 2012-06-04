/*** utefile.c -- high level interface to ute files (r/w)
 *
 * Copyright (C) 2010-2012 Sebastian Freundt
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

#define UTEFILE_C
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
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

#if defined DEBUG_FLAG
# include <assert.h>
# include <stdio.h>
# define UDEBUG(args...)	fprintf(stderr, args)
# define scom_tick_size		__local_scom_tick_size
# define scom_byte_size		__local_scom_byte_size
# define MAYBE_NOINLINE		__attribute__((noinline))
#else
# define UDEBUG(args...)
# define assert(args...)
# define MAYBE_NOINLINE
#endif	/* DEBUG_FLAG */
/* for serious debugging */
#if !defined UDEBUGvv
# define UDEBUGvv(args...)
#endif	/* !UDEBUGvv */

#define SMALLEST_LVTD	(0)

size_t __pgsz;

/* not the best of ideas to have output printing in a lib */
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
static void
__attribute__((format(printf, 2, 3)))
error(int eno, const char *fmt, ...)
{
	va_list vap;

	va_start(vap, fmt);
	fputs("utefile.c: ", stderr);
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

#if defined scom_tick_size
static inline size_t
__local_scom_tick_size(scom_t t)
{
	assert((t->ttf & 0x30U) != 0x30U);

	if (!(scom_thdr_ttf(t) & (SCOM_FLAG_LM | SCOM_FLAG_L2M))) {
		return 1UL;
	} else if (scom_thdr_ttf(t) & SCOM_FLAG_LM) {
		return 2UL;
	} else if (scom_thdr_ttf(t) & SCOM_FLAG_L2M) {
		return 4UL;
	} else {
		return 0UL;
	}
}

static inline size_t
__local_scom_byte_size(scom_t t)
{
	return __local_scom_tick_size(t) * sizeof(struct sndwch_s);
}
#endif	/* scom_tick_size */


/* aux */
static char*
mmap_any(int fd, int prot, int flags, off_t off, size_t len)
{
	sidx_t ofp = off / __pgsz, ofi = off % __pgsz;
	char *p = mmap(NULL, len + ofi, prot, flags, fd, ofp * __pgsz);
	return LIKELY(p != MAP_FAILED) ? p + ofi : NULL;
}

static void
munmap_any(char *map, off_t off, size_t len)
{
	sidx_t ofi = off % __pgsz;
	munmap(map - ofi, len + ofi);
	return;
}

static inline int
__pflags(utectx_t ctx)
{
	return PROT_READ | ((ctx->oflags & UO_RDWR) ? PROT_WRITE : 0);
}

static size_t
get_slut_size(utehdr2_t hdr)
{
	switch (utehdr_endianness(hdr)) {
	case UTE_ENDIAN_UNK:
	case UTE_ENDIAN_LITTLE:
		return le32toh(hdr->slut_sz);
	case UTE_ENDIAN_BIG:
		return be32toh(hdr->slut_sz);
	default:
		break;
	}
	return 0;
}


/* header caching, also probing */
static int
cache_hdr(utectx_t ctx)
{
	/* we just use max size here */
	const size_t sz = sizeof(struct utehdr2_s);
	struct utehdr2_s *res;
	int pflags = __pflags(ctx);

	/* check if the file's big enough */
	if (ctx->fsz < sz) {
		/* must be a mistake then */
		goto err_out;
	}
	/* just map the first sizeof(struct bla) bytes */
	res = mmap(NULL, sz, pflags, MAP_SHARED, ctx->fd, 0);
	if (UNLIKELY(res == MAP_FAILED)) {
		/* it failed, it failed, who cares why */
		goto err_out;
	}
	/* assign the header ... */
	ctx->slut_sz = get_slut_size((ctx->hdrp = res));
	/* ... and take a probe, if it's not for creation */
	if (ctx->oflags & UO_TRUNC) {
		/* don't bother checking the header */
		return 0;
	} else if (!utehdr_check_magic(res)) {
		/* perfect, magic string fits, endianness matches, I'm happy */
		return 0;
	} else if (ctx->oflags & UO_NO_HDR_CHK) {
		/* not perfect but just as good */
		return 0;
	}
	/* otherwise something's fucked */
	munmap(res, sz);
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
		memset((void*)ctx->hdrp, 0, sz);
		bump_header(ctx->hdrp);
	}
	/* file creation means new slut */
	ctx->slut_sz = 0;
	return;
}

/* seek reset */
void
flush_seek(uteseek_t sk)
{
/* the psz code will go eventually and be replaced with the rewound stuff
 * we're currently preparing in the tpc/seek api */
	if (sk->szrw > 0) {
		/* munmap given the computed page size, later to be replaced
		 * by seek_size(sk) which will account for tick rewindings */
		munmap(sk->sp, seek_byte_size(sk));
	}
	/* bit of cleaning up */
	sk->si = -1;
	sk->szrw = 0;
	sk->sp = NULL;
	sk->pg = -1;
	sk->fl = 0;
	return;
}

int
seek_page(uteseek_t sk, utectx_t ctx, uint32_t pg)
{
	size_t pgsz = UTE_BLKSZ * sizeof(*ctx->seek->sp);
	size_t off = page_offset(ctx, pg);
	int pflags = __pflags(ctx);
	sndwch_t sp;
	sidx_t nt = 0;

	/* trivial checks */
	if (UNLIKELY(off > ctx->fsz)) {
		UDEBUGvv("offset %zu out of bounds (%zu)\n", off, ctx->fsz);
		goto wipe;
	} else if (UNLIKELY(off + pgsz >= ctx->fsz)) {
		/* could be tpc space */
		if ((pgsz = ctx->fsz - off) == 0) {
			/* tpc space */
			UDEBUGvv("tpc space (%zub)\n", tpc_byte_size(ctx->tpc));
			goto wipe;
		}
	}
	/* make sure we respect the semantics of mmap() */
	assert(off % __pgsz == 0);
	{
		/* create a new seek */
		void *tmp = mmap(NULL, pgsz, pflags, MAP_SHARED, ctx->fd, off);

		if (UNLIKELY(tmp == MAP_FAILED)) {
			return -1;
		}
		/* prepare sk */
		sk->sp = tmp;
		if (pg == 0) {
			sk->si = sizeof(*ctx->hdrp) / sizeof(*ctx->seek->sp);
		} else {
			sk->si = 0;
		}
		sk->szrw = pgsz;
		sk->pg = pg;
		sk->fl = 0;
	}

	/* check if there's lone naughts at the end of the page */
	assert(sk->szrw / sizeof(*sk->sp) > 0);
	UDEBUGvv("inspecting %zu ticks\n", sk->szrw / sizeof(*sk->sp));
	for (sp = sk->sp + sk->szrw / sizeof(*sk->sp);
	     sp > sk->sp && sp[-1].key == -1ULL && sp[-1].sat == -1ULL;
	     sp--, nt++);
	/* sp should point to the scom after the last non-naught tick */
	UDEBUGvv("rewinding %zu ticks\n", nt);
	seek_rewind(sk, nt);
	return 0;
wipe:
	memset(sk, 0, sizeof(*sk));
	return 0;
}

#if !defined USE_UTE_SORT
static inline size_t
page_size(utectx_t ctx, uint32_t page)
{
/* Return the size (in bytes) of the PAGE-th page in CTX. */
	const size_t pgsz = UTE_BLKSZ * sizeof(*ctx->seek->sp);
	const size_t tot = page * pgsz;

	if (LIKELY(tot + pgsz <= ctx->fsz)) {
		return pgsz;
	}
	/* otherwise check if the page is beyond eof */
	if (LIKELY(tot + pgsz >= ctx->fsz)) {
		return ctx->fsz - tot;
	}
	/* otherwise the page is beyond */
	return 0;
}

static void
seek_tmppage(uteseek_t sk, utectx_t ctx, uint32_t pg)
{
	const int MAP_MEM = MAP_SHARED | MAP_ANONYMOUS;
	const int PROT_MEM = PROT_READ | PROT_WRITE;
	size_t psz = page_size(ctx, pg);
	void *tmp;

	/* create a new seek */
	tmp = mmap(NULL, psz, PROT_MEM, MAP_MEM, -1, 0);
	if (UNLIKELY(tmp == MAP_FAILED)) {
		return;
	}
	sk->sp = tmp;
	if (pg == 0) {
		sk->si = sizeof(*ctx->hdrp) / sizeof(*ctx->seek->sp);
	} else {
		sk->si = 0;
	}
	sk->sz = psz;
	sk->pg = pg;
	sk->fl = 0;
	return;
}
#endif	/* USE_UTE_SORT */

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

static inline sidx_t
index_to_tpc_index(utectx_t ctx, sidx_t i)
{
/* Return the index of I within the tpc hold
 * we just assume that tpc will never have more than UTE_BLKSZ ticks */
	/* calculations in bytes */
	size_t tot_off = i * sizeof(*ctx->seek->sp) + sizeof(*ctx->hdrp);
	if (tot_off >= ctx->fsz) {
		return (tot_off - ctx->fsz) / sizeof(*ctx->seek->sp);
	}
	return 0;
}

/* seek to */
scom_t
ute_seek(utectx_t ctx, sidx_t i)
{
	/* wishful thinking */
	if (UNLIKELY(index_past_eof_p(ctx, i))) {
		return NULL;
	} else if (UNLIKELY(index_in_tpc_space_p(ctx, i))) {
		sidx_t new_i = index_to_tpc_index(ctx, i);
		return tpc_get_scom(ctx->tpc, new_i);
	} else if (UNLIKELY(!index_in_seek_page_p(ctx, i))) {
		reseek(ctx, i);
	} else {
		/* just reseek manually */
		ctx->seek->si = offset_of_index(ctx, i);
	}
	return seek_get_scom(ctx->seek);
}

static void
store_lvtd(utectx_t ctx)
{
	if (tpc_has_ticks_p(ctx->tpc) && ctx->tpc->last > ctx->lvtd) {
		ctx->lvtd = ctx->tpc->last;
	}
	ctx->tpc->least = ctx->lvtd;
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

static inline size_t
next_multiple_of(size_t foo, size_t mul)
{
	return foo % mul ? foo + mul - foo % mul : foo;
}

static inline size_t
prev_multiple_of(size_t foo, size_t mul)
{
	return foo - foo % mul;
}

static void MAYBE_NOINLINE
flush_tpc(utectx_t ctx)
{
	size_t sz = tpc_byte_size(ctx->tpc);
	sidx_t si = ctx->tpc->sk.si;
	size_t sisz = si * sizeof(*ctx->tpc->sk.sp);
	size_t fsz = ctx->fsz;

#if defined DEBUG_FLAG
	/* tpc should be sorted now and contain no randomised data */
	{
		uint64_t thresh = 0;
		uteseek_t sk = &ctx->tpc->sk;

		for (sidx_t i = 0, tsz; i < si; i += tsz) {
			scom_t t = AS_SCOM(sk->sp + i);

			assert((t->ttf & 0x30U) != 0x30U);
			assert(thresh <= t->u);
			thresh = t->u;
			tsz = scom_tick_size(t);
		}
	}
#endif	/* DEBUG_FLAG */
	assert(sisz <= sz);
	/* extend to take SZ additional bytes */
	if (ctx->oflags == UO_RDONLY || !ute_extend(ctx, sz)) {
		return;
	}
	/* span a map covering the SZ new bytes */
	{
		sidx_t ix = fsz % __pgsz;
		size_t foff = prev_multiple_of(fsz, __pgsz);
		size_t mpsz = next_multiple_of(ix + sz, __pgsz);
		char *p;

		p = mmap(NULL, mpsz, PROT_FLUSH, MAP_FLUSH, ctx->fd, foff);
		if (p == MAP_FAILED) {
			return;
		}
		memcpy(p + ix, ctx->tpc->sk.sp, sisz);
		/* memset the rest with the marker tick */
		if (sisz < sz) {
			memset(p + ix + sisz, -1, sz - sisz);
		}
		munmap(p, mpsz);
	}

	/* store the largest-value-to-date */
	store_lvtd(ctx);

	/* munmap the tpc? */
	clear_tpc(ctx->tpc);
	/* ah, this also means we can now use the full capacity */
	ctx->tpc->sk.cap = UTE_BLKSZ;
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
#if !defined USE_UTE_SORT
static void
merge_tpc(utectx_t ctx, utetpc_t tpc)
{
/* assume all tick pages in CTX are sorted except for TPC
 * now merge-sort those with TPC */
	size_t npg = ute_npages(ctx);
	struct uteseek_s sk[2];
	/* page indicator */
	size_t pg;
	/* index keys */
	scidx_t ti;

	/* set up seeker through tpc */
	ti = make_scidx(tpc_first_scom(tpc));

	for (pg = 0; pg < npg; pg++) {
		/* seek to the i-th page */
		seek_page(sk, ctx, pg);
		/* use bin-srch to find an offending tick */
		if (seek_key(sk, ti)) {
			goto mrg;
		}
		/* no need for this page */
		flush_seek(sk);
	}
	return;

mrg:
	/* get a temporary tpc page (where the merges go) */
	seek_tmppage(sk + 1, ctx, pg);
	sk[1].si = sk->si;
	memcpy(sk[1].sp, sk[0].sp, sk->si * sizeof(*sk->sp));

	do {
		merge_2tpc(sk + 1, sk + 0, tpc);
		/* tpc might have become unsorted, sort it now */
		tpc_sort(tpc);
		/* copy the tmp page back */
		memcpy(sk[0].sp, sk[1].sp, sk[1].si * sizeof(*sk->sp));
		/* after this, sk[0] will be empty and we can proceed
		 * with another seek/page */
		flush_seek(sk);
	} while (++pg < npg && (
			 /* load next page */
			 seek_page(sk, ctx, pg),
			 /* reset the tmp page counters */
			 sk[1].si = 0,
			 1
			 ));
	/* update ctx's lvtd and tpc's least */
	{
		scom_t last = seek_last_scom(sk + 1);
		tpc->least = ctx->lvtd = last->u;
	}
	/* flush the tmp seek too */
	flush_seek(sk + 1);
	/* unset the need merge flag */
	unset_tpc_needmrg(tpc);
	return;
}
#endif	/* USE_UTE_SORT */

#if defined AUTO_TILMAN_COMP
static bool
seek_eof_p(uteseek_t sk)
{
	return sk->si >= sk->szrw / sizeof(*sk->sp);
}

static void
clone_seek(uteseek_t tgt, uteseek_t src)
{
	*tgt = *src;
	return;
}

static void
tilman_comp(utectx_t ctx)
{
	size_t npg = ute_npages(ctx);
	struct uteseek_s sk[2];
	size_t pg;
	size_t tpg;

	/* for as long there's no compressed pages, try */
	for (pg = 0;
	     pg < npg && (seek_page(sk, ctx, pg), tilman_1comp(sk, sk) == 0);
	     pg++, flush_seek(sk));

	/* great, either pg < npg ... */
	if (pg >= npg) {
		return;
	}
	/* ... or tilman_1comp() isn't 0 in which case sk hasn't been
	 * flushed and pg hasn't been inc'd */
	clone_seek(sk + 1, sk);
	for (tpg = pg++; pg < npg; pg++) {
		/* seek to the next page */
		seek_page(sk, ctx, pg);
	cont:
		tilman_1comp(sk + 1, sk);
		/* drain the pages properly */
		if (seek_eof_p(sk + 1)) {
			flush_seek(sk + 1);
			seek_page(sk + 1, ctx, ++tpg);
			goto cont;
		} else if (seek_eof_p(sk)) {
			flush_seek(sk);
		} else {
			/* what's going on? */
			abort();
		}
	}
	/* tpg + 1 is now the final page count, sk[1] holds the offset
	 * trunc the file accordingly */
	ute_trunc(ctx, (tpg * UTE_BLKSZ + sk[1].si) * sizeof(*sk->sp));
	return;
}
#endif	/* AUTO_TILMAN_COMP */

static void MAYBE_NOINLINE
tpc_from_seek(utectx_t ctx, uteseek_t sk)
{
	size_t sk_sz = seek_byte_size(sk);
	const size_t cp_sz = sk_sz - sk->si * sizeof(*sk->sp);

	if (!tpc_active_p(ctx->tpc)) {
		if (sk->pg == 0) {
			const size_t hdr = sizeof(*ctx->hdrp) / sizeof(*sk->sp);
			make_tpc(ctx->tpc, UTE_BLKSZ - hdr);
		} else {
			make_tpc(ctx->tpc, UTE_BLKSZ);
		}
	}
	if (cp_sz) {
		/* copy the last page, from index sk->si onwards */
		memcpy(ctx->tpc->sk.sp, sk->sp + sk->si, cp_sz);
		/* ... and set the new length */
		ctx->tpc->sk.si = cp_sz / sizeof(*sk->sp);
	}

	if (cp_sz) {
		/* store the first value as ctx's lvtd
		 * and the last as tpc's last */
		scom_t frst = seek_get_scom(sk);
		scom_t last = seek_last_scom(sk);

		ctx->lvtd = ctx->tpc->least = frst->u;
		ctx->tpc->last = last->u;
	} else {
		/* otherwise rinse */
		ctx->lvtd = ctx->tpc->least = 0;
		ctx->tpc->last = 0;
	}
	return;
}

static void MAYBE_NOINLINE
load_last_tpc(utectx_t ctx)
{
/* take the last page in CTX and make a tpc from it, trunc the file
 * accordingly, this is in a way a reverse flush_tpc() */
	size_t lpg = ute_npages(ctx);
	struct uteseek_s sk[1];

	if (UNLIKELY(lpg == 0)) {
		goto wipeout;
	} else if (!(ctx->oflags & UO_RDWR)) {
		/* we mustn't change things, so fuck off right here */
		goto wipeout;
	} else if ((ctx->oflags & UO_NO_LOAD_TPC)) {
		/* we don't want to load the tpc */
		goto wipeout;
	}

	/* seek to the last page */
	seek_page(sk, ctx, lpg - 1);
	/* create the tpc space */
	tpc_from_seek(ctx, sk);
	/* now munmap the seek */
	flush_seek(sk);
	/* also set the last and lvtd values */
	/* store the largest-value-to-date */
	store_lvtd(ctx);

	/* real shrinking was to dangerous without C-c handler,
	 * make fsz a multiple of page size */
	if (ctx->fsz > tpc_byte_size(ctx->tpc) + ctx->slut_sz) {
		ctx->fsz -= tpc_byte_size(ctx->tpc) + ctx->slut_sz;
	} else {
		ctx->fsz -= ctx->slut_sz;
	}
	return;
wipeout:
	memset(ctx->tpc, 0, sizeof(*ctx->tpc));
	return;
}


static char*
mmap_slut(utectx_t ctx)
{
	const size_t off = ctx->fsz - ctx->slut_sz;
	int pflags = __pflags(ctx);
	return mmap_any(ctx->fd, pflags, MAP_FLUSH, off, ctx->slut_sz);
}

static void
munmap_slut(utectx_t ctx, char *map)
{
	const size_t off = ctx->fsz - ctx->slut_sz;
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

	/* set global page size */
	__pgsz = (size_t)sysconf(_SC_PAGESIZE);

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
	}
	/* load the last page as tpc */
	load_last_tpc(res);
	return res;
}


/* primitives to kick shit off */
utectx_t
ute_open(const char *path, int oflags)
{
	int resfd;
	int real_oflags;

	/* massage the flags */
	if (UNLIKELY((oflags & UO_WRONLY))) {
		/* stupid nutters, we allow read/write, not write-only */
		oflags = (oflags & ~UO_WRONLY) | UO_RDWR;
	}
	/* comb out stuff that will confuse open() */
	real_oflags = oflags & ~(UO_ANON | UO_NO_HDR_CHK | UO_NO_LOAD_TPC);
	/* we need to open the file RDWR at the moment, various
	 * mmap()s use PROT_WRITE */
	if (real_oflags > UO_RDONLY) {
		oflags |= UO_RDWR;
		real_oflags |= UO_RDWR;
	}
	/* try and open the file first */
	if ((resfd = open(path, real_oflags, 0644)) < 0) {
		/* ooooh, leave with a big bang */
		return NULL;
	}
	return make_utectx(path, resfd, oflags);
}

utectx_t
ute_mktemp(int oflags)
{
	char *tmpfn;
	int resfd;

	/* get a tmp name template for mkstemp() */
	tmpfn = ute_tmpnam();
	/* now open it ... */
	resfd = mkstemp(tmpfn);
	chmod(tmpfn, 0644);
	/* wipe and check for anon */
	if ((oflags &= UO_ANON)) {
		/* ... and unlink it if ANON is set */
		unlink(tmpfn);
	}
	oflags |= UO_CREAT;
	oflags |= UO_TRUNC;
	oflags |= UO_RDWR;
	oflags |= O_EXCL;
	return make_utectx(tmpfn, resfd, oflags);
}

#if defined USE_UTE_SORT
static void
ute_prep_sort(utectx_t ctx)
{
/* this is probably the worst we can do */
	/* delete the file */
	unlink(ctx->fname);
	return;
}
#endif	/* USE_UTE_SORT */

void
ute_close(utectx_t ctx)
{
	/* first make sure we write the stuff */
	ute_flush(ctx);
	if (!ute_sorted_p(ctx)) {
#if defined USE_UTE_SORT
		ute_prep_sort(ctx);
		ute_sort(ctx);
#endif	/* USE_UTE_SORT */
		ute_unset_unsorted(ctx);
	}
#if defined AUTO_TILMAN_COMP
	/* tilman compress the file, needs to happen after sorting */
	tilman_comp(ctx);
#endif	/* AUTO_TILMAN_COMP */
	/* serialse the slut */
	flush_slut(ctx);
	/* ... and finalise */
	free_slut(ctx->slut);

	/* close the tpc */
	free_tpc(ctx->tpc);
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
	if (!tpc_active_p(ctx->tpc) || !tpc_has_ticks_p(ctx->tpc)) {
		return;
	}
	/* also sort and diskify the currently active tpc */
	if (!tpc_sorted_p(ctx->tpc)) {
		tpc_sort(ctx->tpc);
	}
	if (tpc_needmrg_p(ctx->tpc)) {
		/* special case when the page cache has detected
		 * a major violation */
		ute_set_unsorted(ctx);
#if !defined USE_UTE_SORT
		/* since ute_sort() doesn't work, just use merge_tcp() */
		merge_tpc(ctx, ctx->tpc);
#endif	/* USE_UTE_SORT */
	}
	flush_tpc(ctx);
	return;
}

void
ute_clone_slut(utectx_t tgt, utectx_t src)
{
	/* free any existing sluts */
	free_slut(tgt->slut);
	/* now clone */
	tgt->slut_sz = src->slut_sz;
	clone_slut(tgt->slut, src->slut);
	return;
}

void
ute_empty_slut(utectx_t ctx)
{
	/* free existing sluts */
	free_slut(ctx->slut);
	/* reset slut size */
	ctx->slut_sz = 0;
	/* make a new slut */
	make_slut(ctx->slut);
	return;
}


/* accessor */
void
ute_add_tick(utectx_t ctx, scom_t t)
{
	size_t tsz;

	/* never trust your users, inspect the tick */
	if (UNLIKELY((t->ttf & 0x30U) == 0x30U)) {
		error(0, "\
this version of uterus cannot cope with tick type %x", t->ttf);
		return;
	} else if (UNLIKELY(t->u == -1ULL)) {
		error(0, "invalid tick");
	}

	/* post tick inspection */
	tsz = scom_tick_size(t);
	assert(tpc_active_p(ctx->tpc));
	if (!tpc_can_hold_p(ctx->tpc, tsz)) {
		/* great, compute the number of leap ticks */
		uteseek_t sk = &ctx->tpc->sk;
		size_t nleap = sk->szrw / sizeof(*sk->sp) - sk->si;

		UDEBUGvv("tpc full (has: %zut/%zut)\n", sk->si, sk->si + nleap);
		assert(sk->szrw / sizeof(*sk->sp) >= sk->si);
		seek_rewind(sk, nleap);
		ute_flush(ctx);
	}
	/* and now it's just passing on everything to the tpc adder */
	tpc_add(ctx->tpc, t, tsz);
	return;
}

/* private version */
void
ute_add_ticks(utectx_t ctx, const void *src, size_t nticks)
{
	assert(tpc_active_p(ctx->tpc));
	if (tpc_full_p(ctx->tpc)) {
		/* oh current tpc is full, flush and start over */
		ute_flush(ctx);
	}
	/* this needs looping! */
	tpc_add(ctx->tpc, src, nticks);
	return;
}

size_t
ute_nticks(utectx_t ctx)
{
/* for the moment just use the file size and number of pages
 * plus whats in the tpc */
	size_t aux_sz = sizeof(struct utehdr2_s) + ctx->slut_sz;
	size_t nticks = (ctx->fsz - aux_sz) / sizeof(*ctx->tpc->sk.sp);
	/* if there are non-flushed ticks, consider them */
	if (tpc_active_p(ctx->tpc)) {
		nticks += ctx->tpc->sk.si;
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
		return sizeof(struct sndwch_s);
	}
	return 0;
}

size_t
ute_tick2(utectx_t ctx, void *tgt, size_t tsz, sidx_t i)
{
	scom_t p = ute_seek(ctx, i);
	if (LIKELY(p != NULL)) {
		memcpy(tgt, p, tsz);
		return sizeof(struct sndwch_s);
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

uint16_t
ute_bang_symidx(utectx_t ctx, const char *sym, uint16_t idx)
{
	if (UNLIKELY(sym == NULL)) {
		return 0;
	}
	return slut_bang(ctx->slut, sym, idx);
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
	return utehdr_version(ctx->hdrp);
}

ute_end_t
ute_endianness(utectx_t ctx)
{
/* return the number of symbols tracked in the ute file */
	return utehdr_endianness(ctx->hdrp);
}

int
ute_check_endianness(utectx_t ctx)
{
/* just fall back to utehdr.h solution */
	return utehdr_check_endianness(ctx->hdrp);
}

/* utefile.c ends here */
