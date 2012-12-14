/*** ute-info.c -- ute file checker
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
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include "utefile-private.h"
#include "utefile.h"
#include "scommon.h"
#include "sl1t.h"
#include "scdl.h"
#include "ssnp.h"
#include "mem.h"

#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif	/* !UNLIKELY */
#if !defined UNUSED
# define UNUSED(_x)	__attribute__((unused)) _x
#endif	/* !UNUSED */
#if !defined countof
# define countof(x)	(sizeof(x) / sizeof(*x))
#endif	/* !countof */

/* one day verbpr() might become --verbose */
#if defined DEBUG_FLAG
# include <assert.h>
# define UDEBUG(args...)	fprintf(stderr, args)
# define UNUSED_dbg(x)		UNUSED(x)
# define UNUSED_nodbg(x)	x
#else
# define UDEBUG(args...)
# define UNUSED_dbg(x)		x
# define UNUSED_nodbg(x)	UNUSED(x)
#endif	/* DEBUG_FLAG */

typedef struct info_ctx_s *info_ctx_t;

struct info_ctx_s {
	bool verbp:1;
};


/* helper functions */
static void
__attribute__((format(printf, 1, 2), unused))
verbprf(const char *UNUSED_nodbg(fmt), ...)
{
#if defined DEBUG_FLAG
	va_list vap;
	va_start(vap, fmt);
	vfprintf(stderr, fmt, vap);
	va_end(vap);
#endif	/* DEBUG_FLAG */
	return;
}

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

typedef void *bset_t;

static bset_t
make_bset(void)
{
	bset_t res = mmap(NULL, 8192U, PROT_MEM, MAP_MEM, -1, 0);
	return LIKELY(res != MAP_FAILED) ? res : NULL;
}

static void
free_bset(bset_t bs)
{
	munmap(bs, 8192U);
	return;
}

#if !defined CHAR_BIT
# define CHAR_BIT	(8U)
#endif	/* CHAR_BIT */

static void
bset_set(bset_t bs, unsigned int bno)
{
/* set BNO-th bit in BS. */
	long unsigned int m = bno / (sizeof(long unsigned int) * CHAR_BIT);
	long unsigned int o = bno % (sizeof(long unsigned int) * CHAR_BIT);
	long unsigned int *blu = bs;
	blu[m] |= (1UL << o);
	return;
}

#define BSET_ITER(i, __bs)						\
	for (size_t _i_ = 0; _i_ < 8192U / sizeof(long unsigned int); _i_++) \
		for (long unsigned _b_ = ((long unsigned int*)bs)[_i_];	\
		     _b_; _b_ = 0)		\
			for (unsigned int i = _i_ * sizeof(_b_) * CHAR_BIT, \
				     _j_ = 0;				\
			     _j_ < sizeof(_b_) * CHAR_BIT; _j_++, i++)	\
				if ((_b_) & (1UL << (_j_)))

static void
pr_intv(int i)
{
	if (i <= 0 && i > 86400) {
		printf("???");
	} else if (i == 86400) {
		printf("eod");
	} else if (i >= 3600 && !(i % 3600)) {
		printf("%02dh", i / 3600);
	} else if (i >= 60 && !(i % 60)) {
		printf("%02dm", i / 60);
	} else {
		printf("%02ds", i);
	}
}


/* page wise operations */
static int
snarf_ttf(info_ctx_t UNUSED(ctx), uteseek_t sk, uint16_t sym)
{
	static int intv[16];
	const size_t ssz = sizeof(*sk->sp);
	const size_t sk_sz = seek_byte_size(sk);
	uint32_t ttfs = 0U;

	memset(intv, 0, sizeof(intv));
	for (size_t i = sk->si * ssz, tz; i < sk_sz; i += tz) {
		scom_t ti = AS_SCOM(sk->sp + i / ssz);

		if (scom_thdr_tblidx(ti) == sym) {
			ttfs |= (1U << scom_thdr_ttf(ti));
		}
		if (scom_thdr_ttf(ti) > SCDL_FLAVOUR) {
			/* try and get interval */
			const_scdl_t x = AS_CONST_SCDL(ti);
			int this = x->hdr->sec - x->sta_ts;

#define INTV(x)		(intv[scom_thdr_ttf(x) & 0xf])
			if (!INTV(ti)) {
				/* always store */
				INTV(ti) = this;
			} else if (INTV(ti) != this) {
				/* sort of reset */
				INTV(ti) = -1;
			}
#undef INTV
		}

		/* determine the length for the increment */
		tz = scom_byte_size(ti);
	}

	if (ttfs & (1U << SCOM_TTF_UNK)) {
		printf("\tunk");
	}
	if (ttfs & (1U << SL1T_TTF_BID)) {
		printf("\ttick_b");
	}
	if (ttfs & (1U << SL1T_TTF_ASK)) {
		printf("\ttick_a");
	}
	if (ttfs & (1U << SL1T_TTF_TRA)) {
		printf("\ttick_t");
	}
	if (ttfs & (1U << SSNP_FLAVOUR)) {
		printf("\ts???");
	}
	if (ttfs & (1U << (SCDL_FLAVOUR | SL1T_TTF_BID))) {
		printf("\tc");
		pr_intv(intv[SL1T_TTF_BID]);
		printf("_b");
	}
	if (ttfs & (1U << (SCDL_FLAVOUR | SL1T_TTF_ASK))) {
		printf("\tc");
		pr_intv(intv[SL1T_TTF_ASK]);
		printf("_a");
	}
	if (ttfs & (1U << (SCDL_FLAVOUR | SL1T_TTF_TRA))) {
		printf("\tc");
		pr_intv(intv[SL1T_TTF_TRA]);
		printf("_t");
	}
	return 0;
}

/* the actual infoing */
static int
infop(info_ctx_t ctx, uteseek_t sk, utectx_t hdl)
{
	const size_t ssz = sizeof(*sk->sp);
	const size_t sk_sz = seek_byte_size(sk);
	bset_t bs;

	if (UNLIKELY((bs = make_bset()) == NULL)) {
		return -1;
	}
	for (size_t i = sk->si * ssz, tz; i < sk_sz; i += tz) {
		scom_thdr_t ti = AS_SCOM_THDR(sk->sp + i / ssz);

		bset_set(bs, scom_thdr_tblidx(ti));

		/* determine the length for the increment */
		tz = scom_byte_size(ti);
	}

	BSET_ITER(i, bs) {
		printf("%s", ute_idx2sym(hdl, i));
		snarf_ttf(ctx, sk, i);
		putchar('\n');
	}

	free_bset(bs);
	return 0;
}


/* file wide operations */
static int
info1(info_ctx_t ctx, utectx_t hdl, const char *UNUSED(fn))
{
	size_t npg;

	/* go through the pages manually */
	npg = ute_npages(hdl);
	if (ctx->verbp) {
		printf("pages\t%zu\n", npg);
	}
	for (size_t p = 0; p < 1U/*assume 1st page info only*/; p++) {
		struct uteseek_s sk[1];

		/* create a new seek */
		seek_page(sk, hdl, p);
		/* info that one page */
		infop(ctx, sk, hdl);
		/* flush the old seek */
		flush_seek(sk);
	}
	return 0;
}


#if defined STANDALONE
#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#endif	/* __INTEL_COMPILER */
#include "ute-info-clo.h"
#include "ute-info-clo.c"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
# pragma warning (default:181)
#endif	/* __INTEL_COMPILER */

int
main(int argc, char *argv[])
{
	struct info_args_info argi[1];
	struct info_ctx_s ctx[1] = {0};
	int res = 0;

	if (info_parser(argc, argv, argi)) {
		res = 1;
		goto out;
	} else if (argi->help_given) {
		info_parser_print_help();
		res = 0;
		goto out;
	}

	/* copy interesting stuff into our own context */
	if (argi->verbose_given) {
		ctx->verbp = true;
	}

	for (unsigned int j = 0; j < argi->inputs_num; j++) {
		const char *fn = argi->inputs[j];
		const int fl = UO_RDONLY | UO_NO_LOAD_TPC;
		utectx_t hdl;

		if ((hdl = ute_open(fn, fl)) == NULL) {
			error(0, "cannot open file `%s'", fn);
			res = 1;
			continue;
		}

		/* the actual checking */
		if (info1(ctx, hdl, fn)) {
			res = 1;
		}

		/* and that's us */
		ute_close(hdl);
	}
out:
	info_parser_free(argi);
	return res;
}
#endif	/* STANDALONE */

/* ute-info.c ends here */
