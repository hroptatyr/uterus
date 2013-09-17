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
#if defined HAVE_SYS_TYPES_H
/* for ssize_t */
# include <sys/types.h>
#endif	/* HAVE_SYS_TYPES_H */
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
# define UDEBUG(args...)	fprintf(stderr, args)
#else  /* !DEBUG_FLAG */
# define assert(args...)
# define UDEBUG(args...)
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
	for (p = *cursor; *p != '\t' && *p != PRCHUNK_EOL && *p != '|'; p++) {
		switch (*p) {
		case '0' ... '9':
			res = (res << 4) + (*p - '0');
			break;
		case 'a' ... 'f':
			res = (res << 4) + (*p - 'a' + 10U);
			break;
		case 'A' ... 'F':
			res = (res << 4) + (*p - 'A' + 10U);
			break;
		default:
			res = 0;
			goto ffw;
		}
	}
	if (*p == '|') {
	ffw:
		/* fast-forward to the next cell */
		for (; *p != '\t' && *p != PRCHUNK_EOL; p++);
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

	switch (*p) {
	case '0':
	case '?':
		if (p[1] == '\t') {
			/* fuckall symbol */
			(*cursor)++;
		case '\t':
			(*cursor)++;
			return NULL;
		}
	default:
		break;
	}

	/* otherwise it could be a real symbol, read up to the tab */
	if ((p = strchr(p + 1, '\t')) == NULL) {
		/* no idea what *cursor should point to, NULL maybe? */
		return *cursor = NULL;
	}
	/* copy to tmp buffer, do we need ute_sym2idx() with a len? */
	if ((len = p - *cursor) >= sizeof(symbuf)) {
		len = sizeof(symbuf) - 1;
	}
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
		for (*on = str; **on != '\t' && **on != PRCHUNK_EOL; (*on)++);
		return 0;
	}
	min_off = ffff_strtol(*on, on, 0);
	return hour_off * 3600 + hour_off >= 0 ? min_off * 60 : min_off * -60;
}

static int
parse_rcv_stmp(scom_thdr_t thdr, const char **cursor)
{
	const char *cp = *cursor;
	struct tm tm;
	time_t stamp;
	int msec;
	int zoff;

	ffff_strptime(cp, &tm);
	stamp = ffff_timegm(&tm);
	cp += 10/*YYYY-MM-DD*/ + 1/*T*/ + 8/*HH:MM:SS*/;
	if (UNLIKELY(*cp++ != '.')) {
		return -1;
	}
	/* get the millisecs */
	msec = (int)ffff_strtol(cp, cursor, 0);

	/* get time zone info */
	zoff = parse_zoff(*cursor, cursor);

	/* write the results to thdr */
	scom_thdr_set_sec(thdr, stamp - zoff/*in secs*/);
	scom_thdr_set_msec(thdr, (uint16_t)msec);
	return 0;
}

static unsigned int
parse_cspec(const char **cursor)
{
	const char *cp = *cursor;
	char dig1;
	char dig2;
	unsigned int res = 0U;

	if (UNLIKELY(*cp++ != 'c')) {
		return 0U;
	}

	if (UNLIKELY((dig1 = *cp++) < '0' || dig1 > '9')) {
		/* completely fucked */
		return 0U;
	} else if ((res = dig1 - '0', (dig2 = *cp++) < '0' || dig2 > '9')) {
		cp--;
	} else {
		res = res * 10U + (dig2 - '0');
	}
	switch (*cp++) {
	case 'd':
		res *= 24U;
	case 'h':
		res *= 60U;
	case 'm':
		res *= 60U;
	case 's':
		break;
	default:
		/* fucked */
		return 0U;
	}
	/* beautiful */
	*cursor = cp;
	return res;
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
	/* what's next?
	 * candle specs c??u where ?? is a number and
	 * u is s for seconds, m for minutes, h for hours, d for days */
	for (unsigned int cspec; (cspec = parse_cspec(cursor));) {
		return cspec;
	}

	/* zap to next tab */
	for (; **cursor != '\t' && **cursor != PRCHUNK_EOL; (*cursor)++);
	return 0;
}

static int
__u64_payload_p(uint16_t ttf)
{
	return (ttf & 0x0cU) == 0x08U || (ttf & 0x0fU) == 0x0fU;
}

static int
read_line(mux_ctx_t ctx, struct sndwch_s *tl)
{
	const char *cursor;
	char *line;
	/* symbol and its index */
	const char *sym;
	unsigned int symidx;
	uint16_t ttf;

	/* get the line, its length and set up the cursor */
	if (UNLIKELY(prchunk_getline(ctx->rdr, &line) <= 0)) {
		return -1;
	}
	cursor = line;

	/* symbol comes next, or `nothing' or `C-c' */
	if (UNLIKELY((sym = parse_symbol(&cursor), cursor == NULL))) {
		/* symbol parse error, innit? */
		return -1;
	} else if (UNLIKELY(parse_rcv_stmp(AS_SCOM_THDR(tl), &cursor) < 0)) {
		/* time stamp buggered */
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
	ttf = (uint16_t)hex2int(&cursor);
	if (UNLIKELY(*cursor++ != '\t')) {
		return -1;
	}

	/* bang it all into the target tick */
	scom_thdr_set_tblidx(AS_SCOM_THDR(tl), (uint16_t)symidx);
	scom_thdr_set_ttf(AS_SCOM_THDR(tl), ttf);

	/* now on to the payload */
	if (ttf < SCOM_FLAG_LM && UNLIKELY(__u64_payload_p(ttf))) {
		/* single payload, 1 u6 */
		struct sl1t_s *l1 = (void*)tl;

		l1->w[0] = ffff_m62_get_s(&cursor).u;
		if (*cursor++ != PRCHUNK_EOL) {
			return -1;
		}

	} else if (ttf < SCOM_FLAG_LM) {
		/* single payload, 2 u32s */
		struct sl1t_s *l1 = (void*)tl;

		l1->v[0] = ffff_m30_get_s(&cursor).u;
		if (*cursor++ != '\t') {
			return -1;
		}
		l1->v[1] = ffff_m30_get_s(&cursor).u;
		if (*cursor++ != PRCHUNK_EOL) {
			return -1;
		}

	} else if (ttf < SCOM_FLAG_LM && ttf < SL1T_TTF_BIDASK) {
		/* single payload, 1 u64s */
		struct sl1t_s *l1 = (void*)tl;

		l1->v[0] = ffff_m30_get_s(&cursor).u;
		if (*cursor++ != '\t') {
			return -1;
		}
		l1->v[1] = ffff_m30_get_s(&cursor).u;
		if (*cursor++ != PRCHUNK_EOL) {
			return -1;
		}

	} else if (ttf >= SCOM_FLAG_LM && ttf < SCOM_FLAG_L2M) {
		/* double payload */
		struct sl1t_lm_s *lm = (void*)tl;
		size_t i = 0;

		if (UNLIKELY(__u64_payload_p(ttf))) {
			/* double payload, 1 u64, 2 u32s */
			lm->w[0] = ffff_m62_get_s(&cursor).u;
			if (*cursor++ != '\t') {
				return -1;
			}
			i = 2;
		}
		for (; i < 4; i++) {
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
pr_cspec(char *restrict tgt, unsigned int cspec)
{
	char *restrict p = tgt;
	char suff;

	if (cspec < 60U) {
		suff = 's';
	} else if (cspec < 3600U) {
		cspec /= 60U;
		suff = 'm';
	} else if (cspec < 86400U) {
		cspec /= 3600U;
		suff = 'h';
	} else {
		cspec /= 86400U;
		suff = 'd';
	}

	*p++ = 'c';
	if (LIKELY(cspec)) {
		*p++ = (char)((cspec / 10U) + '0');
		*p++ = (char)((cspec % 10U) + '0');
	} else {
		*p++ = '?';
		*p++ = '?';
	}
	*p++ = suff;
	/* should be 4 always */
	return p - tgt;
}

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
	return p - tgt;
}

static size_t
__pr_cdl(char *tgt, scom_t st)
{
	const_scdl_t cdl = (const void*)st;
	char *p = tgt;

	if (LIKELY(!__u64_payload_p(scom_thdr_ttf(st)))) {
		/* h(igh) */
		p += ffff_m30_s(p, (m30_t)cdl->h);
		*p++ = '\t';
		/* l(ow) */
		p += ffff_m30_s(p, (m30_t)cdl->l);
	} else {
		/* h(igh) */
		p += ffff_m62_s(p, (m62_t)cdl->vol);
	}
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
	if (cdl->sta_ts >= 100000 || scom_thdr_sec(cdl->hdr) < 100000) {
		p += pr_tsmstz(p, cdl->sta_ts, 0, NULL, 'T');
	} else {
		/* might be a candle spec */
		p += pr_cspec(p, cdl->sta_ts);
	}
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

int
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
	case SL1T_TTF_G32:

	case SL1T_TTF_BIDASK:
	case SL2T_TTF_BID:
	case SL2T_TTF_ASK:
		l1t = (const void*)st;
		/* price value */
		p += ffff_m30_s(p, (m30_t)l1t->v[0]);
		*p++ = '\t';
		/* size value */
		p += ffff_m30_s(p, (m30_t)l1t->v[1]);
		break;
	case SL1T_TTF_VOL:
	case SL1T_TTF_VPR:
	case SL1T_TTF_VWP:
	case SL1T_TTF_OI:
	case SL1T_TTF_G64:
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
		/* vol candles */
	case SL1T_TTF_VOL | SCOM_FLAG_LM:
		/* all the distinctions happen in __pr_cdl() */
		p += __pr_cdl(p, st);
		break;

		/* snaps */
	case SSNP_FLAVOUR:
		p += __pr_snap(p, st);
		break;

	case SCOM_TTF_UNK:
	default:
		break;
	}
	*p++ = '\n';
	*p = '\0';

	/* and off we go */
	if (write(pctx->outfd, tl, p - tl) < 0) {
		return -1;
	}
	return 0;
}

/* uta.c ends here */
