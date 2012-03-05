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

#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif	/* !UNLIKELY */
#if !defined UNUSED
# define UNUSED(_x)	__attribute__((unused)) _x
#endif	/* !UNUSED */

/* track the current level in the xml tree */
static size_t depth = 0;

/* bummer, not the wisest global var name, fix me! */
static zif_t z = NULL;

/* tilman tells me this can either be berlin time or UTC
 * depending on the @formatDate attr */
static const char ibhist_zone[] = "Europe/Berlin";


static void
el_sta(void *UNUSED(data), const char *elem, const char **attr)
{
	for (size_t i = 0; i < depth; i++) {
		fprintf(stderr, " ");
	}
	fprintf(stderr, "%s", elem);

	for (size_t i = 0; attr[i]; i += 2) {
		fprintf(stderr, " %s= '%s'", attr[i], attr[i + 1]);
	}
	fprintf(stderr, "\n");
	depth++;
	return;
}

static void
el_end(void *clo, const char *UNUSED(elem))
{
	XML_Parser hdl = clo;

	depth--;
	if (depth == 0) {
		/* need to reset the parser */
		XML_StopParser(hdl, XML_TRUE);
	}

static void
data(void *UNUSED(data), const char *UNUSED(buf), int UNUSED(bsz))
{
	/* twstools is data-less */
	return;
}


static void
parse(mux_ctx_t ctx)
{
/* bit of a co-routine, yields lines from reader and yields parsed xml */
	XML_Parser hdl = XML_ParserCreate(NULL);
	expobuf_t eb = ctx->rdr;

reset:
	XML_SetElementHandler(hdl, el_sta, el_end);
	XML_SetUserData(hdl, hdl);

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
