/*** fxstreet.c -- parse fxstreet csv'ified json
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
/* fxstreet files are csvified json messages:
 * TIMESTAMP.NANOSECONDS SYMBOL DATETIMETZ BID ASK LAST-TRADE
 * The files are in chronological order, ordered by the received stamp
 * (the first column) */

#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stddef.h>
#include <stdint.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "prchunk.h"
#include "date.h"
#include "utefile.h"
#include "ute-mux.h"
#define DEFINE_GORY_STUFF
#include "m30.h"
#include "m62.h"

/* tick type we need is ssnp solely */
#include "ssnp.h"

#if defined __INTEL_COMPILER
# pragma warning(disable:981)
#endif	/* __INTEL_COMPILER */
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

#if defined DEBUG_FLAG
# include <assert.h>
#else  /* !DEBUG_FLAG */
# define assert(args...)
#endif	/* DEBUG_FLAG */

#define FLAG_INVAL	0x01
#define FLAG_HALTED	0x02

typedef uint8_t symidx_t;
typedef struct fxst_msg_s *fxst_msg_t;

struct fxst_msg_s {
	/* common to all */
	struct time_sns_s rcv_stmp;
	const char *sym;

	/* resulting tick */
	struct ssnp_s snp[1];
};


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


static zif_t z_cet = NULL;

static int
parse_rcv_stmp(fxst_msg_t tgt, const char **cursor)
{
#define DIE_IF_CURSOR0_IS_NOT(c)		\
	if (UNLIKELY(*((*cursor)++) != (c))) {	\
		return -1;			\
	}
	/* the receive time stamp */
	tgt->rcv_stmp.sec = ffff_strtol(*cursor, cursor, 0);
	DIE_IF_CURSOR0_IS_NOT('.');
	tgt->rcv_stmp.nsec = ffff_strtol(*cursor, cursor, 0);
	DIE_IF_CURSOR0_IS_NOT('\t');
#undef DIE_IF_CURSOR0_IS_NOT
	return 0;
}

static inline time_t
parse_ymdhmstz(const char *buf, zif_t z, char sep)
{
	struct tm tm = {0};
	time_t res = -1;
	char *p;

	if ((p = strptime(buf, "%F", &tm)) &&
	    *p++ == sep &&
	    strptime(p, "%T", &tm)) {
		res = ffff_timelocal(&tm, z);
	}
	return res;
}


static bool
parse_line(mux_ctx_t ctx, fxst_msg_t msg)
{
	char *line;
	size_t llen;
	const char *cursor;

	llen = prchunk_getline(ctx->rdr, &line);

	/* we parse the line in 3 steps, receive time stamp, symbol, values */
	cursor = line;
	/* receive time stamp, always first on line */
	if (UNLIKELY(parse_rcv_stmp(msg, &cursor) < 0)) {
		goto bugger;
	}
	/* parse the rest */
	llen -= cursor - line;

	/* go parse the symbol */
	{
		char *eosym;

		if ((eosym = strchr(cursor, '\t')) == NULL) {
			goto bugger;
		}
		/* else finalise it */
		*eosym = '\0';
		/* look 'im up */
		msg->snp->hdr->idx = ute_sym2idx(ctx->wrr, cursor);
		/* ffw the cursor */
		cursor = eosym + 1;
		/* and reset the finaliser */
		*eosym = '\t';
	}
	/* parse the time stamp */
	{
		char *eodt;

		if ((eodt = strchr(cursor, '\t')) == NULL) {
			goto bugger;
		}
		/* else finalise it */
		*eodt = '\0';
		/* parse the stamp (and convert to UTC) */
		msg->snp->hdr->sec = parse_ymdhmstz(cursor, z_cet, ' ');
		msg->snp->hdr->msec = 0;
		/* fwd the cursor */
		cursor = eodt + 1;
		/* and reset the finaliser */
		*eodt = '\t';
	}
	/* now comes: bid \t ask \t tra, make it a snapshot */
	ssnp_set_ttf(msg->snp, SSNP_FLAVOUR);
	{
		m30_t tmp;

		/* bid */
		tmp = ffff_m30_get_s(&cursor);
		msg->snp->bp = tmp.u;
		msg->snp->bq = 0;
		/* move on */
		if (*cursor++ != '\t') {
			goto bugger;
		}

		/* ask */
		tmp = ffff_m30_get_s(&cursor);
		msg->snp->ap = tmp.u;
		msg->snp->aq = 0;
		/* move on */
		if (*cursor++ != '\t') {
			goto bugger;
		}

		/* tra */
		tmp = ffff_m30_get_s(&cursor);
		msg->snp->tvpr = tmp.u;
		msg->snp->tq = 0;
		/* move on */
		if (*cursor++ != '\0') {
			goto bugger;
		}
	}
	return true;

bugger:
	/* declare the line fucked */
	fputs("line b0rked\n> ", stderr);
	fputs(line, stderr);
	fputc('\n', stderr);
	return false;
}

#include "ute-print.h"
static int
write_tick(mux_ctx_t ctx, fxst_msg_t msg)
{
	ute_add_tick(ctx->wrr, AS_SCOM(msg->snp));
	return 0;
}


static void
read_lines(mux_ctx_t ctx)
{
	while (moar_ticks_p(ctx)) {
		struct fxst_msg_s msg[1];

		if (parse_line(ctx, msg)) {
			write_tick(ctx, msg);
		}
	}
	return;
}

void
mux(mux_ctx_t ctx)
{
	static const char cet[] = "/usr/share/zoneinfo/CET";

	/* init reader, we use prchunk here */
	ctx->rdr = init_prchunk(ctx->infd);
	/* intitialise the CET zone */
	z_cet = zif_read_inst(cet);

	/* main loop */
	while (fetch_lines(ctx)) {
		read_lines(ctx);
	}
	/* free prchunk resources */
	free_prchunk(ctx->rdr);
	/* free zone resources */
	zif_close(z_cet);
	return;
}

/* fxstreet.c ends here */
