/*** gesmes.c -- generic statistical message format
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

/* hope it's ours, the one that defines index_t */
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include "pageio.h"
#include "secu.h"
#include "date.h"
#include "sl1t.h"
#include "sl1tfile.h"
#include "sumux.h"

#include <libxml/parser.h>

#if defined USE_DEBUGGING_ASSERTIONS
# include <assert.h>
#endif	/* USE_DEBUGGING_ASSERTIONS */
#define DEFINE_GORY_STUFF
#include "m30.h"
#include "m62.h"

#define countof(x)	(sizeof(x) / sizeof(*x))

struct cclo_s {
	struct sl1t_s s[1];
	void *wrr;
};


static uint16_t
pump_sym(sl1t_fio_t fio, const char *sym)
{
	return sl1t_fio_find_sym_crea(fio, sym, 3);
}


static void
sta(void *ctx, const xmlChar *name, const xmlChar **attrs)
{
	struct cclo_s *clo = ctx;

	if (memcmp(name, "Cube", 4) != 0 || attrs == NULL) {
		/* sod it */
		return;
	}
	/* check if we're in the outer frame specifying a point in time */
	if (memcmp(attrs[0], "time", 4) == 0) {
		struct tm tm;
		uint32_t d;
		ffff_strptime_Ymd((const char*)attrs[1], &tm);
		d = tm.tm_yday +
			365 * (tm.tm_year - 70) +
			/* number of leap years */
			__nleap_years(tm.tm_year + 1900);
		sl1t_set_stmp_sec(clo->s, d * 86400);

	} else if (memcmp(attrs[0], "curr", 4) == 0 &&
		   memcmp(attrs[2], "rate", 4) == 0) {
		const char *s = (const char*)attrs[1];
		const char *r = (const char*)attrs[3];
		sl1t_set_tblidx(clo->s, pump_sym(clo->wrr, s));
		clo->s->v[0] = ffff_m30_get_s(&r).v;
	}
	return;
}

static void
end(void *ctx, const xmlChar *name)
{
	struct cclo_s *clo = ctx;

	if (memcmp(name, "Cube", 4) != 0) {
		/* sod it */
		return;
	} else if (clo->s->v[0] == 0) {
		return;
	}
	/* kick off */
	sl1t_fio_write_ticks(clo->wrr, (void*)clo->s, 1);
	/* reset */
	clo->s->v[0] = 0;
	return;
}


static xmlSAXHandler gesmes = {
	.startElement = sta,
	.endElement = end,
};

static void
parse_gesmes(mux_ctx_t ctx)
{
	struct cclo_s clo;

	/* prepare the candle, innit */
	memset(clo.s, 0, sizeof(*clo.s));
	sl1t_set_ttf(clo.s, SL1T_TTF_FIX);
	clo.wrr = ctx->wrr;
	/* now read */
	for (const char **inp = ctx->opts->infiles; *inp; inp++) {
		if (xmlSAXUserParseFile(&gesmes, &clo, *inp) < 0) {
			/* we do it all by side-effect, so this is bonkers */
			continue;
		}
	}
	return;
}

void
gesmes_slab(mux_ctx_t ctx)
{
/* been too lazy as to actually take a look at the specs, all I know is
 * there's only end-of-data delivered through gesmes, hence time zone
 * crap is nothing we should worry about. */
	struct sl1t_fio_opts_s opts = {
		.flags = UTEHDR_FLAG_ORDERED | UTEHDR_FLAG_SYMTBL,
		.nsecus = 0 /* MAX */,
	};

	/* init reader and write, we use libxml's sax parser */
	ctx->wrr = make_sl1t_writer(ctx->outfd, opts);
	/* main `loop' */
	parse_gesmes(ctx);
	/* and we're out */
	free_sl1t_writer(ctx->wrr);
	return;
}

/* gesmes.c ends here */
