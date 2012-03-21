/*** forexfactory.c -- parse forexfactory irc raw logs
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
/* forexfactory files are mainly json messages on irc
 * TIMESTAMP.NANOSECONDS SENDER PRIVMSG CHANNEL :JSON
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
typedef struct ff_msg_s *ff_msg_t;

typedef enum {
	FF_MSG_UNK,
	FF_MSG_JOIN,
	FF_MSG_TIME,
	FF_MSG_JSON,
} ff_msg_type_t;

struct ff_msg_s {
	ff_msg_type_t ty;

	/* common to all */
	struct time_sns_s rcv_stmp;
	const char *chan;

	union {
		/* time messages */
		const char *time;
		/* json messages */
		const char *json;
	};
	size_t msglen;

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


/* date/time magic */
static struct time_sns_s
diff_su(uint32_t s, uint16_t ms, struct time_sns_s ts)
{
	struct time_sns_s res;
	if (ts.nsec < ms * 1000000UL) {
		res.sec = ts.sec - s - 1;
		res.nsec = 1000000000 + ts.nsec - ms * 1000000;
	} else {
		res.sec = ts.sec - s;
		res.nsec = ts.nsec - ms * 1000000;
	}
	return res;
}

static inline int32_t
diff_su_ms(uint32_t s, uint16_t ms, struct time_sns_s ts)
{
/* return the difference in ms */
	struct time_sns_s d = diff_su(s, ms, ts);
	int32_t o = d.sec * 1000 + d.nsec / 1000000;
	return o;
}

static inline int32_t
diff_scom_ms(scom_t t, struct time_sns_s ts)
{
	uint32_t s = scom_thdr_sec(t);
	uint16_t ms = scom_thdr_msec(t);
	return diff_su_ms(s, ms, ts);
}


static int
parse_rcv_stmp(ff_msg_t tgt, const char **cursor)
{
#define DIE_IF_CURSOR0_IS_NOT(c)		\
	if (UNLIKELY(*((*cursor)++) != (c))) {	\
		return -1;			\
	}
	/* the receive time stamp */
	tgt->rcv_stmp.sec = ffff_strtol(*cursor, cursor, 0);
	DIE_IF_CURSOR0_IS_NOT('.');
	tgt->rcv_stmp.nsec = ffff_strtol(*cursor, cursor, 0);
	DIE_IF_CURSOR0_IS_NOT(' ');
#undef DIE_IF_CURSOR0_IS_NOT
	return 0;
}

static size_t
json_find(const char **tgt, const char *json, const char *key, size_t ksz)
{
	char *p;
	char *q;

	if ((p = strstr(json, key)) == NULL) {
		;
	} else if ((p = strchr(p + ksz, ':')) == NULL) {
		;
	} else if ((q = strchr(p + 1, ',')) == NULL) {
		;
	} else {
		/* all the magic now */
		*tgt = p + 1;
		return q - p - 1;
	}
	return 0;
}


static bool
read_line(mux_ctx_t ctx, ff_msg_t msg)
{
	static const char cmd_pmsg[] = "PRIVMSG";
	static const size_t cmd_pmsg_sz = sizeof(cmd_pmsg) - 1;
	char *line;
	size_t llen;
	const char *cursor;
	char *p;

	llen = prchunk_getline(ctx->rdr, &line);

	/* we parse the line in 3 steps, receive time stamp, symbol, values */
	cursor = line;
	msg->chan = NULL;
	msg->json = NULL;
	msg->msglen = 0UL;
	/* receive time stamp, always first on line */
	if (UNLIKELY(parse_rcv_stmp(msg, &cursor) < 0)) {
		goto bugger;
	}
	/* parse the rest */
	llen -= cursor - line;

	/* message types */
	if ((p = memmem(cursor, llen, cmd_pmsg, cmd_pmsg_sz))) {
		msg->ty = FF_MSG_JSON;
		msg->chan = p + cmd_pmsg_sz + 1;

		if ((p = strchr(msg->chan, ' '))) {
			*p++ = '\0';

			if (*p++ == ':') {
				/* ah a normal channel message */
				llen -= p - cursor;
				msg->json = p;
				msg->msglen = llen;
				p[llen] = '\0';
				return true;
			}
		}
	}
	return false;
bugger:
	/* declare the line fucked */
	fputs("line b0rked\n> ", stderr);
	fputs(line, stderr);
	fputc('\n', stderr);
	return false;
}

static int
parse_line(mux_ctx_t ctx, ff_msg_t msg)
{
#define SLEN(x)		(sizeof(x) - 1)
	static const char pltf[] = "\"platform\"";
	static const char bid[] = "\"bid\"";
	static const char ask[] = "\"ask\"";
	const char *json = msg->json;
	const char *p;
	size_t psz;

	assert(msg->chan);
	assert(msg->json);

	if (UNLIKELY(msg->json[0] != '{')) {
		/* don't know what this is, could be #time */
		goto bugger;
	}

	/* assign the time stamp */
	{
		register unsigned int s = msg->rcv_stmp.sec;
		register unsigned int ms = msg->rcv_stmp.nsec / 1000000U;
		ssnp_set_stmp_sec(msg->snp, s);
		ssnp_set_stmp_msec(msg->snp, (uint16_t)ms);
	}

	/* construct the slut name */
	if (LIKELY((psz = json_find(&p, json, pltf, SLEN(pltf))) > 0)) {
		const char *q = p + psz;
		char sym[SLUT_SYMLEN];
		uint16_t idx;

		if (*p++ != '"' || *--q != '"') {
			goto bugger;
		}
		/* otherwise it's all good */
		memcpy(sym, msg->chan + 2, 6);
		sym[6] = '.';
		if ((size_t)(q - p) > sizeof(sym) - 8) {
			goto bugger;
		}
		memcpy(sym + 7, p, (q - p));
		sym[7 + (q - p)] = '\0';

		/* little can go wrong from here, so just find the idx */
		idx = ute_sym2idx(ctx->wrr, sym);
		ssnp_set_tblidx(msg->snp, idx);
		ssnp_set_ttf(msg->snp, 0);
	} else {
		goto bugger;
	}

	/* care about the payload now */
	msg->snp->bp = msg->snp->ap = 0;
	/* bid */
	if (LIKELY((psz = json_find(&p, json, bid, SLEN(bid))) > 0)) {
		m30_t b = ffff_m30_get_s(&p);
		msg->snp->bp = b.u;
	} else  {
		goto bugger;
	}

	/* ask */
	if (LIKELY((psz = json_find(&p, json, ask, SLEN(ask))) > 0)) {
		m30_t a = ffff_m30_get_s(&p);
		msg->snp->ap = a.u;
	} else  {
		goto bugger;
	}
	return 0;
bugger:
	fputs("json b0rked> ", stderr);
	fputs(msg->json, stderr);
	fputc('\n', stderr);
	return -1;
}

#include "ute-print.h"
static int
write_tick(mux_ctx_t ctx, ff_msg_t msg)
{
	ute_add_tick(ctx->wrr, AS_SCOM(msg->snp));
	return 0;
}


static void
read_lines(mux_ctx_t ctx)
{
	while (moar_ticks_p(ctx)) {
		struct ff_msg_s msg[1];

		if (read_line(ctx, msg) && parse_line(ctx, msg) >= 0) {
			write_tick(ctx, msg);
		}
	}
	return;
}

void
mux(mux_ctx_t ctx)
{
	/* init reader, we use prchunk here */
	ctx->rdr = init_prchunk(ctx->infd);
	/* main loop */
	while (fetch_lines(ctx)) {
		read_lines(ctx);
	}
	/* free prchunk resources */
	free_prchunk(ctx->rdr);
	return;
}

/* forexfactory.c ends here */
