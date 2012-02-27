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
#include "date.h"
#include "utefile.h"
/* for public demux (print) apis, muxing isn't possible yet */
#include "ute-print.h"

/* so we know about ticks, candles and snapshots */
#include "sl1t.h"
#include "scdl.h"

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

static size_t
__pr_snap(char *tgt, scom_t st)
{
	const_ssnap_t snp = (const void*)st;
	char *p = tgt;

	/* bid price */
	p += ffff_m30_s(p, (m30_t)snp->bp);
	*p++ = '\t';
	/* ask price */
	p += ffff_m30_s(p, (m30_t)snp->ap);
	*p++ = '\t';
	/* bid quantity */
	p += ffff_m30_s(p, (m30_t)snp->bq);
	*p++ = '\t';
	/* ask quantity */
	p += ffff_m30_s(p, (m30_t)snp->aq);
	*p++ = '\t';
	/* volume-weighted trade price */
	p += sprintf(p, "%08x", snp->tvpr);
	*p++ = '|';
	p += ffff_m30_s(p, (m30_t)snp->tvpr);
	*p++ = '\t';
	/* trade quantity */
	p += sprintf(p, "%08x", snp->tq);
	*p++ = '|';
	p += ffff_m30_s(p, (m30_t)snp->tq);
	return p - tgt;
}

static size_t
__pr_cdl(char *tgt, scom_t st)
{
	const_scdl_t cdl = (const void*)st;
	char *p = tgt;

	/* h(igh) */
	p += ffff_m30_s(p, (m30_t)cdl->h);
	*p++ = '\t';
	/* l(ow) */
	p += ffff_m30_s(p, (m30_t)cdl->l);
	*p++ = '\t';
	/* o(pen) */
	p += ffff_m30_s(p, (m30_t)cdl->o);
	*p++ = '\t';
	/* c(lose) */
	p += ffff_m30_s(p, (m30_t)cdl->c);
	*p++ = '\t';
	/* start of the candle */
	p += sprintf(p, "%08x", cdl->sta_ts);
	*p++ = '|';
	p += pr_tsmstz(p, cdl->sta_ts, 0, NULL, 'T');
	*p++ = '\t';
	/* event count in candle, print 3 times */
	p += sprintf(p, "%08x", cdl->cnt);
	*p++ = '|';
	p += ffff_m30_s(p, (m30_t)cdl->cnt);
	return p - tgt;
}


/* public functions, muxer and demuxer, but the muxer doesn't work yet */
#if 0
void
uta_slab(mux_ctx_t ctx)
{
	return;
}
#endif	/* 0 */

ssize_t
pr(pr_ctx_t pctx, scom_t st)
{
	char tl[MAX_LINE_LEN];
	uint32_t sec = scom_thdr_sec(st);
	uint16_t msec = scom_thdr_msec(st);
	uint16_t ttf = scom_thdr_ttf(st);
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
	/* index into the sym table */
	p += sprintf(p, "%x", scom_thdr_tblidx(st));
	*p++ = '\t';
	/* tick type */
	p += sprintf(p, "%x", ttf);
	*p++ = '\t';
	switch (ttf) {
		const_sl1t_t l1t;

	case SL1T_TTF_BID:
	case SL1T_TTF_ASK:
	case SL1T_TTF_TRA:
	case SL1T_TTF_FIX:
	case SL1T_TTF_STL:
	case SL1T_TTF_AUC:
		l1t = (const void*)st;
		/* price value */
		p += ffff_m30_s(p, (m30_t)l1t->v[0]);
		*p++ = '\t';
		/* size value */
		p += ffff_m30_s(p, (m30_t)l1t->v[1]);
		break;
	case SL1T_TTF_VOL:
	case SL1T_TTF_VPR:
	case SL1T_TTF_OI:
		/* just one huge value, will there be a m62? */
		l1t = (const void*)st;
		p += ffff_m62_s(p, (m62_t)l1t->w[0]);
		break;

		/* candles */
	case SL1T_TTF_BID | SCOM_FLAG_LM:
	case SL1T_TTF_ASK | SCOM_FLAG_LM:
	case SL1T_TTF_TRA | SCOM_FLAG_LM:
		p += __pr_cdl(p, st);
		break;

		/* snaps */
	case SL1T_TTF_UNK | SCOM_FLAG_LM:
		p += __pr_snap(p, st);
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
