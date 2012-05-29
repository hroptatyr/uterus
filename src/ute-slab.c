/*** ute-slab.c -- cut slabs of tick types, symbols or stamp ranges
 *
 * Copyright (C) 2012 Sebastian Freundt
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

/** test client for libuterus */
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#include "utefile.h"

/* we're just as good as rudi, aren't we? */
#if defined DEBUG_FLAG
# include <assert.h>
#else  /* !DEBUG_FLAG */
# define assert(args...)
#endif	/* DEBUG_FLAG */

#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif	/* !UNLIKELY */
#if !defined UNUSED
# define UNUSED(_x)	__attribute__((unused)) _x
#endif	/* !UNUSED */
#if !defined countof
# define countof(x)	(sizeof(x) / sizeof(*x))
#endif	/* !countof */

typedef struct slab_ctx_s *slab_ctx_t;

struct slab_ctx_s {
	utectx_t out;

	size_t nidxs;
	const unsigned int *idxs;

	size_t nsyms;
	const char *const *syms;

	/* interval [from, till) for time-based extraction */
	time_t from;
	time_t till;
};

static void
__attribute__((format(printf, 2, 3)))
error(int eno, const char *fmt, ...)
{
	va_list vap;
	va_start(vap, fmt);
	vfprintf(stderr, fmt, vap);
	va_end(vap);
	if (eno || errno) {
		fputc(':', stderr);
		fputc(' ', stderr);
		fputs(strerror(eno ?: errno), stderr);
	}
	fputc('\n', stderr);
	return;
}

/* bitsets */
typedef struct bitset_s *bitset_t;

struct bitset_s {
	size_t sz;
	/* the actual set */
	long unsigned int *bs;
#if defined USE_BITCOUNT_ARR
	/* bit counts for each bucket */
	uint8_t *bc;
#endif	/* USE_BITCOUNT_ARR */
};

static bitset_t
make_bitset(size_t bits)
{
	static struct bitset_s pool[2] = {{0}, {0}};
	/* round robin var */
	static size_t rr = 0UL;
	const size_t rd = sizeof(*pool->bs) * 8/*bits/byte*/;
	bitset_t res;

	if (bits == 0) {
		return NULL;
	}
	/* round bits up to the next multiple */
	bits = bits / rd + 1;

	/* get ourselves a result bitset */
	res = pool + ((rr++) % countof(pool));
	/* check for resizes */
	if (bits > res->sz) {
		/* resize */
		free(res->bs);
		res->bs = calloc(bits, sizeof(*res->bs));

#if defined USE_BITCOUNT_ARR
		free(res->bc);
		res->bc = calloc(bits, sizeof(*res->bc));
#endif	/* USE_BITCOUNT_ARR */

		res->sz = bits;
	} else {
		/* wipe */
		memset(res->bs, 0, res->sz * sizeof(*res->bs));
#if defined USE_BITCOUNT_ARR
		memset(res->bc, 0, res->sz * sizeof(*res->bc));
#endif	/* USE_BITCOUNT_ARR */
	}
	return res;
}

static void
bitset_set(bitset_t bs, size_t bit)
{
	const size_t rd = sizeof(*bs->bs) * 8/*bits/byte*/;
	size_t word = bit / rd;
	size_t shft = bit % rd;

	bs->bs[word] |= (1UL << shft);
	return;
}

static int
bitset_get(bitset_t bs, size_t bit)
{
	const size_t rd = sizeof(*bs->bs) * 8/*bits/byte*/;
	size_t word = bit / rd;
	size_t shft = bit % rd;

	return (bs->bs[word] & (1UL << shft)) ? 1 : 0;
}

#define BITSET_LOOP(_x, _n)						\
	for (size_t __i = 0, _n = 0;					\
	     __i < (_x)->sz;						\
	     __i++, _n = __i * sizeof(*(_x)->bs) * 8/*bits/byte*/)	\
		for (long unsigned int __cell = (_x)->bs[__i];		\
		     __cell; __cell >>= 1, _n++)			\
			if (!(__cell & 1)) {				\
				continue;				\
			} else

/* date fiddling */
static time_t
get_date(const char *s)
{
	struct tm tm = {0};
	strptime(s, "%F", &tm);
	return mktime(&tm);
}

static time_t
get_stamp(const char *s)
{
	struct tm tm = {0};
	strptime(s, "%FT%T", &tm);
	return mktime(&tm);
}


static utectx_t
open_out(const char *fn, int fl)
{
	if (fn) {
		return ute_open(fn, fl);
	}
	return ute_mktemp(fl);
}

static void
slab1(slab_ctx_t ctx, utectx_t hdl)
{
	static size_t max_idx = 0UL;
	size_t hdl_nsyms;
	/* bitset for the index filter */
	bitset_t filtix = NULL;
	/* bitset with copied tick indices */
	bitset_t copyix = NULL;

	if (max_idx == 0UL) {
		/* find the maximum index in ctx->idxs */
		for (size_t i = 0; i < ctx->nidxs; i++) {
			if (ctx->idxs[i] > max_idx) {
				max_idx = ctx->idxs[i];
			}
		}
	}
	/* dont bother checking for the largest one, just take nsyms */
	hdl_nsyms = ute_nsyms(hdl);
	if (max_idx < hdl_nsyms && ctx->nsyms > 0) {
		max_idx = hdl_nsyms;
	}
	if (max_idx) {
		/* get ourselves a bitset, won't be freed, so there's a leak! */
		filtix = make_bitset(max_idx);
	} else {
		/* or another one to keep track of to-bang symidxs */
		copyix = make_bitset(hdl_nsyms);
	}
	/* set the bits from the idx */
	for (size_t i = 0; i < ctx->nidxs; i++) {
		/* it's unclear what the final name in the outfile should be */
		uint16_t idx = (uint16_t)ctx->idxs[i];
		const char *sym = ute_idx2sym(hdl, idx);
		bitset_set(filtix, idx);
		ute_bang_symidx(ctx->out, sym, idx);
	}
	/* transform and set the rest */
	for (size_t i = 0; i < ctx->nsyms; i++) {
		const char *sym = ctx->syms[i];
		uint16_t idx = ute_sym2idx(hdl, sym);
		bitset_set(filtix, idx);
		ute_bang_symidx(ctx->out, sym, idx);
	}

	for (size_t i = 0, tsz; i < ute_nticks(hdl); i += tsz) {
		scom_t ti = ute_seek(hdl, i);
		uint16_t idx = scom_thdr_tblidx(ti);
		time_t stmp = scom_thdr_sec(ti);

		/* first off, set the tick size */
		tsz = scom_tick_size(ti);

		/* chain of filters, first one loses */
		if (max_idx && (idx > max_idx || !bitset_get(filtix, idx))) {
			/* index no matchee */
			continue;
		} else if (stmp < ctx->from || stmp >= ctx->till) {
			/* time stamp no matchee */
			continue;
		}

		if (max_idx == 0) {
			/* set the bit in the copyix bitset
			 * coz we're now interested in banging the symbol */
			bitset_set(copyix, idx);
		}
		/* we passed all them tests, just let him through */
		ute_add_tick(ctx->out, ti);
	}

	if (max_idx == 0) {
		BITSET_LOOP(copyix, i) {
			uint16_t idx = i;
			const char *sym = ute_idx2sym(hdl, idx);
			ute_bang_symidx(ctx->out, sym, idx);
		}
	}
	return;
}


#if defined STANDALONE
#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#endif	/* __INTEL_COMPILER */
#include "ute-slab-clo.h"
#include "ute-slab-clo.c"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
# pragma warning (default:181)
#endif	/* __INTEL_COMPILER */

int
main(int argc, char *argv[])
{
	struct slab_args_info argi[1];
	struct slab_ctx_s ctx[1] = {{0}};
	const char *outfn = NULL;
	int outfl = UO_RDWR;
	int res = 0;

	if (slab_parser(argc, argv, argi)) {
		res = 1;
		goto out;
	} else if (argi->help_given) {
		slab_parser_print_help();
		res = 0;
		goto out;
	}

	if (argi->extract_symbol_given) {
		ctx->syms = argi->extract_symbol_arg;
		/* quick count */
		for (const char *const *p = ctx->syms; *p; p++, ctx->nsyms++);
	}
	if (argi->extract_symidx_given) {
		ctx->idxs = (unsigned int*)argi->extract_symidx_arg;
		/* quick count */
		for (const unsigned int *p = ctx->idxs; *p; p++, ctx->nidxs++);
	}

	/* time based extraction */
	if (argi->extract_day_given) {
		ctx->from = get_date(argi->extract_day_arg);
		ctx->till = ctx->from + 86400;
	}
	if (argi->extract_from_given) {
		ctx->from = get_stamp(argi->extract_from_arg);
	}
	if (argi->extract_till_given) {
		ctx->till = get_stamp(argi->extract_till_arg);
	}
	/* set defaults */
	if (ctx->from == 0) {
		ctx->from = -2147483648;
	}
	if (ctx->till == 0) {
		ctx->till = 2147483647;
	}

	/* handle outfile */
	if (argi->output_given && argi->into_given) {
		error(0, "only one of --output and --into can be given");
		res = 1;
		goto out;
	} else if (argi->output_given) {
		outfn = argi->output_arg;
		outfl |= UO_CREAT | UO_TRUNC;
	} else if (argi->into_given) {
		outfn = argi->into_arg;
		outfl |= UO_CREAT;
	} else {
		outfn = NULL;
	}

	if ((ctx->out = open_out(outfn, outfl)) == NULL) {
		error(0, "cannot open output file '%s'", outfn);
		res = 1;
		goto out;
	} else if (outfn == NULL) {
		/* inform the user about our filename decision */
		puts(ute_fn(ctx->out));
	}

	for (unsigned int i = 0; i < argi->inputs_num; i++) {
		/* just quickly do it here */
		const char *fn = argi->inputs[i];
		const int fl = UO_RDONLY;
		utectx_t hdl;

		if ((hdl = ute_open(fn, fl)) == NULL) {
			error(0, "cannot open file '%s'", fn);
			continue;
		}

		/* slab some stuff out of hdl */
		slab1(ctx, hdl);

		/* we worship the ute god by giving back what belongs to him */
		ute_close(hdl);
	}

	/* clear out resources */
	ute_close(ctx->out);

out:
	slab_parser_free(argi);
	return res;
}
#endif	/* STANDALONE */

/* ute-slab.c ends here */
