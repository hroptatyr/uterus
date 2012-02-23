/*** utesort.c -- low-level sorting guts
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

#define UTESORT_C
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



/* sorter */
/* we use an external merge sort algo here
 * since each of the pages are (considered to be) sorted already we have
 * completed the run initialisation phase.
 * what's left to do is to take N runs and merge them into 1 super-run,
 * then take N super-runs and merge them into one super-super-run and so on.
 *
 * we specifically use the algorithm by
 * M. Islam, M. Noor, M. S. Sarker and S. R. Islam:
 * - sort the S runs
 * - divide each run into 2 sub-runs
 * - read lower sub-run of run 1 into the lower half of the output buffer
 *   Q <- 2
 * - read lower sub-run of run Q into the upper half of the output buffer
 * - merge the sub-runs, write upper half to the lower sub-run of run Q
 *   Q <- Q + 1
 *   if Q <= S go to previous step
 * - write lower half into the lower sub-run of run 1
 * - read upper sub-run of run S into the upper half of the output buffer
 *   Q <- S - 1
 * - Read upper sub-run of run Q into the lower half of the output buffer
 * - merge the sub-runs, write lower half to the upper sub-run of run Q
 *   Q <- Q - 1
 *   if Q >= 1, go to previous step
 * - write upper half into the upper sub-run of run S
 */
#include <stdio.h>
#include <limits.h>
#include "intvtree.c"

#define AS_y(t, x)	((t)(long int)(void*)(x))
#define AS_INT(x)	AS_y(int, x)
#define AS_UINT32(x)	AS_y(uint32_t, x)

typedef struct strat_node_s *strat_node_t;
typedef struct strat_s *strat_t;

struct strat_node_s {
	strat_node_t next;
	uint32_t pg;
	uint32_t cnt;
	uint32_t pgs[];
};

struct __cnt_clo_s {
	size_t cnt;
};

struct strat_s {
	itree_t it;
	strat_node_t first;
	strat_node_t last;
};

static void
__mrg(it_node_t itnd, void *clo)
{
	uint32_t pg = AS_UINT32(itnd->data);
	strat_node_t sn = clo;

	if (sn->pg != pg) {
		sn->pgs[sn->cnt++] = pg;
	}
	return;
}

static void
__cnt(it_node_t UNUSED(itnd), void *clo)
{
	struct __cnt_clo_s *c = clo;
	c->cnt++;
	return;
}

static void
__strat_cb(it_node_t itnd, void *clo)
{
	int pg = AS_INT(itnd->data);
	int32_t lo = itnd->lo;
	int32_t hi = itnd->hi;
	strat_t sc = clo;
	struct __cnt_clo_s cc[1];
	strat_node_t sn;
	itree_t it = sc->it;

	cc->cnt = 0;
	itree_find_point_cb(it, lo, __cnt, cc);
	itree_find_point_cb(it, hi, __cnt, cc);

	/* create a strat node */
	sn = xmalloc(sizeof(struct strat_node_s) + (cc->cnt - 1) * sizeof(int));
	sn->pg = pg;
	sn->cnt = 0;
	/* find the points again, this time filling the merge closure */
	itree_find_point_cb(it, lo, __mrg, sn);
	sn->pgs[sn->cnt++] = pg;
	itree_find_point_cb(it, hi, __mrg, sn);

	/* now put that node into our list of nodes */
	if (sc->first == NULL) {
		sc->first = sc->last = sn;
	} else {
		sc->last->next = sn;
		sc->last = sn;
	}
	return;
}

static inline size_t
min_size_t(size_t x, size_t y)
{
	return x < y ? x : y;
}

static void
load_runs(struct uteseek_s *sks, utectx_t ctx, sidx_t start_run, sidx_t end_run)
{
	size_t e = min_size_t(end_run, ute_npages(ctx));
	for (size_t k = start_run, i = 0; k < e; i++, k++) {
		/* set up page i */
		seek_page(&sks[i], ctx, k);
	}
	return;
}

static void
dump_runs(struct uteseek_s *sks, utectx_t ctx, sidx_t start_run, sidx_t end_run)
{
	size_t e = min_size_t(end_run, ute_npages(ctx));
	for (size_t i = 0, k = start_run; k < e; i++, k++) {
		/* set up page i */
		flush_seek(&sks[i]);
	}
	return;
}

static strat_t
sort_strat(utectx_t ctx)
{
/* could be configurable */
#define NRUNS		16
#define AS_VOID_PTR(x)	((void*)(long int)(x))
	struct uteseek_s sks[NRUNS];
	itree_t it = make_itree();
	size_t npages = ute_npages(ctx);
	strat_t sc;

	for (size_t j = 0; j < npages; ) {
		/* initialise the seeks */
		load_runs(sks, ctx, j, j + NRUNS);

		/* obtain intervals */
		for (size_t i = 0, k = j; i < NRUNS && k < npages; i++, k++) {
			scom_t sb = (const void*)sks[i].data;
			uint32_t sbs = scom_thdr_sec(sb);
			//uint16_t sbms = scom_thdr_msec(sb);
			scom_t se = (const void*)
				(sks[i].data + sks[i].mpsz - 16);
			uint32_t ses = scom_thdr_sec(se);
			//uint16_t sems = scom_thdr_msec(se);

			itree_add(it, sbs, ses, AS_VOID_PTR(k));
		}

		/* finish off the seeks */
		dump_runs(sks, ctx, j, j + NRUNS);
		j += NRUNS;
	}
	/* run the strategy evaluator */
	sc = xnew(struct strat_s);
	sc->first = sc->last = NULL;
	sc->it = it;
	itree_trav_in_order(it, __strat_cb, sc);

	/* blast the itree to nirvana */
	free_itree(it);
	return sc;
}

static void
free_strat(strat_t str)
{
	
	for (strat_node_t p = str->first, q; p; p = q) {
		q = p->next;
		xfree(p);
	}
	xfree(str);
	return;
}

static ssize_t
min_run(struct uteseek_s *sks, size_t UNUSED(nruns), strat_t str)
{
	ssize_t res = -1;
	int32_t mins = INT_MAX;
	uint16_t minms = SCOM_MSEC_VALI;
	strat_node_t curnd = str->last;

	if (UNLIKELY(curnd == NULL)) {
		return res;
	}
	for (size_t i = 0; i < curnd->cnt; i++) {
		scom_t sh;
		int32_t s;
		uint16_t ms;
		uint32_t pg = curnd->pgs[i];

		if (sks[pg].idx >= sks[pg].mpsz) {
			continue;
		}
		sh = (void*)(sks[pg].data + sks[pg].idx);
		s = scom_thdr_sec(sh);
		ms = scom_thdr_msec(sh);
		if (s < mins || (s == mins && ms < minms)) {
			res = pg;
			mins = s;
			minms = ms;
		}
	}
	return res;
}

static void
step_run(struct uteseek_s *sks, unsigned int run, strat_t str)
{
/* advance the pointer in the RUN-th run and fetch new stuff if need be */
	strat_node_t curnd = str->last;

	if ((sks[run].idx += tpc_tsz) >= sks[run].mpsz) {
		/* reget shit, for now we just stall this run */
		sks[run].idx = sks[run].mpsz;
#if 0
/* debug */
		printf("run %d out of ticks\n", run);
#endif
		flush_seek(&sks[run]);
		/* now if we flushed the current strategy node's main page
		 * then also switch to the next strategy node */
		if (curnd->pg == run) {
			/* ah. last one in the current strategy node
			 * get me the hammer */
			str->last = str->last->next;
		}
	}
	return;
}

void
ute_sort(utectx_t ctx)
{
	struct uteseek_s *sks;
	size_t npages = ute_npages(ctx);
	void *hdl;
	strat_t str;

	/* this is to obtain a merge strategy,
	 * we have several outcomes:
	 * - merge k-way, where k is the number of pages
	 * - merge k-way n-pass, where k < #pages in multiple passes */
	str = sort_strat(ctx);
#if 0
/* debug */
	for (strat_node_t n = str->first; n; n = n->next) {
		printf("page %d, considering\n", n->pg);
		for (int i = 0; i < n->cnt; i++) {
			printf("  page %d\n", n->pgs[i]);
		}
	}
#endif

	/* let's assume we have an all-way merge */
	sks = xnew_array(struct uteseek_s, npages);
	load_runs(sks, ctx, 0, npages);

	hdl = ute_open(ctx->fname, UO_CREAT | UO_TRUNC);

	/* prepare the strategy, we use the last cell as iterator */
	str->last = str->first;
	/* ALL-way merge */
	for (ssize_t j; (j = min_run(sks, npages, str)) >= 0; ) {
		void *p = (void*)(sks[j].data + sks[j].idx);

		/* add that bloke */
		ute_add_tick(hdl, p);

		/* step the j-th run */
		step_run(sks, (size_t)j, str);
	}

	/* close the ute file */
	ute_close(hdl);
	/* dump the pages */
	dump_runs(sks, ctx, 0, npages);
	free(sks);

	/* free the strategy */
	free_strat(str);
	return;
}

/* utesort.c ends here */
