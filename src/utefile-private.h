/** half-decent trick to type punning
 *
 * Copyright (C) 2009 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <sebastian.freundt@ga-group.nl>
 *
 * This file is part of unserding.
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

#if defined DEBUG_FLAG
# include <stdio.h>
# define UTE_DEBUG(args...)	fprintf(stderr, "[ute/file] " args)
#else
# define UTE_DEBUG(args...)
#endif	/* DEBUG_FLAG */

typedef struct utectx_s *utectx_t;
#if !defined SIDX_T
typedef long int sidx_t;
# define SIDX_T
#endif	/* !SIDX_T */

#if !defined UNUSED
# define UNUSED(_x)	__attribute__((unused)) _x
#endif	/* !UNUSED */
#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif	/* !LIKELY */
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif	/* !LIKELY */

#define UTE_BLKSZ(ctx)	(64 * (ctx)->pgsz)

/* goodness */
#include "utefile.h"
#include "utehdr.h"
#include "utetpc.h"
#include "uteslut.h"

typedef struct uteseek_s *uteseek_t;

struct uteseek_s {
	sidx_t idx;
	size_t mpsz;
	char *data;
	/* size of one tick */
	uint32_t tsz;
	/* page we're on */
	uint32_t page;
};

struct utectx_s {
	/** file descriptor we're banging on about */
	int fd;
	/* file size */
	ssize_t fsz;
	/* seek */
	struct uteseek_s seek[1];
	/* header cache */
	struct utehdr2_s hdrc[1];
	utehdr2_t hdrp;
	/* tick pages cache */
	struct utetpc_s tpc[1];
	/* page size */
	uint32_t pgsz;
	/* whether pages are unsorted et al. */
	uint16_t flags;
	/* file access and open flags */
	uint16_t oflags;
	/* copy of the file name we desire */
	char *fname;
	/* largest value to date */
	uint64_t lvtd;

	/* symbol table, idx->sym and sym->idx */
	ssize_t slut_sz;
	struct uteslut_s slut[1];
};

/**
 * Unsorted flag, set if a page has been flushed that needed sorting.
 * With this flag we decide to resort the whole file before closing. */
#define UTE_FL_UNSORTED		0x01


/* private api */
extern void seek_page(uteseek_t sk, utectx_t ctx, uint32_t pg);
extern void flush_seek(uteseek_t sk);


/* that we need this indicates poor design */
#include "sl1t.h"

/* inlines */
static inline size_t
ute_npages(utectx_t ctx)
{
/* oh oh oh */
	/* metadata size */
	size_t aux_sz = sizeof(struct utehdr2_s) + ctx->slut_sz;
	size_t nt = (ctx->fsz - aux_sz) / sizeof(struct sl1t_s);
	return nt / UTE_BLKSZ(ctx) + (nt % UTE_BLKSZ(ctx) ? 1 : 0);
}

static inline size_t
min_size_t(size_t x, size_t y)
{
	return x < y ? x : y;
}

static inline uint32_t
page_of_index(utectx_t ctx, sidx_t i)
{
/* return the page where the tick with index I is to be found */
/* oh oh oh */
	size_t bsz = UTE_BLKSZ(ctx);
	uint32_t pg = i / bsz;
	return pg;
}

static inline uint32_t
offset_of_index(utectx_t ctx, sidx_t i)
{
/* return the offset of the I-th tick in its page. */
/* oh oh oh */
	size_t bsz = UTE_BLKSZ(ctx);
	uint32_t o = i % bsz;
	return o;
}

static inline size_t
page_size(utectx_t ctx, uint32_t page)
{
/* return the size of the PAGE-th page in CTX. */
/* oh oh oh */
	size_t bsz = UTE_BLKSZ(ctx);
	size_t tsz = sizeof(struct sl1t_s);
	size_t psz = bsz * tsz;
	size_t tot = sizeof(struct utehdr2_s) + page * psz;
	return tot + psz <= ctx->fsz ? psz : ctx->fsz - tot;
}

static inline size_t
page_offset(utectx_t ctx, uint32_t page)
{
/* return the absolute file offset of the PAGE-th page in CTX. */
/* oh oh oh */
	size_t bsz = UTE_BLKSZ(ctx);
	size_t tsz = sizeof(struct sl1t_s);
	size_t psz = bsz * tsz;
	size_t tot = sizeof(struct utehdr2_s) + page * psz;
	return tot;
}

static inline bool
index_past_eof_p(utectx_t ctx, sidx_t i)
{
/* could do this in terms of page_size() */
/* oh oh oh */
	size_t bsz = UTE_BLKSZ(ctx);
	size_t tsz = sizeof(struct sl1t_s);
	uint32_t p = page_of_index(ctx, i);
	uint32_t o = offset_of_index(ctx, i);
	size_t tot = sizeof(struct utehdr2_s) + p * bsz * tsz + o;
	return tot >= ctx->fsz;
}

static inline sidx_t
index_to_tpc_index(utectx_t ctx, sidx_t i)
{
/* we just assume that tpc will never have more than UTE_BLKSZ ticks */
	size_t aux_sz = sizeof(struct utehdr2_s) + ctx->slut_sz;
	sidx_t nticks = (ctx->fsz - aux_sz) / sizeof(struct sl1t_s);
	return i - nticks;
}

static inline bool
index_in_seek_page_p(utectx_t ctx, sidx_t i)
{
	return ctx->seek->page == page_of_index(ctx, i);
}

static inline bool
ute_sorted_p(utectx_t ctx)
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

#endif	/* INCLUDED_utefile_private_h_ */
