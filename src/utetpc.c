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

static inline void
set_tpc_unsorted(utetpc_t tpc)
{
	tpc->flags |= TPC_FL_UNSORTED;
	return;
}

static inline void
unset_tpc_unsorted(utetpc_t tpc)
{
	tpc->flags &= ~TPC_FL_UNSORTED;
	return;
}

static inline void
set_tpc_needmrg(utetpc_t tpc)
{
	tpc->flags |= TPC_FL_NEEDMRG;
	return;
}

static inline __attribute__((unused)) void
unset_tpc_needmrg(utetpc_t tpc)
{
	tpc->flags &= ~TPC_FL_NEEDMRG;
	return;
}

/* calloc like signature */
DEFUN void
make_tpc(utetpc_t tpc, size_t nticks, size_t tsz)
{
	size_t sz = nticks * tsz;
	tpc->tp = mmap(NULL, sz, PROT_MEM, MAP_MEM, 0, 0);
	if (LIKELY(tpc->tp != MAP_FAILED)) {
		tpc->tpsz = sz;
		tpc->tidx = 0;
		tpc->tsz = tsz;
	} else {
		tpc->tpsz = 0;
		tpc->tidx = -1;
		tpc->tsz = 0;
	}
	return;
}

DEFUN void
free_tpc(utetpc_t tpc)
{
	if (tpc_active_p(tpc)) {
		/* seek points to something => munmap first */
		munmap(tpc->tp, tpc->tpsz);
	}
	tpc->tidx = -1;
	tpc->tpsz = 0;
	tpc->tp = NULL;
	return;
}

DEFUN void
clear_tpc(utetpc_t tpc)
{
	if (tpc_active_p(tpc)) {
		tpc->tidx = 0;
	}
	return;
}

DEFUN void
tpc_add_tick(utetpc_t tpc, scom_t t, size_t tsz)
{
	uint64_t skey = tick_sortkey(t);

	if (UNLIKELY(tpc->tidx >= tpc->tpsz)) {
		return;
	}
	memcpy(tpc->tp + tpc->tidx, t, tsz);
	/* just add the total byte size as passed on */
	tpc->tidx += tsz;

	/* maybe mark the whole shebang as unsorted */
	if (UNLIKELY(skey < tpc->last)) {
		set_tpc_unsorted(tpc);
	}
	tpc->last = skey;
	/* check if the whole file needs sorting (merging) */
	if (UNLIKELY(skey < tpc->lvtd /* largest value to-date */)) {
		set_tpc_needmrg(tpc);
	}
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
#define PERM(a, b, c, d)			\
	(uint8_t)(				\
		(a & 0x3) |			\
		(b & 0x3) << 2 |		\
		(c & 0x3) << 4 |		\
		(d & 0x3) << 6)
#define PERMI(p, a)				\
	(uint8_t)((p >> (2 * a)) & 0x3)
#define IDXSORT_SIZE	256

/* union based scidx
 * order in the struct part is important as we need this to compare
 * two such things for sorting; go from least significant to most */
typedef union __attribute__((transparent_union)) {
	uint64_t u;
	struct {
		uint32_t idx:22;
		uint32_t msec:10;
		uint32_t sec:32;
	};
} scidx_t;

static uint8_t
pornsort_perm(scidx_t p[4])
{
	uint8_t ia = 0, ib = 1, ic = 2, id = 3;
	uint8_t perm;

	if (p[1].u < p[0].u) {
		/* swap them right away? */
		ia = 1, ib = 0;
		p[0].u = p[1].u, p[1].u = p[0].u;
	}
	if (p[3].u < p[2].u) {
		/* swap them right away? */
		ic = 3, id = 2;
		p[2].u = p[3].u, p[3].u = p[2].u;
	}

	/* bit like AA-sort now, final comparison */
	if (p[2].u < p[0].u) {
		/* c first */
		if (p[3].u < p[0].u) {
			/* d next, then a, then b */
			perm = PERM(ic, id, ia, ib);
		} else {
			/* a next */
			if (p[3].u < p[1].u) {
				/* d next, then b */
				perm = PERM(ic, ia, id, ib);
			} else {
				/* b next, then d */
				perm = PERM(ic, ia, ib, id);
			}
		}
	} else {
		/* a first */
		if (p[2].u < p[1].u) {
			/* c next */
			if (p[3].u < p[1].u) {
				/* d next, then b */
				perm = PERM(ia, ic, id, ib);
			} else {
				/* b next, then d */
				perm = PERM(ia, ic, ib, id);
			}
		} else {
			/* b next, then c, then d */
			perm = PERM(ia, ib, ic, id);
		}
	}
	return perm;
}

/* pornsort of 4 ticks */
static void
pornsort_apply(scidx_t p[4], uint8_t perm)
{
	uint64_t tmp;

	switch (perm) {
	case PERM(0, 1, 2, 3):
		/* nothing to do */
		break;

	case PERM(1, 0, 2, 3):
		/* (1,2) */
		/* not possible given the above perm generator */
		tmp = p[0].u;
		p[0].u = p[1].u, p[1].u = tmp;
		break;

	case PERM(2, 1, 0, 3):
		/* (1,3) */
		tmp = p[0].u;
		p[0].u = p[2].u, p[2].u = tmp;
		break;

	case PERM(3, 1, 2, 0):
		/* (1,4) */
		tmp = p[0].u;
		p[0].u = p[3].u, p[3].u = tmp;
		break;

	case PERM(0, 2, 1, 3):
		/* (2,3) */
		tmp = p[1].u;
		p[1].u = p[2].u, p[2].u = tmp;
		break;

	case PERM(0, 3, 2, 1):
		/* (2,4) */
		tmp = p[1].u;
		p[1].u = p[3].u, p[3].u = tmp;
		break;

	case PERM(0, 1, 3, 2):
		/* (3,4) */
		/* not possible with the above perm generator */
		tmp = p[2].u;
		p[2].u = p[3].u, p[3].u = tmp;
		break;

	case PERM(1, 0, 3, 2):
		/* (1,2)(3,4) */
		/* not possible with the above perm generator */
		tmp = p[0].u;
		p[0].u = p[1].u, p[1].u = tmp;
		tmp = p[2].u;
		p[2].u = p[3].u, p[3].u = tmp;
		break;

	case PERM(2, 3, 0, 1):
		/* (1,3)(2,4) */
		tmp = p[0].u;
		p[0].u = p[2].u, p[2].u = tmp;
		tmp = p[1].u;
		p[1].u = p[3].u, p[3].u = tmp;
		break;

	case PERM(3, 2, 1, 0):
		/* (1,4)(2,3) */
		tmp = p[0].u;
		p[0].u = p[3].u, p[3].u = tmp;
		tmp = p[1].u;
		p[1].u = p[2].u, p[2].u = tmp;
		break;

	default: {
		/* 3- and 4-cycles here */
		uint64_t tmpa[4];

		if (PERMI(perm, 0) != 0) {
			tmpa[0] = p[0].u;
			p[0].u = p[PERMI(perm, 0)].u;
		}
		if (PERMI(perm, 1) != 1) {
			tmpa[1] = p[1].u;
			/* PERMI(perm, 0) cant be 0 */
			p[1].u = p[PERMI(perm, 1)].u;
		}
		if (PERMI(perm, 2) != 2) {
			tmpa[2] = p[2].u;
			tmpa[3] = p[3].u;
			/* PERMI(perm, 0) cant be 0 */
			p[2].u = tmpa[PERMI(perm, 2)];
		}
		if (PERMI(perm, 3) != 3) {
			/* PERMI(perm, 0) cant be 0 */
			p[3].u = tmpa[PERMI(perm, 1)];
		}
	}
	}
	return;
}

static inline scidx_t
make_scidx(scom_t t, sidx_t idx)
{
	scidx_t res = {
		.sec = scom_thdr_sec(t),
		.msec = scom_thdr_msec(t),
		.idx = idx,
	};
	return res;
}

static inline uint32_t
scidx_idx(scidx_t sci)
{
	return sci.idx;
}

#define min(x, y)	(x < y ? x : y)

static inline size_t
min_size_t(size_t x, size_t y)
{
	return min(x, y);
}

/* not reentrant, fixme */
static scidx_t *__gidx = MAP_FAILED;
#define SCRATCH_SIZE	(IDXSORT_SIZE * 2 * sizeof(uint64_t))

static void
init_scratch(void)
{
	if (UNLIKELY(__gidx == MAP_FAILED)) {
		__gidx = mmap(NULL, SCRATCH_SIZE, PROT_MEM, MAP_MEM, -1, 0);
	}
	return;
}

static void*
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

static void
merge_up(scidx_t *tgt, scidx_t *src, int step, int max)
{
/* bottom up merge steps, for arrays of scidx's */
	int i1 = 0;
	int i2 = step;
	int ei1 = min(max, step);
	int ei2 = min(max, 2 * step);
	int i = 0;

	for (; i1 < ei1 && i2 < ei2; i++) {
		tgt[i].u = src[i1].u < src[i2].u ? src[i1++].u : src[i2++].u;
	}
	for (; i1 < ei1; i++, i1++) {
		tgt[i].u = src[i1].u;
	}
	for (; i2 < ei2; i++, i2++) {
		tgt[i].u = src[i2].u;
	}
	return;
}

static void
merge_all(size_t nticks)
{
	scidx_t *src = get_scratch();
	scidx_t *tgt = src + IDXSORT_SIZE;

	/* steps 4, 8, 16, 32, 64, 128
	 * we strictly need an even number of steps so that the final
	 * result is in the first half of gidx */
	for (int step = 4; step < IDXSORT_SIZE; ) {
		int s2 = 2 * step, j;

		for (size_t i = 0; i < nticks; i += s2) {
			merge_up(tgt + i, src + i, step, nticks);
		}
		/* last ones */
		if ((j = nticks % s2)) {
			int i = nticks - j;
			merge_up(tgt + i, src + i, step, j);
		}
		step = s2;
		/* swap source and target roles (ring buffer) */
		if (src < tgt) {
			tgt = get_scratch();
			src = tgt + IDXSORT_SIZE;
		} else {
			src = get_scratch();
			tgt = src + IDXSORT_SIZE;
		}
	}
	return;
}

static scidx_t*
idxsort(scom_t p, size_t UNUSED(satsz), size_t nticks)
{
	scidx_t *scp = get_scratch();
	size_t m = min(nticks, IDXSORT_SIZE);
	size_t j = 0;

	for (size_t i = 0; i < m; j++) {
		size_t bsz = scom_thdr_size(p);

		scp[j] = make_scidx(p, i);
		p = DATCA(p, bsz);
		/* maybe it's faster to use a hard-coded satsz here? */
		i += bsz / sizeof(struct sl1t_s);
	}
	/* fill scp up to the next multiple of 4 */
	while (j % 4) {
		scp[j++].u = -1UL;
	}
	/* use the local index in scp to do pornsort */
	for (size_t i = 0; i < j; i += 4) {
		uint8_t perm = pornsort_perm(scp + i);
		pornsort_apply(scp + i, perm);
	}

	/* merge steps */
	merge_all(m);
	return scp;
}

static void
collate(void *tgt, const void *src, scidx_t *idxa, size_t nticks, size_t tsz)
{
	for (size_t i = 0, j = 0; i < nticks; j++) {
		sidx_t idx = scidx_idx(idxa[j]);
		const void *s = DATCI(src, idx, tsz);
		size_t bsz = scom_thdr_size(s);

		memcpy(tgt, s, bsz);
		tgt = DATA(tgt, bsz);
		i += bsz / sizeof(struct sl1t_s);
	}
	return;
}

static void*
merge_bup(
	void *tgt,
	const void *srcl, size_t nticksl,
	const void *srcr, size_t nticksr,
	size_t tsz)
{
/* merge left source SRCL and right source SRCR into tgt. */
	const void *elp = DATCI(srcl, nticksl, tsz);
	const void *erp = DATCI(srcr, nticksr, tsz);

	while (srcl < elp && srcr < erp) {
		uint64_t sl = ((const uint64_t*)srcl)[0];
		uint64_t sr = ((const uint64_t*)srcr)[0];

		if (sl <= sr) {
			size_t bszl = scom_thdr_size(srcl);
			/* copy the left tick */
			memcpy(tgt, srcl, bszl);
			/* step things */
			srcl = DATCA(srcl, bszl);
			tgt = DATA(tgt, bszl);
		} else {
			size_t bszr = scom_thdr_size(srcr);
			/* use the right guy */
			memcpy(tgt, srcr, bszr);
			/* step things */
			srcr = DATCA(srcr, bszr);
			tgt = DATA(tgt, bszr);
		}
	}
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

static void
bup_round(void *tgt, void *src, size_t rsz, size_t ntleft, size_t tsz)
{
	void *tp = tgt;
	void *npl = src;
	void *npr = DATI(src, rsz, tsz);

	/* do i really need both npl and npr? */
	while (ntleft > rsz) {
		size_t ntr = min_size_t(ntleft, 2 * rsz);
		/* there's both a left and a right side */
		tp = merge_bup(tp, npl, rsz, npr, ntr - rsz, tsz);
		/* step down counters and step up pointers */
		npl = DATI(npl, 2 * rsz, tsz);
		npr = DATI(npr, 2 * rsz, tsz);
		ntleft -= ntr;
	}
	/* possibly ntleft ticks stuck */
	memcpy(tp, npl, ntleft * tsz);
	return;
}

DEFUN void
tpc_sort(utetpc_t tpc)
{
/* simplified merge sort */
	void *new;

	/* get us another map */
	new = mmap(NULL, tpc->tpsz, PROT_MEM, MAP_MEM, -1, 0);

	for (void *tp = tpc->tp, *np = new,
		     *ep = DATA(tp, tpc->tidx); tp < ep; ) {
		size_t ntleft = DATDI(ep, tp, tpc->tsz);
		size_t nticks = min(IDXSORT_SIZE, ntleft);
		scidx_t *scp = idxsort(tp, tpc->tsz, nticks);
		collate(np, tp, scp, nticks, tpc->tsz);
		tp = DATI(tp, nticks, tpc->tsz);
		np = DATI(np, nticks, tpc->tsz);
	}
	/* now in NEW there's sorted pages consisting of 256 ticks each
	 * we now use a bottom-up merge step */
	{
		void *tgt = tpc->tp;
		void *src = new;
		for (size_t rsz = 256; tpc_nticks(tpc) > rsz; rsz *= 2) {
			void *tmp;
			bup_round(tgt, src, rsz, tpc_nticks(tpc), tpc->tsz);
			/* swap the roles of src and tgt */
			tmp = tgt, tgt = src, src = tmp;
		}
		if (tpc->tp == tgt) {
			/* oh, we were about to copy shit into tgt, so
			 * munmap tpc->tp and install the new vector */
			tpc->tp = src;
		}
		/* munmap()ing is the same in either case */
		munmap(tgt, tpc->tpsz);
	}
	/* set the sorted flag, i.e. unset the unsorted flag */
	unset_tpc_unsorted(tpc);
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
