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

#if !defined SIDX_T
typedef size_t sidx_t;
# define SIDX_T
#endif	/* !SIDX_T */
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif	/* !LIKELY */

typedef struct utetpc_s *utetpc_t;
typedef struct uteseek_s *uteseek_t;

struct utetpc_s {
	char *tp;
	sidx_t tidx;
	size_t tpsz;
	/* size of one tick */
	uint64_t tsz:8;
	uint64_t flags:8;
	/* this used to indicate the last seen value in the tick page
	 * however we only have to guarantee that ALL values in the
	 * current tick page cache are larger than the largest of the
	 * written pages. */
	uint64_t last:48;
	uint64_t lvtd;
} __attribute__((packed));

#define TPC_FL_UNSORTED		0x01
#define TPC_FL_NEEDMRG		0x02

static inline bool
tpc_active_p(utetpc_t tpc)
{
	return tpc->tpsz > 0;
}

static inline bool
tpc_full_p(utetpc_t tpc)
{
	return tpc->tidx >= tpc->tpsz;
}

static inline bool
tpc_sorted_p(utetpc_t tpc)
{
	return (tpc->flags & TPC_FL_UNSORTED) == 0;
}

/**
 * Return if the cached page needs mergen with any of the written pages.
 * If this flag is set a total sort is inevitable. */
static inline bool
tpc_needmrg_p(utetpc_t tpc)
{
	return (tpc->flags & TPC_FL_NEEDMRG) != 0;
}

DECLF void make_tpc(utetpc_t tpc, size_t nticks, size_t tsz);
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

/* (de)initialiser */
DECLF void init_tpc(void);
DECLF void fini_tpc(void);

/**
 * Return the number of bytes in the page cache TPC. */
static inline size_t
tpc_size(utetpc_t tpc)
{
	return tpc->tidx;
}

/**
 * Return the number of ticks in the page cache TPC. */
static inline size_t
tpc_nticks(utetpc_t tpc)
{
	return tpc_size(tpc) / tpc->tsz;
}

/**
 * Return the maximum number of bytes to fit in the page cache TPC. */
static inline size_t
tpc_max_size(utetpc_t tpc)
{
	return tpc->tpsz;
}

/**
 * Return the maximum number of ticks that can be stored in the TPC. */
static inline size_t
tpc_max_nticks(utetpc_t tpc)
{
	return tpc_max_size(tpc) / tpc->tsz;
}

static inline uint64_t
tick_sortkey(scom_t t)
{
	int64_t s = scom_thdr_sec(t);
	uint64_t ms = scom_thdr_msec(t);
	uint64_t res = s << 10;
	if (UNLIKELY(ms > SCOM_MSEC_VALI)) {
		return res;
	}
	return res | ms;
}

static inline scom_t
tpc_last_scom(utetpc_t tpc)
{
	scom_t res = (void*)((char*)tpc->tp + tpc_size(tpc) - tpc->tsz);
	return res;
}

/* like tpc_last_scom() but for random access */
static inline scom_t
tpc_get_scom(utetpc_t tpc, sidx_t i)
{
	scom_t res;

	if (UNLIKELY(i > tpc_size(tpc) / tpc->tsz)) {
		return NULL;
	}
	res = (void*)((char*)tpc->tp + i * tpc->tsz);
	return res;
}

#endif	/* INCLUDED_utetpc_h_ */
