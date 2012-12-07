/** half-decent trick to type punning
 *
 * Copyright (C) 2009-2012 Sebastian Freundt
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

#include "nifty.h"

#if defined DEBUG_FLAG
# include <stdio.h>
# define UTE_DEBUG(args...)	fprintf(stderr, "[ute/file] " args)
#else
# define UTE_DEBUG(args...)
#endif	/* DEBUG_FLAG */

typedef struct utectx_s *utectx_t;

/* goodness */
#include "utefile.h"
#include "utehdr.h"
#include "utetpc.h"
#include "uteslut.h"

struct utectx_s {
	/** file descriptor we're banging on about */
	int fd;
	/* file size */
	size_t fsz;
	/* seek */
	struct uteseek_s seek[1];
	/* header cache */
	struct utehdr2_s hdrc[1];
	/* we can't keep the const promise, so ... */
	struct utehdr2_s *hdrp;
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
	ssize_t slut_sz;
	struct uteslut_s slut[1];

	/* footer, contains page offsets et al */
	size_t ftr_sz;
	void *ftr;
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

/* add a whole bunch of ticks */
extern void ute_add_ticks(utectx_t ctx, const void *p, size_t nticks);

/**
 * Compress (whatever that means) BSZ bytes in BUF. */
extern ssize_t ute_encode(void **tgt, const void *buf, const size_t bsz);

/**
 * Decompress (whatever that means) BSZ bytes in BUF. */
extern ssize_t ute_decode(void **tgt, const void *buf, const size_t bsz);

/**
 * Return the number of tick pages in CTX. */
extern size_t ute_npages(utectx_t ctx);


/* inlines */
static inline uint32_t
page_of_index(utectx_t ctx, sidx_t i)
{
/* Return the page where the tick with index I is to be found. */
	i += sizeof(*ctx->hdrp) / sizeof(*ctx->seek->sp);
	return (uint32_t)(i / UTE_BLKSZ);
}

static inline uint32_t
offset_of_index(utectx_t ctx, sidx_t i)
{
/* Return the offset of the I-th tick in its page. */
	i += sizeof(*ctx->hdrp) / sizeof(*ctx->seek->sp);
	return (uint32_t)(i % UTE_BLKSZ);
}

static inline size_t
page_offset(utectx_t ctx, uint32_t page)
{
/* Return the absolute file offset of the PAGE-th page in CTX. */
	return page * UTE_BLKSZ * sizeof(*ctx->seek->sp);
}

static inline bool
index_past_eof_p(utectx_t ctx, sidx_t i)
{
/* Return true if I has no allocated space in CTX */
	/* calculations in bytes */
	size_t tot_off = i * sizeof(*ctx->seek->sp) + sizeof(*ctx->hdrp);
	size_t tot_tpc = tpc_byte_size(ctx->tpc);
	/* assume file is trunc'd to last settled page */
	return tot_off >= ctx->fsz + tot_tpc;
}

static inline bool
index_in_tpc_space_p(utectx_t ctx, sidx_t i)
{
/* Return true if I is not on the disk but in the tpc hold. */
	/* calculations in bytes */
	size_t tot_off = i * sizeof(*ctx->seek->sp) + sizeof(*ctx->hdrp);
	size_t tot_tpc = tpc_byte_size(ctx->tpc);
	/* assume file is trunc'd to last settled page */
	return tot_off >= ctx->fsz && tot_off < ctx->fsz + tot_tpc;
}

static inline bool
index_in_seek_page_p(utectx_t ctx, sidx_t i)
{
	return ctx->seek->pg == page_of_index(ctx, i);
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


/* shaping the file */
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

static inline bool
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

#endif	/* INCLUDED_utefile_private_h_ */
