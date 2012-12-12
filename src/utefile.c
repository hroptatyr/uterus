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
#include "boobs.h"

#if defined HAVE_LZMA_H
# include <lzma.h>
#endif	/* HAVE_LZMA_H */

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

#if !defined ROUND
# define ROUND(s, a)		(a * ((s + a - 1) / a))
#endif	/* !ROUND */

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

#if defined scom_tick_size && defined DEBUG_FLAG
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

static inline __attribute__((unused)) size_t
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
__rdwrp(const_utectx_t ctx)
{
	return ctx->oflags & 0x03;
}

static inline int
__pflags(const_utectx_t ctx)
{
	return PROT_READ | (__rdwrp(ctx) ? PROT_WRITE : 0);
}

static __attribute__((pure)) size_t
get_ftr_size(const_utectx_t ctx)
{
/* retrieve the size of the footer in the header HDR in native endianness */
	utehdr2_t hdr = ctx->hdrc;

	switch (utehdr_endianness(hdr)) {
	case UTE_ENDIAN_UNK:
	case UTE_ENDIAN_LITTLE:
		return le32toh(hdr->ftr_sz);
	case UTE_ENDIAN_BIG:
		return be32toh(hdr->ftr_sz);
	default:
		break;
	}
	return 0U;
}

static __attribute__((pure)) off_t
get_ftr_off(const_utectx_t ctx)
{
/* get the offset off the slut within the ute file CTX
 * we go backwards through footer and metadata */
	const size_t tz = sizeof(*ctx->seek->sp);
	size_t cand = ctx->fsz - get_ftr_size(ctx);

	/* round down to previous TZ multiple */
	return cand & ~(tz - 1);
}

static __attribute__((pure)) size_t
get_slut_size(const_utectx_t ctx)
{
/* retrieve the size of the slut in the header HDR in native endianness */
	utehdr2_t hdr = ctx->hdrc;

	switch (utehdr_endianness(hdr)) {
	case UTE_ENDIAN_UNK:
	case UTE_ENDIAN_LITTLE:
		return le32toh(hdr->slut_sz);
	case UTE_ENDIAN_BIG:
		return be32toh(hdr->slut_sz);
	default:
		break;
	}
	return 0U;
}

static __attribute__((pure)) off_t
get_slut_off(const_utectx_t ctx)
{
/* get the offset off the slut within the ute file CTX
 * we go backwards through footer and metadata */
	const size_t tz = sizeof(*ctx->seek->sp);
	size_t cand = ctx->fsz - get_ftr_size(ctx) - get_slut_size(ctx);

	/* round down to previous TZ multiple */
	return cand & ~(tz - 1);
}

static size_t
get_npages(utehdr2_t hdr)
{
/* retrieve the number of pages in the header HDR in native endianness */
	switch (utehdr_endianness(hdr)) {
	case UTE_ENDIAN_UNK:
	case UTE_ENDIAN_LITTLE:
		return le32toh(hdr->npages);
	case UTE_ENDIAN_BIG:
		return be32toh(hdr->npages);
	default:
		break;
	}
	return 0U;
}

static inline bool
__fwr_trunc(int fd, size_t sz)
{
	if ((fd < 0) || (ftruncate(fd, sz) < 0)) {
		return false;
	}
	return true;
}

static inline bool
ute_trunc(utectx_t ctx, size_t sz)
{
	if (!__fwr_trunc(ctx->fd, sz)) {
		return false;
	}
	ctx->fsz = sz;
	return true;
}

bool
ute_extend(utectx_t ctx, ssize_t z)
{
/* extend ute file by at least Z bytes */
	const size_t tz = sizeof(*ctx->seek->sp);
	ssize_t tot;

	if (UNLIKELY((tot = z + ctx->fsz) <= 0)) {
		return false;
	}
	/* round up */
	tot = ((tot - 1U) | (tz - 1U)) + 1U;
	if (!__fwr_trunc(ctx->fd, tot)) {
		return false;
	}
	ctx->fsz = (size_t)tot;
	return true;
}

static void
ute_shrink(utectx_t ctx, ssize_t z)
{
/* like ute_extend() with -Z, but round the file size down */
	const size_t tz = sizeof(*ctx->seek->sp);
	ssize_t tot;

	if (UNLIKELY(z == 0)) {
		/* don't bother */
		return;
	} else if (UNLIKELY((tot = ctx->fsz - z) <= 0)) {
		return;
	}
	/* round down */
	tot &= ~(tz - 1U);
	/* don't actually shrink the whole shebang, set the fsz instead */
	ctx->fsz = (size_t)tot;
	return;
}


/* header caching, also probing */
static int
cache_hdr(utectx_t ctx)
{
	/* we just use max size here */
	const size_t sz = sizeof(*ctx->hdrc);
	struct utehdr2_s *res;

	/* check if the file's big enough */
	if (ctx->fsz < sz) {
		/* must be a mistake then */
		goto err_out;
	}
	/* just map the first sizeof(struct bla) bytes */
	res = mmap(NULL, sz, PROT_READ, MAP_SHARED, ctx->fd, 0);
	if (UNLIKELY(res == MAP_FAILED)) {
		/* it failed, it failed, who cares why */
		goto err_out;
	}
	/* assign the header ... */
	ctx->hdrp = res;
	*ctx->hdrc = *res;
	/* do the rest of the probing */
	ctx->npages = get_npages(ctx->hdrc);
	/* ... and take a probe, if it's not for creation */
	if (ctx->oflags & UO_TRUNC) {
		/* don't bother checking the header */
		return 0;
	} else if (!utehdr_check_magic(ctx->hdrc)) {
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
	return -1;
}

static void
close_hdr(utectx_t ctx)
{
	/* munmap the header */
	if (ctx->hdrp != NULL) {
		/* we don't care if we can keep the const promise of HDRP
		 * in CTX hereafter */
		union {
			const void *c;
			void *p;
		} tmp = {
			.c = ctx->hdrp,
		};
		munmap(tmp.p, sizeof(*ctx->hdrp));
	}
	ctx->hdrp = NULL;
	return;
}

static void
creat_hdr(utectx_t ctx)
{
	const size_t sz = sizeof(*ctx->hdrc);

	/* trunc to sz */
	ute_trunc(ctx, sz);
	/* cache the header */
	(void)cache_hdr(ctx);
	/* set standard header payload offset, just to be sure it's sane */
	memset((void*)ctx->hdrc, 0, sz);
	bump_header(ctx->hdrc);
	return;
}

#if defined HAVE_LZMA_H
uint32_t ute_encode_clevel = 6;

ssize_t
ute_encode(void *tgt[static 1], const void *buf, const size_t bsz)
{
	static lzma_stream strm = LZMA_STREAM_INIT;
	static size_t pgsz = 0UL;
	static uint8_t *iobuf = NULL;
	lzma_ret rc;
	ssize_t res = 0;

	if (UNLIKELY(buf == NULL)) {
		/* oh we're meant to free things */
		res = 0;
		if (iobuf != NULL) {
			goto iob_free;
		}
		/* everything should be freed already */
		goto fa_free;
	} else if (iobuf == NULL) {
		pgsz = UTE_BLKSZ * sizeof(struct sndwch_s);
		iobuf = mmap(NULL, pgsz, PROT_MEM, MAP_MEM, -1, 0);
		if (UNLIKELY(iobuf == MAP_FAILED)) {
			res = -1;
			goto enc_free;
		}
	}

	/* set up new encoder */
	rc = lzma_easy_encoder(&strm, ute_encode_clevel, LZMA_CHECK_CRC64);
	if (UNLIKELY(rc != LZMA_OK)) {
		/* indicate total failure, free fuckall */
		res = -1;
		goto fa_free;
	}

	/* reset in/out buffer */
	strm.next_out = iobuf;
	strm.avail_out = pgsz;
	/* point to the stuff we're meant to encode */
	strm.next_in = buf;
	strm.avail_in = bsz;

	if (UNLIKELY((rc = lzma_code(&strm, LZMA_FINISH)) != LZMA_STREAM_END)) {
		/* BUGGER, shall we signal an error? */
		;
	} else if (LIKELY(tgt != NULL)) {
		/* now then, make *tgt point to the outbuffer */
		*tgt = iobuf;
		res = strm.next_out - iobuf;
	}
	lzma_end(&strm);
	strm = (typeof(strm))LZMA_STREAM_INIT;
	return res;
iob_free:
	munmap(iobuf, pgsz);
enc_free:
	iobuf = NULL;
	strm = (typeof(strm))LZMA_STREAM_INIT;
fa_free:
	return res;
}

ssize_t
ute_decode(void *tgt[static 1], const void *buf, const size_t bsz)
{
	static lzma_stream strm = LZMA_STREAM_INIT;
	static size_t pgsz = 0UL;
	static uint8_t *iobuf = NULL;
	lzma_ret rc;
	ssize_t res = 0;

	if (UNLIKELY(buf == NULL)) {
		/* oh we're meant to free things */
		res = 0;
		if (iobuf != NULL) {
			goto iob_free;
		}
		/* everything should be freed already */
		goto fa_free;
	} else if (iobuf == NULL) {
		pgsz = UTE_BLKSZ * sizeof(struct sndwch_s);
		iobuf = mmap(NULL, pgsz, PROT_MEM, MAP_MEM, -1, 0);
		if (UNLIKELY(iobuf == MAP_FAILED)) {
			res = -1;
			goto enc_free;
		}
	}

	/* set up new decoder */
	rc = lzma_stream_decoder(&strm, UINT64_MAX, 0);
	if (UNLIKELY(rc != LZMA_OK)) {
		/* indicate total failure, free fuckall */
		res = -1;
		goto fa_free;
	}
	/* reset in/out buffer */
	strm.next_out = iobuf;
	strm.avail_out = pgsz;
	/* point to the stuff we're meant to encode */
	strm.next_in = buf;
	strm.avail_in = bsz;

	if (UNLIKELY((rc = lzma_code(&strm, LZMA_FINISH)) != LZMA_STREAM_END)) {
		/* BUGGER, shall we signal an error? */
		error(0, "cannot inflate ticks: %d\n", rc);
	} else if (LIKELY(tgt != NULL)) {
		/* now then, make *tgt point to the outbuffer */
		*tgt = iobuf;
		res = strm.next_out - iobuf;
	}
	lzma_end(&strm);
	strm = (typeof(strm))LZMA_STREAM_INIT;
	return res;
iob_free:
	munmap(iobuf, pgsz);
enc_free:
	iobuf = NULL;
	strm = (typeof(strm))LZMA_STREAM_INIT;
fa_free:
	return res;
}

#else  /* !HAVE_LZMA_H */
uint32_t ute_encode_clevel = 0;

ssize_t
ute_encode(void *tgt[static 1], const void *UNUSED(x), const size_t UNUSED(y))
{
	*tgt = NULL;
	return 0;
}

ssize_t
ute_decode(void *tgt[static 1], const void *UNUSED(x), const size_t UNUSED(y))
{
	*tgt = NULL;
	return 0;
}
#endif	/* HAVE_LZMA_H */

/* seek reset */
void
flush_seek(uteseek_t sk)
{
/* the psz code will go eventually and be replaced with the rewound stuff
 * we're currently preparing in the tpc/seek api */
	if (sk->szrw > 0 && !(sk->fl & TPC_FL_STATIC_SP)) {
		/* compute the page size, takes tick rewinds into account */
		size_t pgsz = seek_byte_size(sk);
		size_t o = seek_offset(sk);

		/* munmap it all */
		munmap_any((void*)sk->sp, o, pgsz);
	}
	/* bit of cleaning up */
	sk->si = -1;
	sk->szrw = 0;
	sk->sp = NULL;
	sk->pg = -1;
	sk->fl = 0;
	return;
}


struct sk_offs_s {
	/* file offset */
	size_t foff;
	/* file length */
	size_t flen;
};

static size_t
page_compressed_p(const void *restrict p)
{
/* If P points to a compressed page return its file-level size, 0 otherwise */
	const uint32_t *pu32 = p;
	size_t res = 0UL;

	if (memcmp(pu32 + 1, "\xfd" "7zXZ\0", 6) == 0) {
		res = ROUND(pu32[0] + sizeof(*pu32), sizeof(struct sndwch_s));
	}
	return res;
}

static struct sk_offs_s
seek_get_offs(utectx_t ctx, uint32_t pg)
{
	const size_t pgsz = UTE_BLKSZ * sizeof(*ctx->seek->sp);
	size_t off;
	size_t len;

	if (ctx->hdrc->flags & UTEHDR_FLAG_COMPRESSED && ctx->ftr != NULL) {
		/* use the footer info */
		const struct uteftr_cell_s *cells = ctx->ftr->c;

		UDEBUGvv("using footer info\n");
		if ((ctx->ftr->z / sizeof(*cells)) < pg) {
			off = len = 0U;
		} else {
			off = cells[pg].foff;
			len = cells[pg].flen;
		}

	} else if (ctx->hdrc->flags & UTEHDR_FLAG_COMPRESSED) {
		/* fuck, compression and no footer, i feel terrible */
		const size_t probe_z = 32U;
		off_t try = ute_hdrz(ctx);

		UDEBUGvv("try %zd\n", try);
		for (uint32_t i = 0; i <= pg; i++) {
			void *p = mmap_any(
				ctx->fd, PROT_READ, MAP_SHARED, try, probe_z);
			off_t otry = try;

			if (UNLIKELY(p == NULL)) {
				off = 0;
				len = 0;
				/* big bugger, just so we exit the loop */
				i = pg + 1;
			} else if ((len = page_compressed_p(p))) {
				off = otry;
				try += len;
			} else if (i) {
				/* page was not compressed? */
				off = otry;
				try += (len = pgsz);
			} else {
				/* page not compressed AND it's the first one */
				off = otry;
				try += (len = pgsz - off);
			}
			munmap_any(p, otry, probe_z);
		}
		/* check if we're on the last page */
		if (UNLIKELY((size_t)try > ctx->fsz)) {
			len = ctx->fsz - off;
		}

	} else if (pg > 0) {
		off = page_offset(ctx, pg);
		len = pgsz;

	} else {
		off = ute_hdrz(ctx);
		len = pgsz - off;
	}
	return (struct sk_offs_s){off, len};
}

int
seek_page(uteseek_t sk, utectx_t ctx, uint32_t pg)
{
/* Load page PG of CTX into SK
 * transparently decompress the page */
	struct sk_offs_s offs = seek_get_offs(ctx, pg);
	int pflags = __pflags(ctx);
	sndwch_t sp;
	sidx_t nt = 0;
	void *p;

	UDEBUGvv("I reckon page %u starts at %zu, length %zu\n",
		 pg, offs.foff, offs.flen);

	/* trivial checks */
	if (UNLIKELY(offs.foff > ctx->fsz)) {
		UDEBUGvv("offset %zu out of bounds (file size %zu)\n",
			 offs.foff, ctx->fsz);
		goto wipe;
	} else if (UNLIKELY(offs.foff + offs.flen >= ctx->fsz)) {
		/* could be tpc space */
		if ((offs.flen = ctx->fsz - offs.foff) == 0) {
			/* tpc space */
			UDEBUGvv("tpc space (%zub)\n", tpc_byte_size(ctx->tpc));
			goto wipe;
		}
	} else if (UNLIKELY(offs.flen == 0)) {
		/* something's fucked */
		return -1;
	}
	/* just mmap what we've got so far */
	p = mmap_any(ctx->fd, pflags, MAP_SHARED, offs.foff, offs.flen);
	if (UNLIKELY(p == NULL)) {
		return -1;
	}

	/* check for compression, even if lzma isn't available */
	if (page_compressed_p(p) == offs.flen) {
		/* length in memory, i.e. after decompressing */
		size_t mlen;
		const uint32_t *pu32 = p;
		void *x = NULL;

		mlen = ute_decode(&x, pu32 + 1, pu32[0]);
		/* after decompression we can't really do with this page */
		munmap_any(p, offs.foff, offs.flen);

#if !defined HAVE_LZMA_H
		UDEBUG("\
compressed page detected but no compression support, call the hotline\n");
#endif	/* HAVE_LZMA_H */
		if (UNLIKELY(mlen == 0U)) {
			UDEBUG("decomp'ing page %u gave 0 length\n", pg);
			return -1;
		}

		/* prepare sk, just the map for now */
		sk->sp = x;
		sk->szrw = mlen;
		sk->fl = TPC_FL_STATIC_SP;

	} else {
		/* prepare sk, first bit */
		sk->sp = p;
		sk->szrw = offs.flen;
		sk->fl = 0;
	}
	/* prepare sk */
	sk->si = 0UL;
	sk->pg = pg;
	seek_set_offset(sk, offs.foff);

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

int
clone_page(uteseek_t sk, utectx_t ctx, uteseek_t src)
{
	size_t pgsz = seek_byte_size(src);
	size_t off = page_offset(ctx, src->pg);

	/* trivial checks */
	if (LIKELY(off + pgsz >= ctx->fsz)) {
		/* yep, extend the guy */
		if (!__rdwrp(ctx) || !__fwr_trunc(ctx->fd, off + pgsz)) {
			return -1;
		}
		/* truncation successful */
		ctx->fsz = off + pgsz;
	}
	/* it's just a normal seek_page() */
	return seek_page(sk, ctx, src->pg);
}

int
make_page(uteseek_t sk, utectx_t ctx, uint32_t pg)
{
	sk->pg = pg;
	sk->szrw = UTE_BLKSZ * sizeof(*ctx->seek->sp);
	return clone_page(sk, ctx, sk);
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

/* seek to */
scom_t
ute_seek(utectx_t ctx, sidx_t i)
{
	/* calculate the page the tick should be in
	 * and the offset within that page */
	uint32_t p = page_of_index(ctx, i);
	uint32_t o = offset_of_index(ctx, i);
	size_t np = ute_npages(ctx);

	if (UNLIKELY(p > np)) {
		/* beyond hope */
		return NULL;
	} else if (UNLIKELY(p == np)) {
		/* could be tpc space or beyond eof */
		return tpc_get_scom(ctx->tpc, o);
	} else if (UNLIKELY(p != ctx->seek->pg)) {
		/* flush the old seek */
		flush_seek(ctx->seek);
		/* create a new seek */
		seek_page(ctx->seek, ctx, p);
		ctx->seek->si = o;
	} else {
		/* everything's fine, just reseek within the page */
		ctx->seek->si = o;
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
store_slut(utectx_t ctx, size_t sluz)
{
	struct utehdr2_s *h = ctx->hdrc;

	switch (utehdr_endianness(h)) {
	case UTE_ENDIAN_UNK:
	case UTE_ENDIAN_LITTLE:
		h->slut_sz = htole32(sluz);
		h->slut_nsyms = htole16((uint16_t)ctx->slut->nsyms);
		break;
	case UTE_ENDIAN_BIG:
		h->slut_sz = htobe32(sluz);
		h->slut_nsyms = htobe16((uint16_t)ctx->slut->nsyms);
		break;
	default:
		h->slut_sz = 0U;
		h->slut_nsyms = 0U;
		break;
	}
	return;
}

static void
store_ftrz(utectx_t ctx, size_t z)
{
	struct utehdr2_s *h = ctx->hdrc;

	switch (utehdr_endianness(h)) {
	case UTE_ENDIAN_UNK:
	case UTE_ENDIAN_LITTLE:
		h->ftr_sz = htole32(z);
		/* while we're at it? */
		h->npages = htole32(ctx->npages);
		break;
	case UTE_ENDIAN_BIG:
		h->ftr_sz = htobe32(z);
		/* while we're at it? */
		h->npages = htobe32(ctx->npages);
		break;
	default:
		h->ftr_sz = 0U;
		h->npages = 0U;
		break;
	}
	return;
}

#define PROT_FLUSH	(PROT_READ | PROT_WRITE)
#define MAP_FLUSH	(MAP_SHARED)

static void MAYBE_NOINLINE
flush_tpc(utectx_t ctx)
{
	size_t sz = tpc_byte_size(ctx->tpc);
	sidx_t si = ctx->tpc->sk.si;
	size_t sisz = si * sizeof(*ctx->tpc->sk.sp);
	const size_t hdrz = ute_hdrz(ctx);
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

	/* make sure we start behind the header */
	if (UNLIKELY(fsz < hdrz)) {
		ute_trunc(ctx, hdrz);
		fsz = hdrz;
	}

	/* extend to take SZ additional bytes */
	if (!__rdwrp(ctx) || !ute_extend(ctx, sz)) {
		return;
	}
	/* span a map covering the SZ new bytes */
	{
		char *p;

		p = mmap_any(ctx->fd, PROT_FLUSH, MAP_FLUSH, fsz, sz);
		if (UNLIKELY(p == NULL)) {
			return;
		}
		memcpy(p, ctx->tpc->sk.sp, sisz);
		/* memset the rest with the marker tick */
		if (sisz < sz) {
			memset(p + sisz, -1, sz - sisz);
		}
		munmap_any(p, fsz, sz);
		/* up the npages counter */
		ctx->npages++;
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
	if (UNLIKELY(!__rdwrp(ctx))) {
		return;
	}

	/* first of all serialise into memory */
	slut_seria(ctx->slut, &stbl, &stsz);

	if (UNLIKELY(stbl == NULL)) {
		return;
	} else if (UNLIKELY(stsz == 0)) {
		goto out;
	}
	/* extend to take STSZ (plus alignment) additional bytes */
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
	store_slut(ctx, stsz);

out:
	free(stbl);
	return;
}

static void
flush_hdr(utectx_t ctx)
{
	void *p;

	/* dont try at all in read-only mode */
	if (UNLIKELY(!__rdwrp(ctx))) {
		return;
	}

	UDEBUG("flushing %p %s header ...\n", ctx, ute_fn(ctx));
	/* mmap the header bit */
	p = mmap(NULL, sizeof(*ctx->hdrp), PROT_FLUSH, MAP_FLUSH, ctx->fd, 0);
	if (UNLIKELY(p == MAP_FAILED)) {
		return;
	}

	/* never let a dirty escape to the file flags */
	ctx->hdrc->flags &= ~UTEHDR_FLAG_DIRTY;

	/* otherwise copy the cache */
	memcpy(p, ctx->hdrc, sizeof(*ctx->hdrc));

	/* that's it */
	munmap(p, sizeof(*ctx->hdrp));
	return;
}


/* footer handling */
static void
add_ftr(utectx_t ctx, uint32_t pg, struct uteftr_cell_s c)
{
/* auto-resizing */
	struct uteftr_cell_s *cells;
	size_t ncells = ctx->ftr->z / sizeof(*cells);

	/* resize? */
	if (UNLIKELY(pg >= ncells)) {
		size_t nxpg = ((pg / 16U) + 1U) * 16U;
		ctx->ftr->z = nxpg * sizeof(*cells);
		ctx->ftr->c = realloc(ctx->ftr->c, ctx->ftr->z);
	}
	/* now we're clear to go */
	ctx->ftr->c[pg] = c;
	return;
}

static void
free_ftr(utectx_t ctx)
{
	if (ctx->ftr->c != NULL) {
		free(ctx->ftr->c);
		ctx->ftr->c = NULL;
		ctx->ftr->z = 0UL;
	}
	return;
}

static void
flush_ftr(utectx_t ctx)
{
/* pack the ftr and write a copy at the end of the file
 * should be called last */
	const struct uteftr_cell_s *ftr;
	size_t npg;

	/* dont try at all in read-only mode */
	if (UNLIKELY(!__rdwrp(ctx))) {
		return;
	} else if (UNLIKELY((ftr = ctx->ftr->c) == NULL)) {
		/* piss off right away */
		return;
	} else if (UNLIKELY((npg = ute_npages(ctx)) == 0U)) {
		return;
	} else if (UNLIKELY(ctx->ftr->z < npg * sizeof(*ftr))) {
		/* footer is fucked */
		UDEBUG("footer too small, not dumping %zu\n", ctx->ftr->z);
		return;
	}
	/* otherwise write exactly NPG cells to the disk */
	{
		size_t ftrz = npg * sizeof(*ftr);
		size_t fsz = ctx->fsz;
		char *p;

		/* extend to take BNDZ additional bytes */
		if (!ute_extend(ctx, ftrz)) {
			goto out;
		}

		UDEBUG("writing %zu footer bytes\n", ftrz);
		p = mmap_any(ctx->fd, PROT_FLUSH, MAP_FLUSH, fsz, ftrz);
		if (UNLIKELY(p == NULL)) {
			goto out;
		}
		memcpy(p, ftr, ftrz);
		munmap_any(p, fsz, ftrz);

		/* make sure we put the info in the file header */
		store_ftrz(ctx, ftrz);
	}

out:
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

#if defined HAVE_LZMA_H
static inline __attribute__((pure)) size_t
ute_hdrcz(const_utectx_t ctx)
{
/* return the size of CTX's header on disk in bytes */
	if (LIKELY(ctx->hdrc->ploff)) {
		return (size_t)ctx->hdrc->ploff;
	}
	return ute_hdrz(ctx);
}

static void
lzma_comp(utectx_t ctx)
{
/* compress all pages in CTX and, by side-effect, set CTX's fsz slot
 * (file size) to the total file size after compressing */
	struct ftr_s {
		uint64_t foff;
		uint32_t flen;
	};
	const size_t npg = ute_npages(ctx);
	const size_t tsz = sizeof(*ctx->seek->sp);
	int pflags = __pflags(ctx);
	struct ftr_s *ftr;
	off_t fo;

	if (UNLIKELY(npg == 0)) {
		return;
	} else if (UNLIKELY(pflags == PROT_READ)) {
		return;
	}

	/* build the ftr object */
	ftr = calloc(npg, sizeof(*ftr));
	/* get all them seeks */
	for (size_t i = 0; i < npg; i++) {
		struct sk_offs_s so = seek_get_offs(ctx, i);
		ftr[i].foff = so.foff;
		ftr[i].flen = so.flen;
	}

	/* seek to the first page (target file offset!)
	 * this can be very well different from the source file offset */
	fo = ute_hdrcz(ctx);

	UDEBUG("compressing %zu pages, starting at %zd\n", npg, fo);
	for (size_t i = 0; i < npg; i++) {
		/* i-th page */
		static void *pi = NULL;
		static size_t pz = 0UL;
		/* next page, just so we have the copy-on-write in place */
		void *pn = NULL;
		/* result of compression */
		void *cp;
		/* size after compression */
		ssize_t cz;

		if (pi == NULL) {
			pi = mmap_any(
				ctx->fd, pflags, MAP_PRIVATE,
				ftr[i].foff, pz = ftr[i].flen);
		}
		if (LIKELY(i + 1 < npg) &&
		    UNLIKELY(ftr[i].foff + ftr[i].flen > ftr[i + 1].foff)) {
			/* we need a private map of the guy afterwards */
			pn = mmap_any(
				ctx->fd, pflags, MAP_PRIVATE,
				ftr[i + 1].foff, ftr[i + 1].flen);
		}

		UDEBUG("comp'ing pg %zu  (%p[%zu],%zu)\n",
		       i, pi, ftr[i].foff, pz);
		if (LIKELY((cz = ute_encode(&cp, pi, pz)) > 0)) {
			uint32_t *p;
			size_t fz = ROUND(cz + sizeof(*p), tsz);

			/* pi is private (i.e. COW) so copy to the real file
			 * mmap from FO to FO + FZ */
			UDEBUG("mmapping [%zu,%zu]\n", fo, fo + fz);
			p = (void*)mmap_any(
				ctx->fd, pflags, MAP_SHARED, fo, fo + fz);
			if (LIKELY(p != NULL)) {
				p[0] = (uint32_t)cz;
				/* copy payload */
				memcpy(p + 1, cp, cz);
				/* memset the rest */
				memset((char*)(p + 1) + cz, 0, fz - cz);
				/* diskify */
				munmap_any((void*)p, fo, fo + fz);
			}
			/* also make sure to update the ftr */
			add_ftr(ctx, i, (struct uteftr_cell_s){
					fo, fz, pz / sizeof(*ctx->seek->sp)
						});
			/* and our global counter */
			fo += fz;
		}
		/* definitely munmap pi */
		munmap_any(pi, ftr[i].foff, ftr[i].flen);
		/* cross-assign pn to pi/pz */
		pi = pn;
		pz = ftr[i + 1].flen;
	}
	/* free resources */
	free(ftr);

	/* set ctx file size */
	ctx->fsz = fo;
	return;
}

#else  /* !HAVE_LZMA_H */

static void
lzma_comp(utectx_t UNUSED(ctx))
{
	return;
}
#endif	/* HAVE_LZMA_H */

static void MAYBE_NOINLINE
tpc_from_seek(utectx_t ctx, uteseek_t sk)
{
	size_t sk_sz = seek_byte_size(sk);
	const size_t cp_sz = sk_sz - sk->si * sizeof(*sk->sp);

	if (!tpc_active_p(ctx->tpc)) {
		make_tpc(ctx->tpc, page_sizet(ctx, sk->pg));
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

	if (!__rdwrp(ctx)) {
		/* we mustn't change things, so fuck off right here */
		goto wipeout;
	} else if ((ctx->oflags & UO_NO_LOAD_TPC)) {
		/* we don't want to load the tpc */
		goto wipeout;
	}

	/* seek to the last page */
	if (lpg > 0) {
		seek_page(sk, ctx, lpg - 1);
		/* create the tpc space */
		tpc_from_seek(ctx, sk);
		/* now munmap the seek */
		flush_seek(sk);
		/* update page counter, this isn't an official page anymore */
		ctx->npages--;
	} else {
		make_tpc(ctx->tpc, page_sizet(ctx, 0));
		/* bit of rinsing */
		ctx->lvtd = ctx->tpc->least = 0;
		ctx->tpc->last = 0;
	}
	/* also set the last and lvtd values */
	/* store the largest-value-to-date */
	store_lvtd(ctx);

	/* real shrinking was to dangerous without C-c handler,
	 * make fsz a multiple of page size */
	if (ctx->fsz > tpc_byte_size(ctx->tpc)) {
		ute_shrink(ctx, tpc_byte_size(ctx->tpc));
	}
	return;
wipeout:
	memset(ctx->tpc, 0, sizeof(*ctx->tpc));
	return;
}


/* auxiliary data deserialisers */
static void
load_slut(utectx_t ctx)
{
/* take the stuff past the last page in CTX and make a slut from it */
	if (UNLIKELY(ctx->fsz <= UTEHDR_MIN_SIZE)) {
		return;
	}

	/* otherwise leap to behind the last tick and
	 * deserialise the look-up table */
	const size_t sluz = get_slut_size(ctx);
	const off_t off = get_slut_off(ctx);
	const int pflags = __pflags(ctx);
	char *slut;

	if ((slut = mmap_any(ctx->fd, pflags, MAP_FLUSH, off, sluz)) != NULL) {
		slut_deser(ctx->slut, slut, sluz);
		munmap_any(slut, off, sluz);
	}

	/* real shrink is too dangerous, just adapt fsz instead */
	ute_shrink(ctx, sluz);
	/* act as though we don't have a slut */
	ctx->hdrc->slut_sz = 0U;
	return;
}

static void
load_ftr(utectx_t ctx)
{
/* take the stuff past the last page in CTX and make a slut from it */
	if (UNLIKELY(ctx->fsz <= UTEHDR_MIN_SIZE)) {
		return;
	}

	/* otherwise leap to behind the last tick and
	 * deserialise the look-up table */
	const size_t ftrz = get_ftr_size(ctx);
	const off_t off = get_ftr_off(ctx);
	const int pflags = __pflags(ctx);
	char *ftr;

	if ((ftr = mmap_any(ctx->fd, pflags, MAP_FLUSH, off, ftrz)) != NULL) {
		/* deserialise the footer */
		;
		munmap_any(ftr, off, ftrz);
	}

	/* real shrink is too dangerous, just adapt fsz instead */
	ute_shrink(ctx, ftrz);
	/* act as though we don't have a footer */
	ctx->hdrc->ftr_sz = 0U;
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
		/* load the footer, then the slut
		 * must be in this order because they shrink the file */
		load_ftr(res);
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
	/* check for anon */
	if ((oflags & UO_ANON)) {
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
ute_free(utectx_t ctx)
{
	/* ... and finalise */
	free_slut(ctx->slut);
	/* finish our slut session */
	fini_slut();

	/* close the tpc */
	free_tpc(ctx->tpc);
	/* finish our tpc session */
	fini_tpc();
	/* finalise ftr */
	free_ftr(ctx);

	/* now proceed to closing and finalising */
	close_hdr(ctx);
	ute_fini(ctx);
	close(ctx->fd);
	ctx->fd = -1;
	free(ctx);
	return;
}

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
	if (ctx->hdrc->flags & UTEHDR_FLAG_COMPRESSED &&
	    ctx->hdrc->flags & UTEHDR_FLAG_DIRTY) {
		/* final compression */
		lzma_comp(ctx);
	}
	/* serialise the slut */
	flush_slut(ctx);
	/* serialise the footer */
	flush_ftr(ctx);
	/* serialise the cached header */
	flush_hdr(ctx);

	/* just destroy the rest */
	ute_free(ctx);
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
	clone_slut(tgt->slut, src->slut);
	return;
}

void
ute_empty_slut(utectx_t ctx)
{
	/* free existing sluts */
	free_slut(ctx->slut);
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
	size_t np = ute_npages(ctx);
	size_t res;

	/* each page has UTE_BLKSZ ticks */
	res = np * UTE_BLKSZ;
	/* if there are non-flushed ticks, consider them */
	if (tpc_active_p(ctx->tpc)) {
		res += ctx->tpc->sk.si;
	}
	return res;
}

size_t
ute_nsyms(utectx_t ctx)
{
/* return the number of symbols tracked in the ute file */
	size_t nsyms = ctx->hdrc->slut_nsyms;
	if (UNLIKELY(nsyms == 0)) {
		/* try the slut itself */
		nsyms = ctx->slut->nsyms;
	}
	return nsyms;
}

size_t
ute_npages(utectx_t ctx)
{
/* Return the number of tick pages.
 * The first tick page always contains the header. */
	size_t res;
	size_t hdrz;

	if (LIKELY((res = ctx->npages) > 0)) {
		;
	} else if (ctx->hdrc->flags & UTEHDR_FLAG_COMPRESSED &&
		   ctx->ftr != NULL) {
		/* just use the footer info */
		const struct uteftr_cell_s *cells = ctx->ftr->c;

		UDEBUGvv("using footer info\n");
		for (size_t i = 0; i < ctx->ftr->z / sizeof(*cells); i++) {
			if (cells[i].foff == 0) {
				ctx->npages = res = i;
				break;
			}
		}

	} else if (ctx->hdrc->flags & UTEHDR_FLAG_COMPRESSED) {
		/* compression and no footer, i feel terrible */
		const size_t tz = sizeof(*ctx->seek->sp);
		const size_t pgsz = UTE_BLKSZ * tz;
		const size_t probe_z = 32U;
		off_t try = ute_hdrz(ctx);

		UDEBUGvv("try %zd  fsz %zu\n", try, ctx->fsz);
		for (res = 0; (size_t)try < ctx->fsz; res++) {
			void *p = mmap_any(
				ctx->fd, PROT_READ, MAP_SHARED, try, probe_z);
			off_t otry = try;
			size_t len;

			if (UNLIKELY(p == NULL)) {
				try = -1;
			} else if ((len = page_compressed_p(p))) {
				try += len;
			} else if (res) {
				/* page (>0) was not compressed? */
				try += pgsz;
			} else {
				/* page was not compressed? */
				try += pgsz - otry;
			}
			munmap_any(p, otry, probe_z);
			UDEBUGvv("try %zd  fsz %zu\n", try, ctx->fsz);
			/* cache this in FTR slot */
			add_ftr(ctx, res, (struct uteftr_cell_s){
					otry, try - otry, (try - otry) / tz
						});
		}
		/* cache this? */
		ctx->npages = res;

	} else if (ctx->fsz <= (hdrz = ute_hdrz(ctx))) {
		ctx->npages = res = 0U;

	} else {
		/* GUESS is the file size expressed in ticks */
		size_t guess;

		guess = ctx->fsz - hdrz;
		guess /= sizeof(*ctx->seek->sp);
		res = guess / UTE_BLKSZ + (guess % UTE_BLKSZ ? 1 : 0);
		/* cache this? */
		ctx->npages = res;
	}
	return res;
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

void
ute_set_fn(utectx_t ctx, const char *fn)
{
	/* some lusers might want to fool us */
	if (UNLIKELY(ctx->fname == fn)) {
		return;
	}
	rename(ctx->fname, fn);
	free(ctx->fname);
	ctx->fname = strdup(fn);
	return;
}

ute_ver_t
ute_version(utectx_t ctx)
{
/* return the number of symbols tracked in the ute file */
	return utehdr_version(ctx->hdrc);
}

ute_end_t
ute_endianness(utectx_t ctx)
{
/* return the number of symbols tracked in the ute file */
	return utehdr_endianness(ctx->hdrc);
}

int
ute_check_endianness(utectx_t ctx)
{
/* just fall back to utehdr.h solution */
	return utehdr_check_endianness(ctx->hdrc);
}

void
ute_set_endianness(utectx_t ctx, ute_end_t en)
{
/* return the number of symbols tracked in the ute file */
	utehdr_set_endianness(ctx->hdrc, en);
	return;
}

/* utefile.c ends here */
