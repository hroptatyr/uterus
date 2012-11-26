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
#include "prchunk.h"
#include "utefile.h"
/* for public demux (print) apis */
#include "ute-print.h"
/* for public muxing */
#include "ute-mux.h"
#include "nifty.h"

/* so we know about ticks, candles and snapshots */
#include "sl1t.h"
#include "scdl.h"
#include "ssnp.h"

/* we're just as good as rudi, aren't we? */
#if defined DEBUG_FLAG
# include <assert.h>
#else  /* !DEBUG_FLAG */
# define assert(args...)
#endif	/* DEBUG_FLAG */

#define DEFINE_GORY_STUFF
#include "m30.h"
#include "m62.h"

#define MAX_LINE_LEN	512
#define countof(x)	(sizeof(x) / sizeof(*x))


/* muxing caps */
/* read buffer goodies */
static inline bool
fetch_lines(mux_ctx_t ctx)
{
	return !(prchunk_fill(ctx->rdr) < 0);
}

static inline bool
moar_ticks_p(mux_ctx_t ctx)
{
	return prchunk_haslinep(ctx->rdr);
}

static inline unsigned int
hex2int(const char **cursor)
{
	int res = 0;
	const char *p;
	for (p = *cursor; *p != '\t' && *p != '\n' && *p != '|'; p++) {
		res = (res << 4) + (*p - '0');
	}
	if (*p == '|') {
		/* fast-forward to the next cell */
		while (*p != '\t' && *p != '\n');
	}
	*cursor = p;
	return res;
}

static const char*
parse_symbol(const char **cursor)
{
	const char *p = *cursor;
	static char symbuf[64];
	size_t len;

	if (p[0] == '0' && p[1] == '\t') {
		/* fuckall symbol */
		*cursor += 2;
		return NULL;
	}

	/* otherwise it could be a real symbol, read up to the tab */
	if ((p = strchr(p + 1, '\t')) == NULL) {
		/* no idea what *cursor should point to, NULL maybe? */
		return *cursor = NULL;
	}
	/* copy to tmp buffer, do we need ute_sym2idx() with a len? */
	len = p - *cursor;
	memcpy(symbuf, *cursor, len);
	symbuf[len] = '\0';
	*cursor = p + 1;
	return symbuf;
}

static int
parse_zoff(const char *str, const char **on)
{
	long int hour_off = ffff_strtol(str, on, 0);
	long int min_off;

	if (UNLIKELY(*(*on)++ != ':')) {
		/* just skip to the next tab or newline */
		for (*on = str; **on != '\t' && **on != '\n'; (*on)++);
		return 0;
	}
	min_off = ffff_strtol(*on, on, 0);
	return hour_off * 3600 + hour_off >= 0 ? min_off * 60 : min_off * -60;
}

static int
parse_rcv_stmp(scom_thdr_t thdr, const char **cursor)
{
	struct tm tm;
	time_t stamp;
	long int msec;
	int zoff;

	ffff_strptime(*cursor, &tm);
	stamp = ffff_timegm(&tm);
	*cursor += 10/*YYYY-MM-DD*/ + 1/*T*/ + 8/*HH:MM:SS*/;
	if (UNLIKELY(*(*cursor)++ != '.')) {
		return -1;
	}
	/* get the millisecs */
	msec = ffff_strtol(*cursor, cursor, 0);

	/* get time zone info */
	zoff = parse_zoff(*cursor, cursor);

	/* write the results to thdr */
	scom_thdr_set_sec(thdr, stamp - zoff/*in secs*/);
	scom_thdr_set_msec(thdr, msec);
	return 0;
}

static inline unsigned int
decfld(const char **cursor)
{
/* try date parsing first */
	union scom_thdr_u h[1];

	if (LIKELY(**cursor == '?')) {
		/* ffw to | (if any) */
		while (*(*cursor)++ != '|');
	}
	if (parse_rcv_stmp(h, cursor) == 0) {
		/* cursor should be positioned correctly */
		return scom_thdr_sec(h);
	}
	/* what's next? */

	/* zap to next tab */
	for (; **cursor != '\t' && **cursor != '\n'; (*cursor)++);
	return 0;
}

static int
read_line(mux_ctx_t ctx, struct sndwch_s *tl)
{
	const char *cursor;
	char *line;
	/* symbol and its index */
	const char *sym;
	unsigned int symidx;
	unsigned int ttf;

	/* get the line, its length and set up the cursor */
	if (UNLIKELY(prchunk_getline(ctx->rdr, &line) <= 0)) {
		return -1;
	}
	cursor = line;

	/* symbol comes next, or `nothing' or `C-c' */
	sym = parse_symbol(&cursor);

	/* receive time stamp, always first on line */
	if (UNLIKELY(parse_rcv_stmp(AS_SCOM_THDR(tl), &cursor) < 0)) {
		return -1;
	} else if (UNLIKELY(*cursor++ != '\t')) {
		return -1;
	}

	/* next up is the sym-idx in hex */
	if (LIKELY(*cursor != '?')) {
		symidx = hex2int(&cursor);
		if (UNLIKELY(*cursor++ != '\t')) {
			return -1;
		}
		/* bang the symbol */
		if (ute_bang_symidx(ctx->wrr, sym, symidx) != symidx) {
			/* oh bugger */
			return -1;
		}
	} else {
		/* obtain the actual symidx value from ute_sym2idx() */
		cursor++;
		if (UNLIKELY(*cursor++ != '\t')) {
			return -1;
		}
		/* add the symbol */
		if ((symidx = ute_sym2idx(ctx->wrr, sym)) == 0) {
			return -1;
		}
	}

	/* check the tick type + flags, it's hex already */
	ttf = hex2int(&cursor);
	if (UNLIKELY(*cursor++ != '\t')) {
		return -1;
	}

	/* bang it all into the target tick */
	scom_thdr_set_tblidx(AS_SCOM_THDR(tl), (uint16_t)symidx);
	scom_thdr_set_ttf(AS_SCOM_THDR(tl), (uint16_t)ttf);

	/* now on to the payload */
	if (ttf < SCOM_FLAG_LM) {
		/* single payload */
		struct sl1t_s *l1 = (void*)tl;

		l1->v[0] = ffff_m30_get_s(&cursor).u;
		if (*cursor++ != '\t') {
			return -1;
		}
		l1->v[1] = ffff_m30_get_s(&cursor).u;
		if (*cursor++ != '\n') {
			return -1;
		}

	} else if (ttf >= SCOM_FLAG_LM && ttf < SCOM_FLAG_L2M) {
		/* double payload, at least 4 m30s(?) */
		struct sl1t_lm_s *lm = (void*)tl;

		for (size_t i = 0; i < 4; i++) {
			lm->v[i] = ffff_m30_get_s(&cursor).u;
			if (*cursor++ != '\t') {
				return -1;
			}
		}
		/* now come 2 generic fields, just read the hex portion */
		if (LIKELY(*cursor != '?')) {
			lm->v[4] = hex2int(&cursor);
		} else {
			/* could be a date */
			lm->v[4] = decfld(&cursor);
		}
		if (LIKELY(*cursor != '?')) {
			lm->v[5] = hex2int(&cursor);
		} else {
			/* uh oh, don't know what to do */
			;
		}
	}
	return 0;
}

static void
read_lines(mux_ctx_t ctx)
{
	while (moar_ticks_p(ctx)) {
		struct sndwch_s buf[4];
		if (read_line(ctx, buf) == 0) {
			ute_add_tick(ctx->wrr, AS_SCOM(buf));
		}
	}
	return;
}


/* printing caps */
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
	const_ssnp_t snp = (const void*)st;
	char *p = tgt;

	/* bid price */
	p += ffff_m30_s(p, (m30_t)snp->bp);
	*p++ = '\t';
	/* ask price */
	p += ffff_m30_s(p, (m30_t)snp->ap);
	if (scom_thdr_ttf(st) == SSNP_FLAVOUR) {
		/* real snaps reach out further */
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
	}
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
void
mux(mux_ctx_t ctx)
{
	/* main loop */
	ctx->rdr = init_prchunk(ctx->infd);
	while (fetch_lines(ctx)) {
		read_lines(ctx);
	}
	/* free prchunk resources */
	free_prchunk(ctx->rdr);
	return;
}

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
	case SL1T_TTF_FIX | SCOM_FLAG_LM:
	case SL1T_TTF_STL | SCOM_FLAG_LM:
	case SL1T_TTF_AUC | SCOM_FLAG_LM:
		p += __pr_cdl(p, st);
		break;

		/* snaps */
	case SSNP_FLAVOUR:
	case SBAP_FLAVOUR:
		p += __pr_snap(p, st);
		break;

	case SCOM_TTF_UNK:
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
