/*** utetpc.c -- tick pages
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

#define UTETPC_C
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include "nifty.h"
#include "mem.h"
#include "utetpc.h"

/* we're just as good as rudi, aren't we? */
#if defined DEBUG_FLAG
# include <assert.h>
# include <stdio.h>
# define UDEBUG(args...)	fprintf(stderr, args)
# define scom_tick_size		__local_scom_tick_size
# define scom_byte_size		__local_scom_byte_size
# define MAYBE_NOINLINE		__attribute__((noinline))
#else  /* !DEBUG_FLAG */
# define UDEBUG(args...)
# define assert(args...)
# define MAYBE_NOINLINE
#endif	/* DEBUG_FLAG */
/* for serious debugging */
#if !defined UDEBUGvv
# define UDEBUGvv(args...)
#endif	/* !UDEBUGvv */

typedef struct perm_idx_s *perm_idx_t;

struct perm_idx_s {
	uint64_t skey;
	sidx_t si;
};

static inline void
put_pi(perm_idx_t p, scom_t t, sidx_t i)
{
	p->skey = t->u;
	p->si = i;
	return;
}

#define pi_skey(x)	((x)->skey)

static inline sidx_t
pi_sidx(perm_idx_t p)
{
	return p->si;
}


/* why aren't these in the header file? */
static inline void
set_tpc_unsorted(utetpc_t tpc)
{
	tpc->sk.fl |= TPC_FL_UNSORTED;
	return;
}

static inline void
set_tpc_needmrg(utetpc_t tpc)
{
	tpc->sk.fl |= TPC_FL_NEEDMRG;
	return;
}

static inline uint64_t
tick_sortkey(scom_t t)
{
	/* using scom v0.2 now */
	return t->u;
}

/* one more for debugging */
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

/* calloc like signature */
DEFUN void
make_tpc(utetpc_t tpc, size_t nsndwchs)
{
	/* this should really be the next UTE_BLKSZ multiple of NSNDWCHS */
	size_t sz = UTE_BLKSZ * sizeof(*tpc->sk.sp);

	tpc->sk.sp = mmap(NULL, sz, PROT_MEM, MAP_MEM, -1, 0);
	if (LIKELY(tpc->sk.sp != MAP_FAILED)) {
		tpc->sk.szrw = sz;
		tpc->sk.si = 0;
		/* we (ab)use the pg slot for the sandwich count */
		tpc->sk.pg = nsndwchs;
	} else {
		tpc->sk.szrw = 0;
		tpc->sk.si = -1;
		tpc->sk.cap = 0;
	}
	return;
}

DEFUN void
free_tpc(utetpc_t tpc)
{
	if (tpc_active_p(tpc)) {
		/* seek points to something -> munmap first
		 * we should probably determine the size of the tpc using the
		 * CAP slot and round it to the next UTE_BLKSZ multiple */
		munmap(tpc->sk.sp, UTE_BLKSZ);
	}
	tpc->sk.si = -1;
	tpc->sk.szrw = 0;
	tpc->sk.sp = NULL;
	tpc->sk.cap = 0;
	return;
}

DEFUN void
clear_tpc(utetpc_t tpc)
{
	if (tpc_active_p(tpc)) {
		tpc->sk.si = 0;
		/* reset size to full tpc size again */
		tpc->sk.szrw += tpc_rewound(tpc) * sizeof(*tpc->sk.sp);
		tpc->sk.szrw = tpc_set_rewound(tpc, 0);
	}
	return;
}

DEFUN void
tpc_add(utetpc_t tpc, scom_t t, size_t nt)
{
/* supports variadic ticks */
	uint64_t skey = tick_sortkey(t);

	if (UNLIKELY(tpc_full_p(tpc) || !tpc_can_hold_p(tpc, nt) || !skey)) {
#if defined DEBUG_FLAG
		UDEBUG("not adding tick, reason: ");
		if (tpc_full_p(tpc)) {
			UDEBUG(" tpc_full");
		}
		if (!tpc_can_hold_p(tpc, nt)) {
			UDEBUG(" !tpc_can_hold");
		}
		if (!skey) {
			UDEBUG(" skey==0");
		}
		UDEBUG("\n");
#endif	/* DEBUG_FLAG */
		return;
	}
	memcpy(tpc->sk.sp + tpc->sk.si, t, nt * sizeof(*tpc->sk.sp));
	/* just add the total byte size as passed on */
	tpc->sk.si += nt;

	/* maybe mark the whole shebang as unsorted */
	if (UNLIKELY(skey < tpc->last)) {
		set_tpc_unsorted(tpc);
	}
	/* check if the whole file needs sorting (merging) */
	if (UNLIKELY(skey < tpc->least)) {
		/* found a key that's smaller than what has been flushed */
		set_tpc_needmrg(tpc);
		tpc->least = skey;
	}
	tpc->last = skey;
	return;
}


/* sorters */
#include "scommon.h"

#undef ALGN
#define DATA(p, i)	((void*)(((char*)(p)) + (i)))
#define DATCA(p, i)	((const void*)(((const char*)(p)) + (i)))
#define DATD(p, q)	((size_t)(((char*)(p)) - ((char*)(q))))
#define DATCD(p, q)	((size_t)(((const char*)(p)) - ((const char*)(q))))
#define DATDI(p, q, sz)	((size_t)(((char*)(p)) - ((char*)(q))) / (sz))
#define DATI(p, i, sz)	((void*)(((char*)(p)) + (i) * (sz)))
#define DATCI(p, i, sz)	((const void*)(((const char*)(p)) + (i) * (sz)))
#define KEYI(p, i, sz)	(*((int32_t*)DATI(p, i, sz)))
#define ALGN(t, o)	((o / sizeof(t)) * sizeof(t))
#define PERM(a, b, c, d)			\
	(uint8_t)(				\
		(a & 0x3) << 0 |		\
		(b & 0x3) << 2 |		\
		(c & 0x3) << 4 |		\
		(d & 0x3) << 6)
#define PERMI(p, a)				\
	(uint8_t)((p >> (2 * a)) & 0x3)
#define IDXSORT_SIZE	256

/* union based scidx
 * order in the struct part is important as we need this to compare
 * two such things for sorting; go from least significant to most */
/* scidx_t is now part of scommon.h */

/* special version of ilog that works for inputs up to 256 */
static inline size_t __attribute__((const))
__ilog2_ceil(size_t n)
{
	if (n > 128) {
		return 256;
	} else if (n <= 4) {
		return 4;
	} else if (n <= 8) {
		return 8;
	} else if (n <= 16) {
		return 16;
	} else if (n <= 32) {
		return 32;
	} else if (n <= 64) {
		return 64;
	} else {
		return 128;
	}
}

static void
swap_pi(perm_idx_t p1, perm_idx_t p2)
{
	struct perm_idx_s tmp = *p1;
	*p1 = *p2, *p2 = tmp;
}

static uint8_t
pornsort_perm(struct perm_idx_s p[4])
{
	uint8_t perm;

	if (pi_skey(p + 1) < pi_skey(p + 0)) {
		/* swap them right away? */
		swap_pi(p + 0, p + 1);
	}
	if (pi_skey(p + 3) < pi_skey(p + 2)) {
		/* swap them right away? */
		swap_pi(p + 2, p + 3);
	}

	/* bit like AA-sort now, final comparison */
	if (pi_skey(p + 2) < pi_skey(p + 0)) {
		/* c first */
		if (pi_skey(p + 3) < pi_skey(p + 0)) {
			/* d next, then a, then b */
			perm = PERM(2, 3, 0, 1);
		} else {
			/* a next */
			if (pi_skey(p + 3) < pi_skey(p + 1)) {
				/* d next, then b */
				perm = PERM(2, 0, 3, 1);
			} else {
				/* b next, then d */
				perm = PERM(2, 0, 1, 3);
			}
		}
	} else {
		/* a first */
		if (pi_skey(p + 2) < pi_skey(p + 1)) {
			/* c next */
			if (pi_skey(p + 3) < pi_skey(p + 1)) {
				/* d next, then b */
				perm = PERM(0, 2, 3, 1);
			} else {
				/* b next, then d */
				perm = PERM(0, 2, 1, 3);
			}
		} else {
			/* b next, then c, then d */
			perm = PERM(0, 1, 2, 3);
		}
	}
	return perm;
}

/* pornsort of 4 ticks */
static void
pornsort_apply(struct perm_idx_s p[4], uint8_t perm)
{
	switch (perm) {
	case PERM(0, 1, 2, 3):
		/* nothing to do */
		break;

	case PERM(2, 1, 0, 3):
		/* (1,3) */
		swap_pi(p + 0, p + 2);
		break;

	case PERM(3, 1, 2, 0):
		/* (1,4) */
		swap_pi(p + 0, p + 3);
		break;

	case PERM(0, 2, 1, 3):
		/* (2,3) */
		swap_pi(p + 1, p + 2);
		break;

	case PERM(0, 3, 2, 1):
		/* (2,4) */
		swap_pi(p + 1, p + 3);
		break;

	case PERM(1, 0, 2, 3):
		/* (1,2) */
		/* not possible given the above perm generator */
	case PERM(0, 1, 3, 2):
		/* (3,4) */
		/* not possible with the above perm generator */
	case PERM(1, 0, 3, 2):
		/* (1,2)(3,4) */
		/* not possible with the above perm generator */
		abort();
		break;

	case PERM(2, 3, 0, 1):
		/* (1,3)(2,4) */
		swap_pi(p + 0, p + 2);
		swap_pi(p + 1, p + 3);
		break;

	case PERM(3, 2, 1, 0):
		/* (1,4)(2,3) */
		swap_pi(p + 0, p + 3);
		swap_pi(p + 1, p + 2);
		break;

	default: {
		/* 3- and 4-cycles here */
		struct perm_idx_s tmpa[4];

		if (PERMI(perm, 0) != 0) {
			tmpa[0] = p[0];
			p[0] = p[PERMI(perm, 0)];
		}
		if (PERMI(perm, 1) != 1) {
			tmpa[1] = p[1];
			/* PERMI(perm, 1) could be 0 */
			p[1] = PERMI(perm, 1) ? p[PERMI(perm, 1)] : tmpa[0];
		}
		if (PERMI(perm, 2) != 2) {
			tmpa[2] = p[2];
			tmpa[3] = p[3];
			/* all tmpa's are set by now */
			p[2] = tmpa[PERMI(perm, 2)];
		}
		if (PERMI(perm, 3) != 3) {
			/* all tmpa's are set by now */
			p[3] = tmpa[PERMI(perm, 3)];
		}
	}
	}
	/* some assertions */
	assert(pi_skey(p + 0) <= pi_skey(p + 1));
	assert(pi_skey(p + 1) <= pi_skey(p + 2));
	assert(pi_skey(p + 2) <= pi_skey(p + 3));
	return;
}

#define min(x, y)	(x < y ? x : y)

/* not reentrant, fixme */
static perm_idx_t __gidx = MAP_FAILED;
#define SCRATCH_SIZE	(IDXSORT_SIZE * 2 * sizeof(*__gidx))

static void
init_scratch(void)
{
	if (UNLIKELY(__gidx == MAP_FAILED)) {
		__gidx = mmap(NULL, SCRATCH_SIZE, PROT_MEM, MAP_MEM, -1, 0);
	}
	return;
}

/* scratch space where sort keys and indices are stored */
static perm_idx_t
get_scratch(void)
{
/* singleton */
	if (UNLIKELY(__gidx == MAP_FAILED)) {
		init_scratch();
	}
	return __gidx;
}

static void
fini_scratch(void)
{
	if (LIKELY(__gidx != MAP_FAILED)) {
		(void)munmap(__gidx, SCRATCH_SIZE);
		__gidx = MAP_FAILED;
	}
	return;
}

static void MAYBE_NOINLINE
merge_up(perm_idx_t tgt, perm_idx_t src, int step, int max)
{
/* bottom up merge steps, for arrays of scidx's */
	int i1 = 0;
	int i2 = step;
	int ei1 = min(max, step);
	int ei2 = min(max, 2 * step);
	int i = 0;

	for (; i1 < ei1 && i2 < ei2; i++) {
		tgt[i] = pi_skey(src + i1) < pi_skey(src + i2)
			? src[i1++]
			: src[i2++];
	}
	for (; i1 < ei1; i++, i1++) {
		tgt[i] = src[i1];
	}
	for (; i2 < ei2; i++, i2++) {
		tgt[i] = src[i2];
	}

	for (int k = 1; k < i; k++) {
		assert(pi_skey(tgt + k - 1) <= pi_skey(tgt + k));
	}
	return;
}

static void MAYBE_NOINLINE
merge_all(size_t nticks)
{
	perm_idx_t src = get_scratch();
	perm_idx_t tgt = src + IDXSORT_SIZE;

	/* steps 4, 8, 16, 32, 64, 128
	 * we strictly need an even number of rounds so that the final
	 * result is in the first half of gidx */
	for (size_t step = 4; step < nticks; ) {
		size_t s2 = 2 * step;

		for (size_t i = 0; i < nticks; i += s2) {
			merge_up(tgt + i, src + i, step, nticks);
		}
		/* per definitionem we only hand in nticks that are 2 powers
		 * so there's no need to merge_up() any remainders */

		/* swap source and target roles (ring buffer) */
		if (src < tgt) {
			tgt = get_scratch();
			src = tgt + IDXSORT_SIZE;
		} else {
			src = get_scratch();
			tgt = src + IDXSORT_SIZE;
		}
		/* step up */
		step = s2;
	}
	if (UNLIKELY(tgt < src)) {
		/* odd number of rounds, memmove */
		memmove(tgt, src, IDXSORT_SIZE * sizeof(*src));
	}
	return;
}

#if defined DEBUG_FLAG
static size_t nleading_naughts = 0;
#endif	/* DEBUG_FLAG */

static size_t MAYBE_NOINLINE
idxsort(perm_idx_t *pip, sndwch_t sp, sndwch_t ep)
{
/* sort stuff in the range [p, ep) */
	perm_idx_t keys = get_scratch();
	const size_t max = IDXSORT_SIZE;
	size_t nsc = 0;
	size_t nt = 0;
	size_t m_2p = 0;

	for (size_t tsz; nsc < max && sp + nt < ep; nsc++, nt += tsz) {
		scom_t p = AS_SCOM(sp + nt);
		tsz = scom_tick_size(p);

		/* there must be no naught ticks in the map */
		assert(p->u != 0ULL);
		assert(p->u != -1ULL);
		/* produce a mapping SCOM |-> TICK */
		put_pi(keys + nsc, p, nt);
	}
	UDEBUGvv("idxsort on %zu scoms and %zu ticks\n", nsc, nt);
	/* there must be one tick for this to work */
	assert(nt > 0);
	/* and if there's one tick there should be one scom */
	assert(nsc > 0);
	/* compute the next 2-power */
	m_2p = __ilog2_ceil(nsc);
	/* check if m_2p is a 2-power indeed */
	assert((m_2p & (m_2p - 1)) == 0);
	/* ... and a ceil */
	assert(m_2p >= nsc);
	/* fill scp up with naughts */
	memset(keys + nsc, 0, (m_2p - nsc) * sizeof(*keys));

	/* use the local index in scp to do pornsort */
	for (size_t i = 0; i < nsc; i += 4) {
		uint8_t perm = pornsort_perm(keys + i);
		pornsort_apply(keys + i, perm);
	}

	/* merge steps, up to the next 2-power */
	merge_all(m_2p);

	/* keys should be in ascending order now */
	for (size_t i = 1; i < nsc; i++) {
		assert(keys[i].skey >= keys[i - 1].skey);
		assert(keys[i].skey != 0xffffffffffffffff);
	}
	/* last one should contain at least a non-naught key
	 * or else we have been doing this shit for nothing */
	assert(keys[m_2p - 1].skey);

	/* assign results and off we pop */
	*pip = keys;
#if defined DEBUG_FLAG
	nleading_naughts = m_2p - nsc;
	for (size_t i = 0; i < nleading_naughts; i++) {
		assert(keys[i].skey == 0ULL);
		assert(keys[i].si == 0U);
	}
#endif	/* DEBUG_FLAG */
	return nt;
}

static void MAYBE_NOINLINE
collate(void *tgt, const void *src, perm_idx_t pi, size_t nticks)
{
/* collating is the process of putting the satellite data along with
 * the sorted keys again
 * SRC is an array of keys and satellite data and
 * PI is the permutation to apply */
	/* skip 0 idxs first */
	size_t j = 0;

	for (const size_t max = nticks >= 256 ? 256 : __ilog2_ceil(nticks);
	     j < max && pi_skey(pi + j) == 0ULL; j++);

	UDEBUGvv("%zu leading naught ticks (of %zu)\n", j, nticks);

#if defined DEBUG_FLAG
	/* since idxsort used 2-powers and we don't allow 0 skeys
	 * J should be the same as nleading_naughts as set in idxsort() */
	assert(j == nleading_naughts);
#endif	/* DEBUG_FLAG */

	for (size_t i = 0, bsz, tsz; i < nticks; j++, i += tsz) {
		sidx_t idx = pi_sidx(pi + j);
		const void *s = DATCI(src, idx, sizeof(struct sndwch_s));

		tsz = scom_tick_size(s);
		bsz = scom_byte_size(s);

		if (j > 0) {
			assert(pi_skey(pi + j - 1) <= pi_skey(pi + j));
		}
		if (j > 0 && pi_skey(pi + j - 1) != 0ULL) {
			assert(AS_SCOM(DATCI(src, pi_sidx(pi + j - 1),
					     sizeof(struct sndwch_s)))->u <=
			       AS_SCOM(s)->u);
		}

		memcpy(tgt, s, bsz);
		tgt = DATA(tgt, bsz);
	}
	return;
}

static void* MAYBE_NOINLINE
merge_bup(void *tgt, sndwch_t spl, size_t ntl, sndwch_t spr, size_t ntr)
{
/* merge left source SPL and right source SPR into tgt. */
	struct sndwch_s *tp = tgt;
	sndwch_t elp = spl + ntl;
	sndwch_t erp = spr + ntr;

	while (spl < elp && spr < erp) {
		if (spl->key <= spr->key) {
			size_t tsz = scom_tick_size(AS_SCOM(spl));

			/* copy the left tick */
			memcpy(tp, spl, tsz * sizeof(*spl));
			/* step things */
			spl += tsz;
			tp += tsz;
		} else {
			size_t tsz = scom_tick_size(AS_SCOM(spr));

			/* use the right guy */
			memcpy(tp, spr, tsz * sizeof(*spr));
			/* step things */
			spr += tsz;
			tp += tsz;
		}
	}
	/* the end pointers must match exactly, otherwise we copied too much */
	assert(!(spl > elp));
	assert(!(spr > erp));

	if (spl < elp) {
		/* not all left ticks */
		size_t sz = elp - spl;
		memcpy(tp, spl, sz * sizeof(*spl));
		tp += sz;
	} else if (spr < erp) {
		/* right ticks left */
		size_t sz = erp - spr;
		memcpy(tp, spr, sz * sizeof(*spr));
		tp += sz;
	} else {
		;
	}
	return tp;
}

static void MAYBE_NOINLINE
bup_round(void *tgt, const void *src, size_t rsz, uint32_t *offs, size_t noffs)
{
	struct sndwch_s *tp = tgt;
	sndwch_t sp = src;

	for (size_t i = 0, tot; i < noffs; i += rsz, sp += tot, tp += tot) {
		/* get the nticks of the left and right sides */
		uint32_t ntl = offs[i];
		uint32_t ntr = offs[i + rsz / 2];
		/* set up the pointers from src */
		sndwch_t npl = sp;
		sndwch_t npr = sp + ntl;

		/* the actual merge */
		(void)merge_bup(tp, npl, ntl, npr, ntr);
		/* compute new offsets for later rounds */
		tot = ntl + ntr;
		offs[i] = tot;
	}
	return;
}

static bool
scom_lm_p(scom_t t)
{
	uint16_t ttf = scom_thdr_ttf(t);
	return ttf >= SCOM_FLAG_LM && ttf < SCOM_FLAG_L2M;
}

static bool
scom_simple_p(scom_t t)
{
	uint16_t ttf = scom_thdr_ttf(t);
	return ttf < SCOM_FLAG_LM;
}

static sidx_t
algn_tick(uteseek_t sk, sidx_t ix)
{
/* align a tick referenced by sandwich count, this assumes sorted pages */
	const scom_t bop = AS_SCOM(sk->sp);

	if (UNLIKELY(ix == 0)) {
		/* the 0-th tick is aligned trivially */
		return 0;
	}
	/* we take probes left and right of TP, and we check that
	 * - the scom sizes match
	 * - the scom orders match */

	/* first assume everything's alright */
	for (scom_t tp; (tp = AS_SCOM(sk->sp + ix)) > bop; ix--) {
		scom_t p = AS_SCOM(sk->sp + ix - 1);

		if (p->u <= tp->u) {
			/* yay! */
			if (scom_simple_p(p)) {
				return ix;
			}
			/* means, linked mode, go back a bit */
			if (UNLIKELY((p = AS_SCOM(sk->sp + ix - 2)) < bop)) {
				return ix;
			} else if (p->u <= tp->u && scom_lm_p(p)) {
				/* everything still perfect */
				return ix;
			}
		}
	}
	return ix;
}

/* could be public */
static struct sndwch_s*
seek_last_sndwch(uteseek_t sk)
{
	if (UNLIKELY(sk->sp == NULL)) {
		return NULL;
	} else if (UNLIKELY(sk->szrw - seek_rewound(sk) == 0)) {
		return NULL;
	} else {
		const size_t probsz = sizeof(*sk->sp);
		sndwch_t tp = sk->sp + sk->szrw / probsz - 1;
		return sk->sp + algn_tick(sk, tp - sk->sp);
	}
}


/* public funs */
DEFUN scom_t
tpc_last_scom(utetpc_t tpc)
{
	if (UNLIKELY(!tpc_has_ticks_p(tpc))) {
		return NULL;
	}
	return AS_SCOM(tpc->sk.sp + algn_tick(&tpc->sk, tpc->sk.si - 1));
}

DEFUN scom_t
seek_last_scom(uteseek_t sk)
{
	typeof(sk->sp) sp = seek_last_sndwch(sk);
	if (UNLIKELY(sp == NULL)) {
		return NULL;
	}
	return AS_SCOM(sp);
}

DEFUN scom_t
seek_key(uteseek_t sk, scidx_t key)
{
/* use a binary search and also set SK's si accordingly */
	typeof(sk->sp) sp;

	/* try the tail first */
	if (UNLIKELY((sp = seek_last_sndwch(sk)) == NULL)) {
		return NULL;
	} else if (LIKELY(make_scidx(AS_SCOM(sp)).u <= key.u)) {
		return NULL;
	}

	/* binsrch, try the middle */
	for (typeof(sp) bosp = sk->sp,
		     eosp = sk->sp + sk->szrw / sizeof(*sp); bosp < eosp; ) {
		sidx_t diam = (eosp - bosp) / 2;

		sp = sk->sp + algn_tick(sk, (bosp - sk->sp) + diam);
		if (make_scidx(AS_SCOM(sp)).u > key.u) {
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
	/* must be the offending tick, update index and return */
	sk->si = sp - sk->sp;
	assert(sk->si < sk->szrw / sizeof(*sk->sp));
	return AS_SCOM(sp);
}

DEFUN void
merge_2tpc(uteseek_t tgt, uteseek_t src, utetpc_t swp)
{
/* merge stuff from SRC and SWP into TGT, leftovers will be put in SWP */
	void *tp = tgt->sp + tgt->si;
	void *eot = tgt->sp + tgt->szrw / sizeof(*tgt->sp);
	void *sp = src->sp + src->si;
	void *eos = src->sp + src->szrw / sizeof(*tgt->sp);
	void *rp = swp->sk.sp;
	void *eor = swp->sk.sp + swp->sk.si;

	while (tp < eot && sp < eos && rp < eor) {
		if (AS_SCOM(sp)->u <= AS_SCOM(rp)->u) {
			/* copy the src */
			size_t ssz = scom_byte_size(sp);
			memcpy(tp, sp, ssz);
			sp = DATA(sp, ssz);
			tp = DATA(tp, ssz);
		} else {
			/* copy from swap */
			size_t rsz = scom_byte_size(rp);
			memcpy(tp, rp, rsz);
			rp = DATA(rp, rsz);
			tp = DATA(tp, rsz);
		}
	}
	assert(DATD(eos, sp) <= DATD(rp, swp->sk.sp));
	assert(tp == eot);
	/* copy left-overs to swp */
	{
		size_t sleft_sz = DATD(eos, sp);
		size_t rleft_sz = DATD(eor, rp);
		size_t gapsz = DATD(rp, DATA(swp->sk.sp, sleft_sz));

		memcpy(swp->sk.sp, sp, sleft_sz);
		/* and close the gap */
		if (gapsz > 0) {
			memmove(DATA(swp->sk.sp, sleft_sz), rp, rleft_sz);
			/* adapt the tidx */
			swp->sk.si -= gapsz / sizeof(*swp->sk.sp);
		}
	}
	/* adapt tgt idx, according to the assertion, that's quite simple */
	tgt->si = tgt->szrw / sizeof(*tgt->sp);
	return;
}

/* for the tilman compression, which doesn't really belong here */
#include "ssnp.h"

static bool
ssnp_compressiblep(const_ssnp_t t1, const_ssnp_t t2)
{
	/* make use of the fact that snaps have no notion
	 * of bid and ask and that scom-v0.2 is nice */
	if (t1->hdr->u != t2->hdr->u) {
		/* nope, time stamps or tblidx don't match */
		return false;
	} else if ((t1->ap || t2->bp) && (t2->ap || t1->bp)) {
		return false;
	}
	return true;
}

static void
ssnp_compress(ssnp_t tgt, const_ssnp_t s1, const_ssnp_t s2)
{
	struct ssnp_s tmp = {
		.hdr = s1->hdr->u
	};

	if (s1->bp) {
		tmp.bp = s1->bp;
		tmp.bq = s1->bq;
		tmp.ap = s2->ap;
		tmp.aq = s2->aq;
	} else if (s1->ap) {
		tmp.bp = s2->bp;
		tmp.bq = s2->bq;
		tmp.ap = s1->ap;
		tmp.aq = s1->aq;
	}
	if (s1->tvpr) {
		tmp.tvpr = s1->tvpr;
		tmp.tq = s1->tq;
	} else {
		tmp.tvpr = s2->tvpr;
		tmp.tq = s2->tq;
	}
	/* copy the candle to the target position */
	*tgt = tmp;
	return;
}

DEFUN sidx_t
tilman_1comp(uteseek_t tgt, uteseek_t sk)
{
/* we perform this on a seek as we need the ticks sorted
 * also, this is in-situ if TGT == SK. */
	void *tp = tgt->sp + tgt->si;
	void *eot = tgt->sp + tgt->szrw / sizeof(*sk->sp);
	void *sp = sk->sp + sk->si;
	void *eos = sk->sp + sk->szrw / sizeof(*sk->sp);
	sidx_t res = 0;

	while (tp < eot && sp < eos) {
		switch (scom_thdr_ttf(sp)) {
			size_t bsz;
		case SSNP_FLAVOUR: {
			void *nex;

			bsz = sizeof(struct ssnp_s);
			if (UNLIKELY((nex = DATA(sp, bsz)) >= eos)) {
				/* best to fuck off */
				goto condens;
			} else if (!ssnp_compressiblep(sp, nex)) {
				/* what a pity */
				goto condens;
			}
			/* yay we can compress the guys */
			ssnp_compress(tp, sp, nex);
			/* update pointers */
			tp = DATA(tp, bsz);
			sp = DATA(nex, bsz);
			/* step the result counter */
			res++;
			break;
		}
		default:
			bsz = scom_byte_size(sp);
		condens:
			if (tp != sp) {
				memcpy(tp, sp, bsz);
			}
			tp = DATA(tp, bsz);
			sp = DATA(sp, bsz);
		}
	}
	sk->si = DATD(sp, sk->sp) / sizeof(*sk->sp);
	tgt->si = DATD(tp, tgt->sp) / sizeof(*tgt->sp);
	return res;
}

DEFUN void
seek_sort(uteseek_t sk)
{
/* simplified merge sort */
	struct sndwch_s *np;
	sndwch_t tp, ep;
	size_t noffs = 0;
	size_t sk_sz = seek_byte_size(sk);
	void *new;
#define new_data	((struct sndwch_s*)((char*)new + __pgsz))
#define new_offs	((uint32_t*)(new))

	/* we never hand out bigger pages */
	assert(sk_sz <= UTE_BLKSZ * sizeof(*sk->sp));

	/* get us another map */
	new = mmap(NULL, __pgsz + sk_sz, PROT_MEM, MAP_MEM, -1, 0);

#if defined DEBUG_FLAG
	/* randomise the rest of the seek page */
	{
		size_t rbsz = sk_sz - sk->si * sizeof(*sk->sp);
		UDEBUGvv("seek_sort(): randomising %zu bytes\n", rbsz);
		memset(sk->sp + sk->si, -1, rbsz);
	}
#endif	/* DEBUG_FLAG */

	np = new_data;
	tp = sk->sp;
	ep = sk->sp + sk->si;
	for (size_t nticks; tp < ep; tp += nticks, np += nticks) {
		perm_idx_t pi;

		/* sort 256 (or less) *scom*s */
		nticks = idxsort(&pi, tp, ep);
		/* store the value of nticks in the offset array */
		new_offs[noffs++] = nticks;
		/* now collate */
		collate(np, tp, pi, nticks);
	}

	/* now in NEW->data there's sorted pages consisting of 256
	 * *scom*s each the tick offsets are in NEW->offs, there's a
	 * maximum of 1024 offsets (thats MAX_TICKS_PER_TPC / 256)
	 * we now use a bottom-up merge step */
#if defined DEBUG_FLAG
	/* check for randomisation leaks */
	{
		size_t tot = 0;

		/* determine the number of ticks */
		for (size_t i = 0; i < noffs; i++) {
			tot += new_offs[i];
		}
		UDEBUGvv("a total of %zu ticks to be b'up sorted\n", tot);

		for (sndwch_t foo = new_data, bar = foo + tot;
		     foo < bar; foo += scom_tick_size(AS_SCOM(foo))) {
			assert(AS_SCOM(foo)->u != 0ULL);
			assert(AS_SCOM(foo)->u != -1ULL);
		}
	}
#endif	/* DEBUG_FLAG */
	{
		void *const data = new_data;
		void *tgt = sk->sp;
		void *src = data;

		for (size_t rsz = 2; rsz < 2 * noffs; rsz *= 2) {
			/* ordinary bottom-up merge round */
			bup_round(tgt, src, rsz, new_offs, noffs);
			{
				/* swap the roles of src and tgt */
				void *tmp = tgt;
				tgt = src;
				src = tmp;
			}
		}
		if (sk->sp == tgt) {
			/* oh, we were about to copy shit into tgt
			 * just copy the rest so it ends up in seek space */
			memcpy(sk->sp, data, sk_sz);
		}
		/* munmap()ing is the same in either case */
		munmap(new, __pgsz + sk_sz);
	}
#if defined DEBUG_FLAG
	/* tpc should be sorted now innit */
	{
		uint64_t thresh = 0;
		for (sidx_t i = 0, tsz; i < sk->si; i += tsz) {
			scom_t t = AS_SCOM(sk->sp + i);

			assert(t->u);
			assert(t->u != -1ULL);
			assert(thresh <= t->u);
			thresh = t->u;
			tsz = scom_tick_size(t);
		}
	}
#endif	/* DEBUG_FLAG */
#undef new_offs
#undef new_data
	return;
}

DEFUN void
tpc_sort(utetpc_t tpc)
{
	/* sort the internal seek */
	seek_sort(&tpc->sk);
	/* set the sorted flag, i.e. unset the unsorted flag */
	unset_tpc_unsorted(tpc);
	/* update last key */
	tpc->last = tpc_last_scom(tpc)->u;
	return;
}

DEFUN void
init_tpc(void)
{
	init_scratch();
	return;
}

DEFUN void
fini_tpc(void)
{
	fini_scratch();
	return;
}

/* utetpc.c ends here */
