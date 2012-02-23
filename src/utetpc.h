/*** utetpc.h -- tick pages
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

#if !defined INCLUDED_utetpc_h_
#define INCLUDED_utetpc_h_

#include <stdint.h>
#include "scommon.h"

#undef DECLF
#undef DEFUN
#if defined STATIC_GUTS
# define DEFUN	static
# define DECLF	static
#else  /* !STATIC_GUTS */
# define DEFUN
# define DECLF	extern
#endif	/* STATIC_GUTS */

#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif	/* !LIKELY */

typedef struct utetpc_s *utetpc_t;
typedef struct uteseek_s *uteseek_t;

struct uteseek_s {
	/** index into data (in sandwiches) */
	sidx_t si;
	/** total alloc'd size of the page (in bytes) */
	size_t sz;
	/** the actual page data */
	struct sndwch_s *sp;
	/** page we're on */
	sidx_t pg:48;
	/** general page flags */
	uint64_t fl:8;
};

struct utetpc_s {
	struct uteseek_s sk;
	/* this used to indicate the last seen value in the tick page
	 * however we only have to guarantee that ALL values in the
	 * current tick page cache are larger than the largest of the
	 * written pages. */
	uint64_t last;
	/** any key must be at least this, after the instantiation of a
	 * tpc this will point to the largest key that has been flushed */
	uint64_t least;
} __attribute__((packed));

#define TPC_FL_UNSORTED		0x01
#define TPC_FL_NEEDMRG		0x02

static inline bool
tpc_active_p(utetpc_t tpc)
{
	return tpc->sk.sz > 0;
}

static inline bool
tpc_has_ticks_p(utetpc_t tpc)
{
	return tpc->sk.si > 0;
}

static inline bool
tpc_full_p(utetpc_t tpc)
{
	return tpc->sk.si >= tpc->sk.sz / sizeof(*tpc->sk.sp);
}

/**
 * Return non-false if all ticks in the page are sorted. */
static inline bool
tpc_sorted_p(utetpc_t tpc)
{
	return (tpc->sk.fl & TPC_FL_UNSORTED) == 0;
}

static inline void
unset_tpc_unsorted(utetpc_t tpc)
{
	tpc->sk.fl &= ~TPC_FL_UNSORTED;
	return;
}

/**
 * Return if the cached page needs mergen with any of the written pages.
 * If this flag is set a total sort is inevitable. */
static inline bool
tpc_needmrg_p(utetpc_t tpc)
{
	return (tpc->sk.fl & TPC_FL_NEEDMRG) != 0;
}

static inline void
unset_tpc_needmrg(utetpc_t tpc)
{
	tpc->sk.fl &= ~TPC_FL_NEEDMRG;
	return;
}


/* public funs */
DECLF void make_tpc(utetpc_t tpc, size_t nsndwchs);
DECLF void free_tpc(utetpc_t tpc);
/**
 * Clear the current page cache. */
DECLF void clear_tpc(utetpc_t tpc);
DECLF void tpc_add_tick(utetpc_t tpc, scom_t t, size_t tsz);

/* temporary */
DECLF void tpc_sort(utetpc_t);

/**
 * Merge ticks from SRC and SWP into TGT and leave left-overs in SWP. */
DECLF void merge_2tpc(uteseek_t tgt, uteseek_t src, utetpc_t swp);

/**
 * In SK seek to the first tick that's >= KEY, or return NULL. */
DECLF scom_t seek_key(uteseek_t sk, scidx_t key);

/* (de)initialiser */
DECLF void init_tpc(void);
DECLF void fini_tpc(void);

/**
 * Return the number of bytes in the page cache TPC. */
static inline size_t
tpc_byte_size(utetpc_t tpc)
{
	return tpc->sk.si * sizeof(*tpc->sk.sp);
}

/**
 * Return the maximum number of bytes to fit in the page cache TPC. */
static inline size_t
tpc_max_size(utetpc_t tpc)
{
	return tpc->sk.sz;
}

/**
 * Return the first tick in TPC. */
static inline scom_t
tpc_first_scom(utetpc_t tpc)
{
	if (UNLIKELY(!tpc_has_ticks_p(tpc))) {
		return NULL;
	}
	return AS_SCOM(tpc->sk.sp);
}

/**
 * Return the last tick in TPC. */
DECLF scom_t tpc_last_scom(utetpc_t tpc);

/* like tpc_last_scom() but for random access */
static inline scom_t
tpc_get_scom(utetpc_t tpc, sidx_t i)
{
	if (UNLIKELY(i > tpc->sk.si)) {
		return NULL;
	}
	return AS_SCOM(tpc->sk.sp + i);
}

#endif	/* INCLUDED_utetpc_h_ */
