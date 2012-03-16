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
#endif	/* !UNLIKELY */
#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif	/* !LIKELY */

typedef struct utetpc_s *utetpc_t;
typedef struct uteseek_s *uteseek_t;

#if defined _Generic
/* C11 only, fucking gcc hurry up! */
# define STRUCT_ILOG2B(x)			\
	_Generic((x),				\
		 default: 1,			\
		 struct sndwch_s 4,		\
		)
#else  /* _Generic */
# define STRUCT_ILOG2B(x)			\
	/* we know it's sndwch */		\
	4
#endif	/* _Generic */

struct uteseek_s {
	/** index into data (in sandwiches) */
	sidx_t si;
	union {
		/** total alloc'd size of the page (in bytes) */
		size_t sz;
		/** size-fiddled indicator, this is to reflect
		 * that we mmap()ed more space than SZ says but then
		 * had to rewind it to not expose naught padding ticks
		 * at the end of the page
		 * this is possible because all data should be aligned
		 * to sndwch_t size anyway */
		size_t rewound:STRUCT_ILOG2B(struct sndwch_s);
	};
	/** the actual page data */
	struct sndwch_s *sp;
	/** page we're on */
	uint32_t pg;
	/** general page flags */
	uint32_t fl;
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
};

#define TPC_FL_UNSORTED		0x01
#define TPC_FL_NEEDMRG		0x02

static inline __attribute__((pure)) bool
seek_rewound_p(uteseek_t sk)
{
	return sk->rewound != 0;
}

static inline size_t
seek_size(uteseek_t sk)
{
	return LIKELY(!seek_rewound_p(sk) ?
		      /* no fiddling needed */
		      sk->sz :
		      /* we can unwind max 15 ticks! */
		      sk->sz + sk->rewound * sizeof(*sk->sp));
}

static inline void
seek_rewind(uteseek_t sk, size_t nticks)
{
/* consider SK rewound by NTICKS ticks */
	sk->sz -= nticks * sizeof(*sk->sp);
	sk->rewound = nticks;
	return;
}

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

static inline bool
tpc_can_hold_p(utetpc_t tpc, size_t nt)
{
/* whether there's space for NT more sandwiches in TPC */
	return tpc->sk.si + nt <= tpc->sk.sz / sizeof(*tpc->sk.sp);
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

/**
 * Add a tick sandwich T to TPC that consists of NT sandwiches */
DECLF void tpc_add(utetpc_t tpc, scom_t t, size_t nt);

/* temporary */
DECLF void seek_sort(uteseek_t);
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
/* account for rewind bits */
	size_t nrw = tpc->sk.rewound;
	return (tpc->sk.si + nrw) * sizeof(*tpc->sk.sp);
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

/**
 * Return the first tick in SK. */
static inline scom_t
seek_first_scom(uteseek_t sk)
{
	if (UNLIKELY(!AS_SCOM(sk->sp)->u)) {
		return NULL;
	}
	return AS_SCOM(sk->sp);
}

/**
 * Return the last tick in a sought page. */
DECLF scom_t seek_last_scom(uteseek_t sk);

/**
 * Return the currently sought-at scom in SK. */
static inline scom_t
seek_get_scom(uteseek_t sk)
{
	if (UNLIKELY(sk->si * sizeof(*sk->sp) >= sk->sz)) {
		return NULL;
	}
	return AS_SCOM(sk->sp + sk->si);
}

/* isn't that too high level? */
/**
 * Perform a tilman-compression on SK and write its result to TGT.
 * That is merge mutually exclusive neighbouring ticks into one when
 * tick types allow for that.
 * Return the number of compressed ticks.
 * This compression is in-situ if TGT == SK. */
DECLF sidx_t tilman_1comp(uteseek_t tgt, uteseek_t sk);

#endif	/* INCLUDED_utetpc_h_ */
