/*** utesort.c -- low-level sorting guts
 *
 * Copyright (C) 2010-2013 Sebastian Freundt
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

#define UTESORT_C
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stddef.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#if defined HAVE_SYS_TYPES_H
/* for ssize_t */
# include <sys/types.h>
#endif	/* HAVE_SYS_TYPES_H */
#include <fcntl.h>
#include <errno.h>
#include "utefile-private.h"
#include "utefile.h"
#include "utehdr.h"
#include "utetpc.h"
#include "mem.h"
#include "nifty.h"

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
/* for serious debugging */
#if !defined UDEBUGv
# define UDEBUGv(args...)
#endif	/* !UDEBUGv */
#if !defined UDEBUGvv
# define UDEBUGvv(args...)
#endif	/* !UDEBUGvv */


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
	union {
		strat_node_t last;
		strat_node_t curr;
	};
};

struct sks_s {
	size_t nsks;
	struct uteseek_s *sks;
	/* bit set for pages that have been proc'd already */
	size_t npgbs;
	unsigned int *pgbs;
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
		UDEBUG("page %u is subset of %zu other pages\n", pg, cc->cnt);
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

static sidx_t MAYBE_NOINLINE
load_runs(uteseek_t sks, utectx_t ctx, sidx_t sta, sidx_t end, size_t npg)
{
	size_t e = min_size_t(end, npg);

	UDEBUGv("k <- %zu - %zu  (%zu)\n", sta, e, e - sta);
	for (size_t k = sta, j = 0; k < e; j++, k++) {
		/* set up page i */
		if (seek_page(sks + j, ctx, k) < 0) {
			UDEBUGv("UHOH seek page %zu no succeedee: %s\n",
				k, strerror(errno));
			e = k;
			break;
		}
	}

#if defined DEBUG_FLAG
	for (size_t k = sta, j = 0; k < e; j++, k++) {
		const size_t sks_nticks = sks[j].szrw / sizeof(*sks->sp);
		uint64_t thresh = 0;

		UDEBUGv("sks[%zu].si = %zu/%zu\n", j, sks[j].si, sks_nticks);
		for (sidx_t i = sks[j].si, tsz; i < sks_nticks; i += tsz) {
			scom_t t = AS_SCOM(sks[j].sp + i);

			/* the seeker should not give us trailing naughts */
			assert(t->u);
			assert((t->ttf & 0x30U) != 0x30U);
			assert(thresh <= t->u);
			thresh = t->u;
			tsz = scom_tick_size(t);
		}
	}
#endif	/* DEBUG_FLAG */
	return e;
}

static void
dump_runs(uteseek_t sks, utectx_t UNUSED(ctx), sidx_t s, sidx_t e, size_t npg)
{
	e = min_size_t(e, npg);
	for (size_t i = 0, k = s; k < e; i++, k++) {
		/* set up page i */
		flush_seek(sks + i);
	}
	return;
}

static strat_t MAYBE_NOINLINE
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

	UDEBUG("generating a sort strategy for %zu (+%d) pages\n",
	       npages, tpc_has_ticks_p(ctx->tpc));
	npages += tpc_has_ticks_p(ctx->tpc);
	for (size_t j = 0; j < npages; j += NRUNS) {
		/* initialise the seeks */
		load_runs(sks, ctx, j, j + NRUNS, npages);

		/* obtain intervals */
		for (size_t i = 0, k = j; i < NRUNS && k < npages; i++, k++) {
			scom_t sb = seek_get_scom(sks + i);
			scom_t se = seek_last_scom(sks + i);

			assert(sb && se);
			assert(sb->u <= se->u);
			itree_add(it, sb->u, se->u, AS_VOID_PTR(k));
		}

		/* finish off the seeks */
		dump_runs(sks, ctx, j, j + NRUNS, npages);
	}
	/* run the strategy evaluator */
	s = xnew(struct strat_s);
	s->first = s->last = NULL;
	s->it = it;
	/* load the strat cb closure */
	sc->strat = s;
	itree_trav_in_order(it, __strat_cb, sc);

	/* check if the strategy actually produced results */
	if (s->first == NULL) {
		/* this is actually possible when all pages have the same
		 * min and max value, so they're all supseteq's of each other
		 * anyway, in this case we just add the first page to the
		 * strategy and the other ones as children */
		strat_node_t sn;

		assert(s->last == NULL);
		sn = xmalloc(sizeof(*sn) + npages * sizeof(int));
		sn->pg = 0;
		sn->cnt = npages;
		sn->next = NULL;
		for (unsigned int i = 0; i < npages; i++) {
			sn->pgs[i] = i;
		}
		/* actually attach the cell to our strategy */
		s->first = s->last = sn;
	}

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

static int
drop_run(struct sks_s s[static 1], sidx_t j)
{
	size_t ns = s->nsks;

	assert(j < ns);

	/* first off, flushing, thoroughly */
	flush_seek(s->sks + j);

	if (UNLIKELY(j + 1U >= ns)) {
		/* bit of a short cut, no need to move */
		goto succ;
	}
	/* memmove around the whole */
	UDEBUGv("moving [%zu..%zu] <- [%zu..%zu]\n", j, ns - 1, j + 1U, ns);
	memmove(s->sks + j, s->sks + j + 1U, (ns - (j + 1U)) * sizeof(*s->sks));
succ:
	s->nsks--;
	return 0;
}

static int
sks_pgbs_get(struct sks_s s[static 1], uint32_t pg)
{
	static const size_t pgbs_w = sizeof(*s->pgbs) * 8U;
	size_t idx = pg / pgbs_w;
	size_t bit = pg % pgbs_w;

	return (s->pgbs[idx] >> bit) & 0x01;
}

static void
sks_pgbs_set(struct sks_s s[static 1], uint32_t pg)
{
	static const size_t pgbs_w = sizeof(*s->pgbs) * 8U;
	size_t idx = pg / pgbs_w;
	size_t bit = pg % pgbs_w;

	s->pgbs[idx] |= 1U << bit;
	return;
}

static int
node_has_page_p(strat_node_t nd, uint32_t pg)
{
	for (size_t i = 0; i < nd->cnt; i++) {
		if (nd->pgs[i] == pg) {
			return 1;
		}
	}
	return 0;
}

static int
load_run(struct sks_s s[static 1], utectx_t ctx, strat_node_t nd)
{
/* return new size of seek vector S */
	size_t ns = s->nsks;

	UDEBUGv("loading run %u (%u pgs)\n", nd->pg, nd->cnt);
	for (size_t i = 0; i < nd->cnt; i++) {
		/* set up page i */
		uint32_t pg = nd->pgs[i];

		if (sks_pgbs_get(s, pg)) {
			/* do nothing */
			UDEBUGv("sks have/had pg %u already\n", pg);
		} else if (seek_page(s->sks + ns++, ctx, pg) < 0) {
			UDEBUGv("UHOH seek page %u no succeedee: %s\n",
				pg, strerror(errno));
			ns--;
			break;
		} else {
			sks_pgbs_set(s, pg);
		}
	}

#if defined DEBUG_FLAG
	for (size_t i = s->nsks; i < ns; i++) {
		const size_t sks_nticks = s->sks[i].szrw / sizeof(*s->sks->sp);
		uint64_t thresh = 0;

		UDEBUGv("sks[%zu (pg %u)].si = %zu/%zu\n",
			i, s->sks[i].pg, s->sks[i].si, sks_nticks);
		for (sidx_t j = s->sks[i].si, tsz; j < sks_nticks; j += tsz) {
			scom_t t = AS_SCOM(s->sks[i].sp + j);

			/* the seeker should not give us trailing naughts */
			assert(t->u);
			assert((t->ttf & 0x30U) != 0x30U);
			assert(thresh <= t->u);
			thresh = t->u;
			tsz = scom_tick_size(t);
		}
	}
#endif	/* DEBUG_FLAG */
	s->nsks = ns;
	return 0;
}

static sidx_t
min_run(struct sks_s s[static 1])
{
	sidx_t res = (sidx_t)-1;
	uint64_t min = ULLONG_MAX;

	/* check current run and next run's pages */
	for (sidx_t i = 0; i < s->nsks; i++) {
		scom_t sh;

		if ((sh = seek_get_scom(s->sks + i)) == NULL) {
			continue;
		} else if (sh->u <= min) {
			res = i;
			min = sh->u;
		}
	}
	/* we used to compare the current min value with all values
	 * in later pages, however, we can't do this check anymore,
	 * as not all pages are currently loaded */
	return res;
}

static bool
seek_eof_p(uteseek_t sk)
{
	return sk->si >= sk->szrw / sizeof(*sk->sp);
}

static int
step_run(struct sks_s s[static 1], utectx_t ctx, strat_t str, size_t j)
{
/* advance the pointer in the j-th run and fetch new stuff if need be */
	strat_node_t curnd = str->curr;
	uteseek_t skj = s->sks + j;
	scom_t cursc = seek_get_scom(skj);
	size_t tsz = scom_tick_size(cursc);
	uint32_t pgj = skj->pg;

	assert(s->nsks > 0);

	skj->si += tsz;
	if (seek_eof_p(skj)) {
		UDEBUG("idx %zu (pg %u) out of ticks\n", j, pgj);
		UDEBUGv("B %zu seeks under surveillance\n", s->nsks);

		/* more pages need loading if the currently dropped page is
		 * mentioned in the next strat node, or if there's no more
		 * seeks in sks but there's a next strat node */
		if (UNLIKELY(curnd->next == NULL)) {
			/* we're finished, yay! */
			;
		} else if (drop_run(s, j) == 0U && s->nsks == 0U ||
			   node_has_page_p(curnd->next, pgj)) {
			/* load moar (and advance the current strat node) */
			str->curr = curnd = curnd->next;
			if (load_run(s, ctx, curnd) < 0) {
				/* FUCK! */
				return -1;
			}
		}
		UDEBUGv("A %zu seeks under surveillance\n", s->nsks);
	}
	return 0;
}

void
ute_sort(utectx_t ctx)
{
	struct sks_s s[1];
	size_t npg = ute_npages(ctx);
	utectx_t hdl;
	strat_t str;
#if defined DEBUG_FLAG
	uint64_t check = 0ULL;
	size_t ntadd = 0;
#endif	/* DEBUG_FLAG */

	/* tpc might be in there as well */
	npg += tpc_has_ticks_p(ctx->tpc);

	/* this is to obtain a merge strategy,
	 * we have several outcomes:
	 * - merge k-way, where k is the number of pages
	 * - merge k-way n-pass, where k < #pages in multiple passes */
	str = sort_strat(ctx);

	/* get the highest CNT */
	size_t nmaxpg = 0U;
	for (strat_node_t n = str->first; n; n = n->next) {
		UDEBUG("page %u, considering\n", n->pg);
		if (n->cnt > nmaxpg) {
			nmaxpg = n->cnt;
		}
		for (uint32_t i = 0; i < n->cnt; i++) {
			UDEBUG("  page %u\n", n->pgs[i]);
		}
	}
	UDEBUG("max %zu pages need considering simultaneously\n", nmaxpg);

	/* we merge all of a strat node's pages simultaneously,
	 * let's assume that an NMAXPG-merge is possible */
	s->sks = xnew_array(struct uteseek_s, nmaxpg);
	s->nsks = 0U;
	s->npgbs = npg;
	s->pgbs = calloc(npg / sizeof(*s->pgbs) / 8U + 1U, sizeof(*s->pgbs));

	with (uint16_t oflags = UO_CREAT | UO_TRUNC) {
		if (UNLIKELY(ctx->oflags & UO_ANON)) {
			/* ok, we could short cut it here if the file's
			 * about to be deleted anyway ...
			 * maybe later */
			oflags |= UO_ANON;
		}
		hdl = ute_open(ctx->fname, oflags);
	}

	/* prepare the strategy */
	if (load_run(s, ctx, str->curr = str->first) < 0) {
		/* big bugger */
		abort();
	}
	for (ssize_t j;
	     /* index of the minimal page in the current sks set */
	     (j = min_run(s)) >= 0;
	     /* step the j-th run */
	     step_run(s, ctx, str, (size_t)j)) {
		scom_t t = seek_get_scom(s->sks + j);

#if defined DEBUG_FLAG
		if (check > t->u) {
			UDEBUG("FUCK %lx > %lx\n", check, t->u);
		}
		assert(check <= t->u);
		check = t->u;
		ntadd++;
#endif	/* DEBUG_FLAG */

		/* add that bloke */
		ute_add_tick(hdl, t);
	}

	/* lest something's gone utterly wrong */
	assert(ute_sorted_p(hdl));
	assert(s->nsks == 0U);

	UDEBUG("added %zu ticks\n", ntadd);

	/* clone the slut */
	ute_clone_slut(hdl, ctx);

	/* close the ute file */
	ute_close(hdl);

	/* free the strategy */
	free_strat(str);
	free(s->sks);
	free(s->pgbs);
	return;
}

/* utesort.c ends here */
