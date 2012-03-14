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

#if defined DEBUG_FLAG
# include <assert.h>
# include <stdio.h>
# define UDEBUG(args...)	fprintf(stderr, args)
#else  /* !DEBUG_FLAG */
# define UDEBUG(args...)
# define assert(args...)
#endif	/* DEBUG_FLAG */


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

/* we make use of the fact that intvtree.h is a template */
#if !defined INTV_TYPE
# define INTV_TYPE	uint64_t
#endif	/* INTV_TYPE */

#include "intvtree.h"
#include "intvtree.c"

#define AS_y(t, x)	((t)(long int)(void*)(x))
#define AS_UINT32(x)	AS_y(uint32_t, x)

#define T		INTV_TYPE

typedef struct strat_node_s *strat_node_t;
typedef struct strat_s *strat_t;

struct strat_node_s {
	strat_node_t next;
	uint32_t pg;
	uint32_t cnt;
	uint32_t pgs[];
};

struct strat_s {
	itree_t it;
	strat_node_t first;
	strat_node_t last;
};

struct __mrg_clo_s {
	strat_node_t sn;
};

static void
__mrg(it_node_t itnd, void *clo)
{
	struct __mrg_clo_s *c = clo;
	uint32_t pg = AS_UINT32(itnd->data);
	strat_node_t sn = c->sn;

	if (sn->pg != pg) {
		sn->pgs[sn->cnt++] = pg;
	}
	return;
}

static void
__mrg_self(it_node_t itnd, void *clo)
{
	struct __mrg_clo_s *c = clo;
	uint32_t pg = AS_UINT32(itnd->data);
	strat_node_t sn = c->sn;

	sn->pgs[sn->cnt++] = pg;
	return;
}

struct __cnt_clo_s {
	size_t cnt;
};

static void
__cnt(it_node_t UNUSED(itnd), void *clo)
{
	struct __cnt_clo_s *c = clo;
	c->cnt++;
	return;
}

struct __strat_clo_s {
	/* current page */
	strat_t strat;
};

static void
__strat_cb(it_node_t itnd, void *clo)
{
/* The rationale here is to find all pages that contain the current page's
 * lo and hi values and chain them up.
 * Special care has to be taken for pages that are completely contained
 * in another page, i.e.
 *   lo_x in [lo_y ... hi_y] and
 *   hi_x in [lo_y ... hi_y]
 * it follows that lo_y <= lo_x <= hi_x <= hi_y.
 * And just regarding the end-points of Y wouldn't detect page X.
 *
 * To solve that we keep track of the current highest hi page and include
 * it in the list of highers */
	struct __strat_clo_s *c = clo;
	uint32_t pg = AS_UINT32(itnd->data);
	T lo = itnd->lo;
	T hi = itnd->hi;
	strat_t s = c->strat;
	struct __cnt_clo_s cc[1];
	struct __mrg_clo_s mc[1];
	strat_node_t sn;
	itree_t it = s->it;

	cc->cnt = 0;
	itree_find_sups_cb(it, lo, hi, __cnt, cc);
	if (cc->cnt > 1) {
		UDEBUG("uh oh, page %u is subset of some other page\n", pg);
		return;
	}
	cc->cnt = 0;
	itree_find_point_cb(it, lo, __cnt, cc);
	itree_find_subs_cb(it, lo, hi, __cnt, cc);
	itree_find_point_cb(it, hi, __cnt, cc);

	assert(cc->cnt >= 2);
	if (cc->cnt < 2) {
		return;
	}

	/* create a strat node */
	sn = xmalloc(sizeof(struct strat_node_s) + (cc->cnt - 2) * sizeof(int));
	sn->pg = pg;
	sn->cnt = 0;
	sn->next = NULL;
	/* find the points again, this time filling the merge closure */
	mc->sn = sn;
	itree_find_point_cb(it, lo, __mrg, mc);
	/* insert pages that are contained in PG (inserts PG itself) */
	itree_find_subs_cb(it, lo, hi, __mrg_self, mc);
	/* finally insert all pages that contain the hi point */
	itree_find_point_cb(it, hi, __mrg, mc);

	/* now put that node into our list of nodes */
	if (s->first == NULL) {
		s->first = s->last = sn;
	} else {
		s->last->next = sn;
		s->last = sn;
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
		seek_page(sks + i, ctx, k);
	}

#if defined DEBUG_FLAG
	for (size_t k = start_run, j = 0; k < e; j++, k++) {
		const size_t sks_nticks = sks[j].sz / sizeof(*sks->sp);
		uint64_t thresh = 0;

		for (sidx_t i = 0, tsz; i < sks_nticks; i += tsz) {
			scom_t t = AS_SCOM(sks[j].sp + i);

			assert(thresh <= t->u);
			thresh = t->u;
			tsz = scom_tick_size(t);
		}
	}
#endif	/* DEBUG_FLAG */
	return;
}

static void
dump_runs(struct uteseek_s *sks, utectx_t ctx, sidx_t start_run, sidx_t end_run)
{
	size_t e = min_size_t(end_run, ute_npages(ctx));
	for (size_t i = 0, k = start_run; k < e; i++, k++) {
		/* set up page i */
		flush_seek(sks + i);
	}
	return;
}

static strat_t
sort_strat(utectx_t ctx)
{
/* could be configurable */
#define NRUNS		64
#define AS_VOID_PTR(x)	((void*)(long int)(x))
	struct uteseek_s sks[NRUNS];
	itree_t it = make_itree();
	size_t npages = ute_npages(ctx);
	strat_t s;
	struct __strat_clo_s sc[1];

	npages += tpc_has_ticks_p(ctx->tpc);
	for (size_t j = 0; j < npages; j += NRUNS) {
		/* initialise the seeks */
		load_runs(sks, ctx, j, j + NRUNS);

		/* obtain intervals */
		for (size_t i = 0, k = j; i < NRUNS && k < npages; i++, k++) {
			scom_t sb = seek_first_scom(sks + i);
			scom_t se = seek_last_scom(sks + i);

			assert(sb->u <= se->u);
			itree_add(it, sb->u, se->u, AS_VOID_PTR(k));
		}

		/* finish off the seeks */
		dump_runs(sks, ctx, j, j + NRUNS);
	}
	/* run the strategy evaluator */
	s = xnew(struct strat_s);
	s->first = s->last = NULL;
	s->it = it;
	/* load the strat cb closure */
	sc->strat = s;
	itree_trav_in_order(it, __strat_cb, sc);

	/* blast the itree to nirvana */
	free_itree(it);
	return s;
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
	uint64_t min = ULLONG_MAX;
	strat_node_t curnd = str->last;

	if (UNLIKELY(curnd == NULL)) {
		return res;
	}
	/* check current run and next run's pages */
	for (size_t i = 0; i < curnd->cnt; i++) {
		scom_t sh;
		uint32_t pg = curnd->pgs[i];

		if ((sh = seek_get_scom(sks + pg)) == NULL) {
			continue;
		} else if (sh->u < min) {
			res = pg;
			min = sh->u;
		}
	}
#if defined DEBUG_FLAG
	/* there must be no more minimal pages */
	for (strat_node_t nd = curnd; nd = nd->next; ) {
		for (size_t i = 0; i < nd->cnt; i++) {
			scom_t sh;
			uint32_t pg = nd->pgs[i];

			if ((sh = seek_get_scom(sks + pg)) == NULL) {
				continue;
			}
			if (min > sh->u) {
				UDEBUG("%u cur min %u %zd %lx\n",
				       str->last->pg, curnd->pg, res, min);
				UDEBUG("  min'ner one: %u %u %lx\n",
				       nd->pg, pg, sh->u);
			}
			assert(min <= sh->u);
		}
	}
#endif	/* DEBUG_FLAG */
	return res;
}

static bool
seek_eof_p(uteseek_t sk)
{
	return sk->si * sizeof(*sk->sp) >= sk->sz;
}

static void
step_run(struct uteseek_s sks[], unsigned int run, strat_t str)
{
/* advance the pointer in the RUN-th run and fetch new stuff if need be */
	strat_node_t curnd = str->last;
	scom_t cursc = seek_get_scom(sks + run);
	size_t tsz = scom_tick_size(cursc);

	sks[run].si += tsz;
	if (seek_eof_p(sks + run)) {
		UDEBUG("run %u out of ticks\n", run);
		flush_seek(sks + run);
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
	utectx_t hdl;
	strat_t str;
#if defined DEBUG_FLAG
	uint64_t check = 0ULL;
#endif	/* DEBUG_FLAG */

	/* this is to obtain a merge strategy,
	 * we have several outcomes:
	 * - merge k-way, where k is the number of pages
	 * - merge k-way n-pass, where k < #pages in multiple passes */
	str = sort_strat(ctx);

	for (strat_node_t n = str->first; n; n = n->next) {
		UDEBUG("page %u, considering\n", n->pg);
		for (uint32_t i = 0; i < n->cnt; i++) {
			UDEBUG("  page %u\n", n->pgs[i]);
		}
#if defined DEBUG_FLAG
		uint64_t thresh = 0;
		for (size_t i = 0, tsz; i < sks[n->pg].si; i += tsz) {
			scom_t t = AS_SCOM(sks[n->pg].sp + i);

			assert(thresh <= t->u);
			tsz = scom_tick_size(t);
		}
#endif	/* DEBUG_FLAG */
	}

	/* let's assume we have an all-way merge */
	sks = xnew_array(struct uteseek_s, npages);
	load_runs(sks, ctx, 0, npages);

	hdl = ute_open(ctx->fname, UO_CREAT | UO_TRUNC);

	/* prepare the strategy, we use the last cell as iterator */
	str->last = str->first;
	/* ALL-way merge */
	for (ssize_t j; (j = min_run(sks, npages, str)) >= 0; ) {
		scom_t t = seek_get_scom(sks + j);

#if defined DEBUG_FLAG
		if (check > t->u) {
			UDEBUG("FUCK %lx > %lx\n", check, t->u);
		}
		assert(check <= t->u);
		check = t->u;
#endif	/* DEBUG_FLAG */

		/* add that bloke */
		ute_add_tick(hdl, t);

		/* step the j-th run */
		step_run(sks, (size_t)j, str);
	}

	/* something must have gone utterly wrong */
	assert(ute_sorted_p(hdl));

	/* clone the slut */
	ute_clone_slut(hdl, ctx);

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
