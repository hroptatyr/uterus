/*** uta.c -- ute ascii format
 *
 * Copyright (C) 2012  Sebastian Freundt
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

/* hope it's ours, the one that defines index_t */
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include "secu.h"
#include "date.h"
#include "sl1t.h"
#include "utefile.h"
/* for public demux (print) apis, muxing isn't possible yet */
#include "ute-print.h"

#if defined USE_DEBUGGING_ASSERTIONS
# include <assert.h>
#endif	/* USE_DEBUGGING_ASSERTIONS */
#define DEFINE_GORY_STUFF
#include "m30.h"
#include "m62.h"

#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif	/* !UNLIKELY */
#if !defined UNUSED
# define UNUSED(_x)	__attribute__((unused)) _x
#endif	/* !UNUSED */

#define MAX_LINE_LEN	512
#define countof(x)	(sizeof(x) / sizeof(*x))

/* 'nother type extension */
#define NSYMS	(16384)
struct metrsymtbl_s {
	/* fuck ugly, this points into kernel space */
	utehdr_t st;
	/* caches */
	m30_t tra[NSYMS];
	m30_t tsz[NSYMS];
	m30_t bid[NSYMS];
	m30_t bsz[NSYMS];
	m30_t ask[NSYMS];
	m30_t asz[NSYMS];
};


/* convenience macroes for the additional tables */
#define SYMTBL_SEC	(symtbl.st->sec)
#define SYMTBL_TRA	(symtbl.tra)
#define SYMTBL_TSZ	(symtbl.tsz)
#define SYMTBL_BID	(symtbl.bid)
#define SYMTBL_BSZ	(symtbl.bsz)
#define SYMTBL_ASK	(symtbl.ask)
#define SYMTBL_ASZ	(symtbl.asz)

#if defined USE_DEBUGGING_ASSERTIONS
static __attribute__((unused)) void
fputn(FILE *whither, const char *p, size_t n)
{
	for (const char *endp = p + n; p < endp; p++) {
		fputc(*p, whither);
	}
	return;
}
#endif	/* USE_DEBUGGING_ASSERTIONS */


/* public functions, muxer and demuxer, but the muxer doesn't work yet */
#if 0
void
uta_slab(mux_ctx_t ctx)
{
	return;
}
#endif	/* 0 */

ssize_t
uta_pr(pr_ctx_t pctx, scom_t st)
{
	char tl[MAX_LINE_LEN];
	const_sl1t_t t = (const void*)st;
	uint32_t sec = sl1t_stmp_sec(t);
	uint16_t msec = sl1t_stmp_msec(t);
	uint16_t ttf = sl1t_ttf(t);
	ssize_t res;
	char *p;

	/* equip or print context with buffers and whatnot */
	pctx->buf = tl;
	pctx->bsz = sizeof(tl);
	if ((res = print_tick_sym(pctx, st)) > 0) {
		p = tl + res;
	} else {
		/* great, no symbol entry, what are we gonna do? */
		p = tl;
		*p++ = '0';
	}
	*p++ = '\t';
	p += pr_tsmstz(p, sec, msec, NULL, 'T');
	*p++ = '\t';
	/* tick type */
#pragma warning (disable:2259)
	*p++ = ttf < 10 ? ttf + '0' : ttf - 10 + 'a';
#pragma warning (default:2259)
	*p++ = '\t';
	/* index into the sym table */
	p += sprintf(p, "%x", sl1t_tblidx(t));
	/* sequence is always 0 */
	*p++ = '\t';
	switch (ttf) {
	case SL1T_TTF_BID:
	case SL1T_TTF_ASK:
	case SL1T_TTF_TRA:
	case SL1T_TTF_FIX:
	case SL1T_TTF_STL:
	case SL1T_TTF_AUC:
		/* price value */
		p += ffff_m30_s(p, (m30_t)t->v[0]);
		*p++ = '\t';
		/* size value */
		p += ffff_m30_s(p, (m30_t)t->v[1]);
		break;
	case SL1T_TTF_VOL:
	case SL1T_TTF_VPR:
	case SL1T_TTF_OI:
		/* just one huge value, will there be a m62? */
		p += ffff_m62_s(p, (m62_t)t->w[0]);
		break;
	case SL1T_TTF_UNK:
	default:
		break;
	}
	*p++ = '\n';
	*p = '\0';

	/* and off we go */
	write(pctx->outfd, tl, res = p - tl);
	return res;
}

/* uta.c ends here */
