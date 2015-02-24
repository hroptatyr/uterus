/** half-decent trick to type punning
 *
 * Copyright (C) 2009-2015 Sebastian Freundt
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

#if !defined INCLUDED_utefile_private_h_
#define INCLUDED_utefile_private_h_

/* for ssize_t */
#include <unistd.h>
#include <stdbool.h>
#include "nifty.h"

#if defined DEBUG_FLAG
# include <stdio.h>
# define UTE_DEBUG(args...)	fprintf(stderr, "[ute/file] " args)
#else
# define UTE_DEBUG(args...)
#endif	/* DEBUG_FLAG */

typedef struct utectx_s *utectx_t;
typedef const struct utectx_s *const_utectx_t;

/* goodness */
#include "utefile.h"
#include "utehdr.h"
#include "utetpc.h"
#include "uteslut.h"
#include "scommon.h"

struct __gen_s {
	union scom_thdr_u scom[1];
	uint32_t v[14];
};
#define AS_GEN(x)	((const struct __gen_s*)(x))

struct utectx_s {
	/** file descriptor we're banging on about */
	int fd;
	/* file size */
	size_t fsz;
	/* seek */
	struct uteseek_s seek[1];
	/* header cache */
	struct utehdr2_s hdrc[1];
	/* the header on the disk */
	struct utehdr2_s *restrict hdrp;
	/* tick pages cache */
	struct utetpc_s tpc[1];
	/* page size */
	uint32_t pad;
	/* whether pages are unsorted et al. */
	uint16_t flags;
	/* file access and open flags */
	uint16_t oflags;
	/* copy of the file name we desire */
	char *fname;
	/* largest value to date */
	uint64_t lvtd;

	/* symbol table, idx->sym and sym->idx */
	struct uteslut_s slut[1];
	/* real slut size in bytes (including alignment) */
	size_t sluz;

	/* number of pages, native endianness */
	size_t npages;
	/* payload offset, native endianness */
	size_t ploff;

	/* footer, contains page offsets et al */
	struct {
		size_t z;
		struct uteftr_cell_s *c;
	} ftr[1];

	/* iter magic */
	struct {
		unsigned int iter_st;
		sidx_t iter_si;
		struct __gen_s iter_tmp;
	};
};

/**
 * Unsorted flag, set if a page has been flushed that needed sorting.
 * With this flag we decide to resort the whole file before closing. */
#define UTE_FL_UNSORTED		0x01


/* private api */
extern int seek_page(uteseek_t sk, utectx_t ctx, uint32_t pg);
extern void flush_seek(uteseek_t sk);
extern int make_page(uteseek_t sk, utectx_t ctx, uint32_t pg);
extern int clone_page(uteseek_t sk, utectx_t ctx, uteseek_t src);

extern void bump_header(struct utehdr2_s *hdr);

/**
 * Add the tick T to the ute context specified by CTX, using the header in H. */
extern void ute_add_tick_as(utectx_t ctx, scom_t t, scom_t h);

/**
 * Compress (whatever that means) BSZ bytes in BUF. */
extern ssize_t
ute_encode(void *tgt[static 1], const void *buf, const size_t bsz);

/**
 * Decompress (whatever that means) BSZ bytes in BUF. */
extern ssize_t
ute_decode(void *tgt[static 1], const void *buf, const size_t bsz);

/* compression level to use for ute_encode() */
extern uint32_t ute_encode_clevel;

/**
 * Return the number of tick pages in CTX. */
extern size_t ute_npages(utectx_t ctx);


/* inlines */
static inline __attribute__((pure)) size_t
ute_hdrz(const_utectx_t ctx)
{
/* return the size of CTX's header on disk in bytes */
	if (LIKELY(ctx->ploff)) {
		return (size_t)ctx->ploff;
	}
	return UTEHDR_MAX_SIZE;
}

static inline __attribute__((pure)) size_t
ute_hdrzt(const_utectx_t ctx)
{
/* like ute_hdrz() but return the header size in ticks instead of bytes */
	return ute_hdrz(ctx) / sizeof(*ctx->seek->sp);
}

static inline __attribute__((pure)) uint32_t
page_of_index(const_utectx_t ctx, sidx_t i)
{
/* Return the page where the tick with index I is to be found. */
	i += ute_hdrzt(ctx);
	return (uint32_t)(i / UTE_BLKSZ);
}

static inline __attribute__((pure)) uint32_t
offset_of_index(const_utectx_t ctx, sidx_t i)
{
/* Return the offset of the I-th tick in its page. */
	const size_t blk = UTE_BLKSZ;
	const size_t hdrt = ute_hdrzt(ctx);

	i += hdrt;
	if (LIKELY(i / blk)) {
		return (uint32_t)(i % blk);
	} else {
		return i - hdrt;
	}
}

static inline __attribute__((pure)) size_t
page_offset(const_utectx_t ctx, uint32_t page)
{
/* Return the absolute file offset of the PAGE-th page in CTX. */
	const size_t cand = page * UTE_BLKSZ * sizeof(*ctx->seek->sp);
	return page ? cand : ute_hdrz(ctx);
}

static inline __attribute__((pure)) size_t
page_size(const_utectx_t ctx, uint32_t page)
{
/* Return the absolute (memory) size of the PAGE-th page in CTX in bytes. */
	const size_t cand = UTE_BLKSZ * sizeof(*ctx->seek->sp);
	return page ? cand : cand - ute_hdrz(ctx);
}

static inline __attribute__((pure)) size_t
page_sizet(const_utectx_t ctx, uint32_t page)
{
/* Return the absolute (memory) size of the PAGE-th page in CTX in ticks. */
	return page_size(ctx, page) / sizeof(*ctx->seek->sp);
}

static inline bool
ute_sorted_p(const_utectx_t ctx)
{
	return (ctx->flags & UTE_FL_UNSORTED) == 0;
}

static inline void
ute_set_unsorted(utectx_t ctx)
{
	ctx->flags |= UTE_FL_UNSORTED;
	return;
}

static inline void
ute_unset_unsorted(utectx_t ctx)
{
	ctx->flags &= ~UTE_FL_UNSORTED;
	return;
}

static inline void
ute_encode_free(void)
{
	void *unused;
	(void)ute_encode(&unused, NULL, 0UL);
	return;
}

static inline void
ute_decode_free(void)
{
	void *unused;
	(void)ute_decode(&unused, NULL, 0UL);
	return;
}

#endif	/* INCLUDED_utefile_private_h_ */
