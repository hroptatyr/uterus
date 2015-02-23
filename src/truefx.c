/*** truefx.c -- truefx log parser
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
/* truefx files are simple
 * SYM,DATE TIME.MILLIS,BID,ASK
 * The files are in chronological order. */

/* hope it's ours, the one that defines index_t */
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "prchunk.h"
#include "date.h"
#include "utefile.h"
#include "sl1t.h"
#include "ute-mux.h"
#include "nifty.h"
#define DEFINE_GORY_STUFF
#include "m30.h"
#include "m62.h"

#if defined __INTEL_COMPILER
# pragma warning(disable:981)
#endif	/* __INTEL_COMPILER */

#define MAX_LINE_LEN	512

typedef uint8_t symidx_t;
typedef struct truefx_tl_s *restrict truefx_tl_t;

struct truefx_tl_s {
	char sym[8U];
	unsigned int ts;
	unsigned int ms;
	m30_t b;
	m30_t a;
};

static m30_t bid;
static m30_t ask;


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


/* dt goodies */
static const int tenners[10] = {0, 10, 20, 30, 40, 50, 60, 70, 80, 90};

static int
get_year(const char *buf)
{
/* we have to do year - 1900, so we just use the third and fourth char
 * and add 100 if buf[1] is '0' */
	return (buf[1] == '0' ? 100 : 0) +
		tenners[(buf[2] - '0')] + (int)(buf[3] - '0');
}

static int
get_month(const char *buf)
{
	return (buf[0] == '0' ? 0 : 10) + (int)(buf[1] - '0');
}

static int
get_day(const char *buf)
{
	return tenners[(buf[0] - '0')] + (int)(buf[1] - '0');
}

static int
get_hour(const char *buf)
{
	return tenners[(buf[0] - '0')] + (int)(buf[1] - '0');
}

static int
get_minute(const char *buf)
{
	return tenners[(buf[0] - '0')] + (int)(buf[1] - '0');
}

static int
get_second(const char *buf)
{
	return tenners[(buf[0] - '0')] + (int)(buf[1] - '0');
}

static int
yday(int y, int m, int d)
{
	static const unsigned short int __mon_yday[13] = {
		0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365
	};
	return d + __mon_yday[m] + (UNLIKELY(!(y % 4U)) && m >= 2);
}

static void
ffff_strpdt_tru(const char buf[static 17], struct tm *restrict tm)
{
/* like strptime() with "%Y%m%d %H:%M:%S" as format */
	tm->tm_year = get_year(buf + 0U);
	tm->tm_mon = get_month(buf + 4U) - 1;
	tm->tm_mday = get_day(buf + 6U);

	tm->tm_hour = get_hour(buf + 9U);
	tm->tm_min = get_minute(buf + 12U);
	tm->tm_sec = get_second(buf + 15U);

	/* need set this as timegm makes use of it */
	tm->tm_yday = yday(tm->tm_year, tm->tm_mon, tm->tm_mday) - 1;
	return;
}


static inline time_t
parse_time(const char **buf, const char *eobuf)
{
	const size_t step = 8U/*YYYYMMDD*/ + 1U/*' '*/  + 8U/*HH:MM:SS*/;
	struct tm tm = {
		.tm_yday = 0
	};

	/* check if the buffer is large enough */
	if (UNLIKELY(*buf + step > eobuf)) {
		*buf = eobuf;
		return 0;
	}
	/* use our sped-up version */
	ffff_strpdt_tru(*buf, &tm);
	*buf += step;
	return ffff_timelocal(&tm, NULL);
}

static inline unsigned int
parse_msec(const char **buf, const char *eobuf)
{
	const size_t step = 1U/*.*/ + 3U/*mmm*/;
	unsigned int res;
	const char *bp = *buf;

	/* check if the buffer is large enough */
	if (UNLIKELY(bp + step > eobuf)) {
		*buf = eobuf;
		return 0;
	}
	/* quickly read it off the wire directly */
	res = ((bp[1] - '0') * 10 + (bp[2] - '0')) * 10 + (bp[3] - '0');

	*buf += step;
	return res;
}

static void
pr_tl(mux_ctx_t ctx, truefx_tl_t UNUSED(t), const char *cursor, size_t len)
{
	static const char prefix[] = "invalid line: ";
	int fd = ctx->badfd;

	write(fd, prefix, sizeof(prefix) - 1); 
	write(fd, cursor, len);
	write(fd, "\n", 1);
	return;
}


static bool
read_line(mux_ctx_t ctx, truefx_tl_t tl)
{
	char *line;
	size_t llen;
	const char *cursor;
	const char *ep;

	llen = prchunk_getline(ctx->rdr, &line);
	ep = line + llen;

	/* skip straight to the timestamp */
	if (UNLIKELY((cursor = strchr(line, ',')) == NULL)) {
		goto bugger;
	}
	memcpy(tl->sym, line, cursor - line);
	tl->sym[7U] = '\0';

	/* get the time stamp parsed */
	if (UNLIKELY(!(cursor++, tl->ts = parse_time(&cursor, ep)))) {
		goto bugger;
	}
	tl->ms = parse_msec(&cursor, ep);
	if (UNLIKELY(*cursor++ != ',')) {
		goto bugger;
	}
	/* read the bid */
	tl->b = ffff_m30_get_s(&cursor);
	if (UNLIKELY(*cursor++ != ',')) {
		goto bugger;
	}
	tl->a = ffff_m30_get_s(&cursor);
	return true;
bugger:
	/* we should make this optional */
	pr_tl(ctx, tl, line, llen);
	return false;
}

static void
write_tick(mux_ctx_t ctx, truefx_tl_t tl)
{
/* create one or more sparse ticks, sl1t_t objects */
	static struct sl1t_s t[2];
	sl1t_t tp = t;
	uint16_t idx;

	if (UNLIKELY(!(idx = (uint16_t)ute_sym2idx(ctx->wrr, tl->sym)))) {
		return;
	}
	if (tl->b.u != bid.u) {
		sl1t_set_stmp_sec(tp, tl->ts);
		sl1t_set_stmp_msec(tp, (uint16_t)tl->ms);
		sl1t_set_ttf(tp, SL1T_TTF_BID);
		sl1t_set_tblidx(tp, idx);
		tp->bid = (bid = tl->b).u;
		tp->bsz = 0;
		tp++;
	}

	if (tl->a.u != ask.u) {
		sl1t_set_stmp_sec(tp, tl->ts);
		sl1t_set_stmp_msec(tp, (uint16_t)tl->ms);
		sl1t_set_ttf(tp, SL1T_TTF_ASK);
		sl1t_set_tblidx(tp, idx);
		tp->ask = (ask = tl->a).u;
		tp->asz = 0;
		tp++;
	}

	for (int i = 0; i < (tp - t); i++) {
		ute_add_tick(ctx->wrr, AS_SCOM(t + i));
	}
	return;
}

static void
read_lines(mux_ctx_t ctx)
{
	while (moar_ticks_p(ctx)) {
		struct truefx_tl_s atl[1];
		memset(atl, 0, sizeof(*atl));
		if (read_line(ctx, atl)) {
			write_tick(ctx, atl);
		}
	}
	return;
}

void
mux(mux_ctx_t ctx)
{
	/* init reader, we use prchunk here */
	ctx->rdr = init_prchunk(ctx->infd);
	while (fetch_lines(ctx)) {
		read_lines(ctx);
	}
	/* free prchunk resources */
	free_prchunk(ctx->rdr);
	return;
}

/* truefx.c ends here */
