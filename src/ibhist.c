/*** ibhist.c -- twstools' ibhist data parser
 *
 * See https://github.com/rudimeier/twstools
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

#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>

/* we use them expat parser thing */
#include <expat.h>

#include "date.h"
#include "utefile.h"
#include "ute-mux.h"

/* chunk reader of choice */
#define STATIC_EB	1
#include "expobuf.h"

#undef DEFINE_GORY_STUFF
#include "m30.h"
#include "m62.h"

/* so we know about ticks, candles and snapshots */
#include "sl1t.h"
#include "scdl.h"

/* we're just as good as rudi, aren't we? */
#if defined DEBUG_FLAG
# include <assert.h>
#else  /* !DEBUG_FLAG */
# define assert(args...)
#endif	/* DEBUG_FLAG */

#if defined __INTEL_COMPILER
# pragma warning (disable:981)
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

struct qry_s {
	int bar_len;
	zif_t z;
};

struct rsp_s {
	struct scdl_s cdl;
	
};

struct xp_clo_s {
	XML_Parser xp;
	utectx_t wrr;
};

/* bummer, not the wisest global var name, fix me! */
static zif_t z = NULL;

/* tilman tells me this can either be berlin time or UTC
 * depending on the @formatDate attr */
static const char ibhist_zone[] = "Europe/Berlin";


/* track the current level in the xml tree */
#if defined DEBUG_FLAG
static size_t depth = 0;
#endif	/* DEBUG_FLAG */
static struct qry_s cur_qry = {0};
static struct rsp_s cur_rsp = {0};

static void
inspect_qry(utectx_t UNUSED(hdl), const char **attr)
{
	for (size_t i = 0; attr[i]; i += 2) {
		if (strcmp(attr[i], "whatToShow") == 0) {
			uint16_t ttf = SCDL_FLAVOUR;
			switch (attr[i + 1][0]) {
			case 'A':
				ttf |= SL1T_TTF_ASK;
				break;
			case 'B':
				ttf |= SL1T_TTF_BID;
				break;
			case 'T':
				ttf |= SL1T_TTF_TRA;
				break;
			default:
				ttf = SL1T_TTF_UNK;
				break;
			}
			/* write back to forming space */
			scdl_set_ttf(&cur_rsp.cdl, ttf);

		} else if (strcmp(attr[i], "formatDate") == 0) {
			switch (attr[i + 1][0]) {
			case '1':
				/* berlin time */
				cur_qry.z = z;
				break;
			case '2':
				/* UTC? */
			default:
				cur_qry.z = NULL;
				break;
			}
		} else if (strcmp(attr[i], "barSizeSetting") == 0) {
			/* quick manual */
			char *on;
			long int n = strtol(attr[i + 1], &on, 10);
			if (on == NULL || *on != ' ') {
				goto unknown;
			}
			/* otherwise check the unit */
			switch (*++on) {
			case 'm':
				/* should be minute */
				cur_qry.bar_len = n * 60;
				break;
			case 's':
				/* should be second */
				cur_qry.bar_len = n;
				break;
			case 'h':
				/* should be hour */
				cur_qry.bar_len = n * 60 * 60;
				break;
			case 'd':
				/* days? */
				cur_qry.bar_len = n * 60 * 60 * 24;
				break;
			default:
			unknown:
				cur_qry.bar_len = 0;
				break;
			}
		}
		/* all other attrs are not of interest to us */
	}
	return;
}

static void
inspect_con(utectx_t hdl, const char **attr)
{
	const char *sym = NULL;
	const char *sty = NULL;
	const char *xch = NULL;
	const char *exp = NULL;
	const char *ccy = NULL;
	const char *mul = NULL;

	for (size_t i = 0; attr[i]; i += 2) {
		if (strcmp(attr[i], "symbol") == 0) {
			sym = attr[i + 1];
		} else if (strcmp(attr[i], "secType") == 0) {
			sty = attr[i + 1];
		} else if (strcmp(attr[i], "expiry") == 0) {
			exp = attr[i + 1];
		} else if (strcmp(attr[i], "multiplier") == 0) {
			mul = attr[i + 1];
		} else if (strcmp(attr[i], "exchange") == 0) {
			xch = attr[i + 1];
		} else if (strcmp(attr[i], "currency") == 0) {
			ccy = attr[i + 1];
		}
		/* ignore everything else */
	}

	/* free what's there */
	{
		char tmp[32];
		char *p = tmp;
		uint16_t idx;

		p = stpcpy(p, sym);
		*p++ = ' ';
		p = stpcpy(p, sty);
		*p++ = ' ';
		p = stpcpy(p, xch);
		*p++ = ' ';
		p = stpcpy(p, exp);
		*p++ = ' ';
		p = stpcpy(p, ccy);
		*p++ = ' ';
		p = stpcpy(p, mul);
		*p = '\0';
		/* just to speed up computing later on */
		if ((idx = ute_sym2idx(hdl, tmp)) || idx == 0) {
			scdl_set_tblidx(&cur_rsp.cdl, idx);
		}
	}
	return;
}

static void
inspect_row(utectx_t UNUSED(hdl), const char **attr)
{
/* fill is on descent, write is on ascent */
	for (size_t i = 0; attr[i]; i += 2) {
		const char *p = attr[i + 1];
		if (strcmp(attr[i], "date") == 0) {
			struct tm tm;
			time_t sta, end;
			strptime(p, "%Y%m%d  %H:%M:%S", &tm);
			sta = ffff_timelocal(&tm, cur_qry.z);
			end = sta + cur_qry.bar_len;
			/* write back to result forming area */
			cur_rsp.cdl.sta_ts = sta;
			scdl_set_stmp_sec(&cur_rsp.cdl, end);
			scdl_set_stmp_msec(&cur_rsp.cdl, 0);
		} else if (strcmp(attr[i], "open") == 0) {
			cur_rsp.cdl.o = ffff_m30_get_s(&p).v;
		} else if (strcmp(attr[i], "high") == 0) {
			cur_rsp.cdl.h = ffff_m30_get_s(&p).v;
		} else if (strcmp(attr[i], "low") == 0) {
			cur_rsp.cdl.l = ffff_m30_get_s(&p).v;
		} else if (strcmp(attr[i], "close") == 0) {
			cur_rsp.cdl.c = ffff_m30_get_s(&p).v;
		}
	}
	/* no volume nor count */
	cur_rsp.cdl.cnt = 0;
	return;
}

static void
el_sta(void *clo, const char *elem, const char **attr)
{
	struct xp_clo_s *xp_clo = clo;

	if (strcmp(elem, "query") == 0) {
		inspect_qry(xp_clo->wrr, attr);
	} else if (strcmp(elem, "reqContract") == 0) {
		inspect_con(xp_clo->wrr, attr);
	} else if (strcmp(elem, "row") == 0) {
		inspect_row(xp_clo->wrr, attr);
	}
#if defined DEBUG_FLAG
	depth++;
#endif	/* DEBUG_FLAG */
	return;
}

static void
el_end(void *clo, const char *elem)
{
	struct xp_clo_s *xp_clo = clo;

#if defined DEBUG_FLAG
	depth--;
#endif	/* DEBUG_FLAG */
	if (strcmp(elem, "response") == 0) {
		/* make sure we free up qry resources */
		;
	} else if (strcmp(elem, "row") == 0) {
		/* write the candle */
		if (cur_rsp.cdl.sta_ts > 0) {
			ute_add_tick(xp_clo->wrr, AS_SCOM(&cur_rsp.cdl));
			memset(&cur_rsp.cdl, 0, sizeof(cur_rsp.cdl));
		}
	} else if (strcmp(elem, "TWSXML") == 0) {
		assert(depth == 0);
		/* need to reset the parser */
		XML_StopParser(xp_clo->xp, XML_TRUE);
	}
	return;
}


static void
parse(mux_ctx_t ctx)
{
/* bit of a co-routine, yields lines from reader and yields parsed xml */
	XML_Parser hdl = XML_ParserCreate(NULL);
	expobuf_t eb = ctx->rdr;
	struct xp_clo_s clo = {
		.xp = hdl,
		.wrr = ctx->wrr,
	};

reset:
	XML_SetElementHandler(hdl, el_sta, el_end);
	XML_SetUserData(hdl, &clo);

	for (size_t carry = 0; eb_fetch_lines(eb); eb_unfetch_lines(eb)) {
		enum XML_Status res;
		const char *data = eb_current_line(eb);
		size_t dtsz = eb_rest_len(eb);

		res = XML_Parse(hdl, data, dtsz, XML_FALSE);
		if (UNLIKELY(res == XML_STATUS_ERROR)) {
			const enum XML_Error errnum = XML_GetErrorCode(hdl);
			const char *errstr = XML_ErrorString(errnum);

			/* best not to print stuff here? */
			write(ctx->badfd, errstr, strlen(errstr));
			write(ctx->badfd, "\n", 1);
		}
		/* set eb's index */
		{
			size_t inc = XML_GetCurrentByteIndex(hdl) - carry;
			size_t new_carry = eb_rest_len(eb);
			/* advance a bit, just for the inspection below */
			eb_set_current_line_by_offs(eb, inc);
			/* set carry */
			carry = new_carry;
		}

		/* check if there's more */
		if (eb_current_line(eb)[0] == '\f' &&
		    eb_rest_len(eb) > 1) {
			/* advance once more (read over the \f */
			eb_set_current_line_by_offs(eb, 1);
			/* reset carry */
			carry = 0;
			/* resume the parser */
			XML_ParserReset(hdl, NULL);
			/* off we go */
			goto reset;
		} else if (eb_current_line(eb)[0] == '\f') {
			/* ah, the last ^L */
			eb_unfetch_lines(eb);
			break;
		}
		/* otherwise leave everything as is and advance the expobuf */
		eb_consume_lines(eb);
	}

	XML_ParserFree(hdl);
	return;
}


/* the public entry point */
void
mux(mux_ctx_t ctx)
{
	/* open our timezone definition */
	if (ctx->opts->zone != NULL) {
		z = zif_read_inst(ctx->opts->zone);
	} else {
		z = zif_read_inst(ibhist_zone);
	}

	/* init reader, we use prchunk here */
	ctx->rdr = make_expobuf(ctx->infd);

	/* main loop */
	parse(ctx);

	/* free prchunk resources */
	free_expobuf(ctx->rdr);
	if (z != NULL) {
		zif_free(z);
	}
	return;
}

/* ibhist.c ends here */
