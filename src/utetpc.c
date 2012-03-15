/*** utetpc.c -- tick pages
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

#define UTETPC_C
#include <stddef.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include "utetpc.h"

/* we're just as good as rudi, aren't we? */
#if defined DEBUG_FLAG
# include <assert.h>
# include <stdio.h>
# define UDEBUG(args...)	fprintf(stderr, args)
# define MAYBE_NOINLINE		__attribute__((noinline))
#else  /* !DEBUG_FLAG */
# define UDEBUG(args...)
# define assert(args...)
# define MAYBE_NOINLINE
#endif	/* DEBUG_FLAG */

#if !defined UNUSED
# define UNUSED(_x)	__attribute__((unused)) _x
#endif	/* !UNUSED */
#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif	/* !LIKELY */
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif	/* !LIKELY */

#define MAP_MEM		(MAP_PRIVATE | MAP_ANONYMOUS)
#define PROT_MEM	(PROT_READ | PROT_WRITE)

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

/* calloc like signature */
DEFUN void
make_tpc(utetpc_t tpc, size_t nsndwchs)
{
	size_t sz = nsndwchs * sizeof(struct sndwch_s);

	tpc->sk.sp = mmap(NULL, sz, PROT_MEM, MAP_MEM, 0, 0);
	if (LIKELY(tpc->sk.sp != MAP_FAILED)) {
		tpc->sk.sz = sz;
		tpc->sk.si = 0;
	} else {
		tpc->sk.sz = 0;
		tpc->sk.si = -1;
	}
	return;
}

DEFUN void
free_tpc(utetpc_t tpc)
{
	if (tpc_active_p(tpc)) {
		/* seek points to something => munmap first */
		munmap(tpc->sk.sp, tpc->sk.sz);
	}
	tpc->sk.si = -1;
	tpc->sk.sz = 0;
	tpc->sk.sp = NULL;
	return;
}

DEFUN void
clear_tpc(utetpc_t tpc)
{
	if (tpc_active_p(tpc)) {
		tpc->sk.si = 0;
	}
	return;
}

DEFUN void
tpc_add(utetpc_t tpc, scom_t t, size_t nt)
{
/* supports variadic ticks */
	uint64_t skey = tick_sortkey(t);

	if (UNLIKELY(tpc_full_p(tpc) || !tpc_can_hold_p(tpc, nt) || !skey)) {
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

static inline size_t
min_size_t(size_t x, size_t y)
{
	return min(x, y);
}

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

static perm_idx_t MAYBE_NOINLINE
idxsort(scom_t p, size_t nticks)
{
	perm_idx_t keys = get_scratch();
	size_t m = min(nticks, IDXSORT_SIZE);
	size_t j = 0;

	for (size_t i = 0, tsz, bsz; i < m; j++, i += tsz) {
		tsz = scom_tick_size(p);
		bsz = scom_byte_size(p);

		put_pi(keys + j, p, i);
		p = DATCA(p, bsz);
	}
	/* reuse m to compute the next 2-power */
	m = __ilog2_ceil(j);
	/* check if m is a 2-power indeed */
	assert((m & (m - 1)) == 0);
	/* ... and a ceil */
	assert(m >= j);
	/* fill scp up with naughts */
	memset(keys + j, 0, (m - j) * sizeof(*keys));

	/* use the local index in scp to do pornsort */
	for (size_t i = 0; i < j; i += 4) {
		uint8_t perm = pornsort_perm(keys + i);
		pornsort_apply(keys + i, perm);
	}

	/* merge steps, up to the next 2-power */
	merge_all(m);
	return keys;
}

static void
collate(void *tgt, const void *src, perm_idx_t pi, size_t nticks)
{
/* collating is the process of putting the satellite data along with
 * the sorted keys again
 * SRC is an array of keys and satellite data and
 * PI is the permutation to apply */
	/* skip 0 idxs first */
	size_t j;

	for (j = 0; j < nticks && pi_skey(pi + j) == 0ULL; j++);

#if defined DEBUG_FLAG && 0
	/* since idxsort used 2-powers and we don't allow 0 skeys
	 * the sum of J and NTICKS should be a 2-power
	 * we use the identity 1 + \sum_i 2^i = 2^{i+1} to detect a 2-power */
	size_t ni = j + nticks;
	assert(((ni - 1) & ni) == 0);
	/* this turns out to be wrong in the case of variadic tick sizes */
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
merge_bup(
	void *tgt,
	const void *srcl, size_t nticksl,
	const void *srcr, size_t nticksr)
{
/* merge left source SRCL and right source SRCR into tgt. */
	const void *elp = DATCI(srcl, nticksl, sizeof(struct sndwch_s));
	const void *erp = DATCI(srcr, nticksr, sizeof(struct sndwch_s));

	while (srcl < elp && srcr < erp) {
		uint64_t sl = ((const uint64_t*)srcl)[0];
		uint64_t sr = ((const uint64_t*)srcr)[0];

		if (sl <= sr) {
			size_t bszl = scom_byte_size(srcl);
			/* copy the left tick */
			memcpy(tgt, srcl, bszl);
			/* step things */
			srcl = DATCA(srcl, bszl);
			tgt = DATA(tgt, bszl);
		} else {
			size_t bszr = scom_byte_size(srcr);
			/* use the right guy */
			memcpy(tgt, srcr, bszr);
			/* step things */
			srcr = DATCA(srcr, bszr);
			tgt = DATA(tgt, bszr);
		}
	}
	/* the end pointers must match exactly, otherwise we copied too much */
	assert(srcl == elp || srcr == erp);

	if (srcl < elp) {
		/* not all left ticks */
		size_t sz = DATCD(elp, srcl);
		memcpy(tgt, srcl, sz);
		tgt = DATA(tgt, sz);
	} else if (srcr < erp) {
		/* right ticks left */
		size_t sz = DATCD(erp, srcr);
		memcpy(tgt, srcr, sz);
		tgt = DATA(tgt, sz);
	} else {
		;
	}
	return tgt;
}

static void MAYBE_NOINLINE
bup_round(void *tgt, void *src, size_t rsz, size_t ntleft)
{
	void *tp = tgt;
	void *npl = src;
	void *npr = DATI(src, rsz, sizeof(struct sndwch_s));

	/* do i really need both npl and npr? */
	while (ntleft > rsz) {
		size_t ntr = min_size_t(ntleft, 2 * rsz);
		/* there's both a left and a right side */
		tp = merge_bup(tp, npl, rsz, npr, ntr - rsz);
		/* step down counters and step up pointers */
		npl = DATI(npl, 2 * rsz, sizeof(struct sndwch_s));
		npr = DATI(npr, 2 * rsz, sizeof(struct sndwch_s));
		ntleft -= ntr;
	}
	/* possibly ntleft ticks stuck */
	memcpy(tp, npl, ntleft * sizeof(struct sndwch_s));
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
	const size_t probsz = sizeof(*sk->sp);

	if (UNLIKELY(sk->sp == NULL)) {
		return NULL;
	}
	return sk->sp + algn_tick(sk, sk->sz / probsz - 1);
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
		     eosp = DATA(sk->sp, sk->sz); bosp < eosp; ) {
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
	assert(sk->si * sizeof(*sk->sp) < sk->sz);
	return AS_SCOM(sp);
}

DEFUN void
merge_2tpc(uteseek_t tgt, uteseek_t src, utetpc_t swp)
{
/* merge stuff from SRC and SWP into TGT, leftovers will be put in SWP */
	void *tp = tgt->sp + tgt->si;
	void *eot = DATA(tgt->sp, tgt->sz);
	void *sp = src->sp + src->si;
	void *eos = DATA(src->sp, src->sz);
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
	tgt->si = tgt->sz / sizeof(*tgt->sp);
	return;
}

/* for the tilman compression, which doesn't really belong here */
#include "scdl.h"

static bool
ssnap_compressiblep(const_ssnap_t t1, const_ssnap_t t2)
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
ssnap_compress(ssnap_t tgt, const_ssnap_t s1, const_ssnap_t s2)
{
	struct ssnap_s tmp = {
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
	void *eot = DATA(tgt->sp, tgt->sz);
	void *sp = sk->sp + sk->si;
	void *eos = DATA(sk->sp, sk->sz);
	sidx_t res = 0;

	while (tp < eot && sp < eos) {
		switch (scom_thdr_ttf(sp)) {
			size_t bsz;
		case SSNP_FLAVOUR: {
			void *nex;

			bsz = sizeof(struct ssnap_s);
			if (UNLIKELY((nex = DATA(sp, bsz)) >= eos)) {
				/* best to fuck off */
				goto condens;
			} else if (!ssnap_compressiblep(sp, nex)) {
				/* what a pity */
				goto condens;
			}
			/* yay we can compress the guys */
			ssnap_compress(tp, sp, nex);
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
	const size_t tpc_tsz = sizeof(*sk->sp);
	void *new;

	/* get us another map */
	new = mmap(NULL, sk->sz, PROT_MEM, MAP_MEM, -1, 0);

#if defined DEBUG_FLAG
	/* randomise the rest of the seek page */
	{
		size_t rest_bsz = sk->sz - sk->si * sizeof(*sk->sp);
		UDEBUG("seek_sort(): randomising %zu bytes\n", rest_bsz);
		memset(sk->sp + sk->si, 0x93, rest_bsz);
	}
#endif	/* DEBUG_FLAG */

	for (void *tp = sk->sp, *np = new, *ep = sk->sp + sk->si; tp < ep; ) {
		size_t ntleft = DATDI(ep, tp, tpc_tsz);
		size_t nticks = min(IDXSORT_SIZE, ntleft);
		perm_idx_t pi = idxsort(tp, nticks);

		UDEBUG("collation of %zu ticks (%zu left)\n", nticks, ntleft);
		collate(np, tp, pi, nticks);
		tp = DATI(tp, nticks, tpc_tsz);
		np = DATI(np, nticks, tpc_tsz);
	}
	/* now in NEW there's sorted pages consisting of 256 ticks each
	 * we now use a bottom-up merge step */
	{
		void *tgt = sk->sp;
		void *src = new;
		for (size_t rsz = 256; sk->si > rsz; rsz *= 2) {
			void *tmp;

			UDEBUG("b'up round with round size %zu\n", rsz);
			bup_round(tgt, src, rsz, sk->si);
			/* swap the roles of src and tgt */
			tmp = tgt, tgt = src, src = tmp;
		}
		if (sk->sp == tgt) {
			/* oh, we were about to copy shit into tgt
			 * just copy the rest so it ends up in seek space */
			memcpy(sk->sp, new, sk->sz);
		}
		/* munmap()ing is the same in either case */
		munmap(new, sk->sz);
	}
#if defined DEBUG_FLAG
	/* tpc should be sorted now innit */
	{
		uint64_t thresh = 0;
		for (sidx_t i = 0, tsz; i < sk->si; i += tsz) {
			scom_t t = AS_SCOM(sk->sp + i);

			assert(thresh <= t->u);
			thresh = t->u;
			tsz = scom_tick_size(t);
		}
	}
#endif	/* DEBUG_FLAG */
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
