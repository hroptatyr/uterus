/*** ute-print.h -- ute tick printers
 *
 * Copyright (C) 2010-2015 Sebastian Freundt
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
#if !defined INCLUDED_ute_print_h_
#define INCLUDED_ute_print_h_

#include <string.h>
/* for ssize_t */
#include <unistd.h>
#include "scommon.h"
#include "nifty.h"

#define MAX_LINE_LEN		512

typedef struct pr_ctx_s *pr_ctx_t;
typedef const struct pr_opt_s *pr_opt_t;

struct pr_ctx_s {
	/** file index for bad ticks */
	int badfd;
	/** output file index, or -1 for buffer printing */
	int outfd;

	/** overall ute context, will only be used for symbol printing */
	utectx_t uctx;

	char *buf;
	size_t bsz;

	/* options, for finer control */
	pr_opt_t opts;
};

struct pr_opt_s {
	const char *outfile;
};


/* some useful fun */
#if defined INCLUDED_date_h_
static inline size_t
pr_ts(char *restrict buf, uint32_t sec, char sep)
{
	struct tm tm;
	ffff_gmtime(&tm, sec);
	ffff_strftime(buf, 32, &tm, sep);
	return 19;
}

static inline size_t
pr_tsmstz(char *restrict buf, uint32_t sec, uint32_t msec, zif_t z, char sep)
{
	struct tm tm;
	int h, m, off;
	ffff_localtime(&tm, sec, z);
	ffff_strftime(buf, 32, &tm, sep);
	buf[19] = '.';
	buf[20] = (char)(((msec / 100) % 10) + '0');
	buf[21] = (char)(((msec / 10) % 10) + '0');
	buf[22] = (char)(((msec / 1) % 10) + '0');
	/* compute offset as HHMM */
	if ((off = tm.tm_gmtoff) == 0) {;
		buf[23] = '+';
		buf[24] = '0';
		buf[25] = '0';
		buf[26] = ':';
		buf[27] = '0';
		buf[28] = '0';
		goto done;
	} else if (off > 0) {
		h = off / 3600;
		m = (off % 3600) / 60;
		buf[23] = '+';
	} else /* (off < 0) */ {
		off = -off;
		h = off / 3600;
		m = (off % 3600) / 60;
		buf[23] = '-';
	}
	buf[24] = (char)(((h / 10) % 10) + '0');
	buf[25] = (char)(((h / 1) % 10) + '0');
	buf[26] = ':';
	buf[27] = (char)(((m / 10) % 10) + '0');
	buf[28] = (char)(((m / 1) % 10) + '0');
done:
	return 29;
}
#endif	/* INCLUDED_date_h_ */

static inline size_t
pr_sym(utectx_t ctx, char *restrict buf, unsigned int idx)
{
	const char *p;
	size_t pl = 0;

	if (LIKELY((p = ute_idx2sym(ctx, idx)) != NULL)) {
		pl = strlen(p);
		memcpy(buf, p, pl);
	}
	return pl;
}

/* has same signature as a prf */
static inline ssize_t
print_tick_sym(pr_ctx_t pctx, scom_t st)
{
	unsigned int si = scom_thdr_tblidx(st);
	ssize_t res = 0;

	if (LIKELY(pctx->uctx != NULL)) {
		pctx->bsz += res = pr_sym(pctx->uctx, pctx->buf, si);
	}
	return res;
}

/**
 * Public print function.
 * Implemented through DSOs.
 * The return value should indicate whether or not the tick in ST
 * has been processed, a value <0 indicates failure. */
extern int pr(pr_ctx_t pctx, scom_t st);

/**
 * For printers that need initialisation and finalisation. */
extern void init(pr_ctx_t pctx);
extern void fini(pr_ctx_t pctx);

/**
 * Same as `init()' but also hand over command line args. */
extern int init_main(pr_ctx_t pctx, int argc, char *argv[]);

#endif	/* INCLUDED_ute_print_h_ */
