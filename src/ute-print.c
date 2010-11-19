/*** ute-print.c -- ute tick printer
 *
 * Copyright (C) 2010 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <sebastian.freundt@ga-group.nl>
 *
 * This file is part of sushi/uterus.
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

/** test client for libuterus */
#include <stdio.h>
#include <assert.h>
#include "utefile.h"
#include "sl1t.h"
#include "sumux.h"
#define DEFINE_GORY_STUFF
#include "m30.h"
#include "m62.h"

#include "timegm.c"
#include "strftime.c"

#include "ute-print.h"

static void
print_tick_rTI(utectx_t uctx, scom_t st)
{
	char tl[MAX_LINE_LEN];
	const_sl1t_t t = (const void*)st;
	uint16_t si = sl1t_tblidx(t);
	uint32_t sec = sl1t_stmp_sec(t);
	uint16_t msec = sl1t_stmp_msec(t);
	uint16_t ttf = sl1t_ttf(t);
	char *p;

	if ((p = tl + pr_sym(uctx, tl, si)) == tl) {
		*p++ = '0';
	}
	*p++ = '\t';
	p += pr_ts(p, sec);
	*p++ = '\t';
	p += sprintf(p, "%03hu", msec);
	*p++ = '\t';
	/* sequence is always 0 */
	*p++ = '0';
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
	*p++ = '\t';
	/* tick type */
#pragma warning (disable:2259)
	*p++ = ttf < 10 ? ttf + '0' : ttf - 10 + 'a';
#pragma warning (default:2259)
	*p++ = '\n';
	*p++ = '\0';

	fputs(tl, stdout);
	return;
}


/* ute print FILE*/
static const char ute_cmd_print_help[] =
	"usage: ute print FILE\n"
	"\n"
	"Print the contents of FILE in an rTickItems like fashion.\n";

static void __attribute__((noreturn))
__print_fuck_off_and_die(void)
{
	fputs(ute_cmd_print_help, stdout);
	exit(1);
}

static void
ute_cmd_print_popt(sumux_opt_t opts, int argc, const char *argv[])
{
	/* input file index */
	int ifi = 0;

	/* first of all make a infiles array as large as argv */
	opts->infiles = malloc(argc * sizeof(char*));

	for (int i = 1; i < argc; i++) {
		if (0) {
			;
		} else if (!strcmp(argv[i], "--help") ||
			   !strcmp(argv[i], "-h")) {
			/* --help */
			__print_fuck_off_and_die();
		} else {
			/* must be a file to process */
			opts->infiles[ifi++] = argv[i];
		}
	}
	/* finalise the infile list */
	opts->infiles[ifi] = NULL;
	return;
}

static void
ute_cmd_print_unpopt(sumux_opt_t opts)
{
	/* just that inline thing */
	free(opts->infiles);
	return;
}

static void
ute_cmd_print(sumux_opt_t opts)
{
	for (const char **p = opts->infiles; *p; p++) {
		void *hdl;

		if ((hdl = ute_open(*p, UO_RDONLY)) == NULL) {
			continue;
		}
		/* otherwise print all them ticks */
		for (size_t i = 0; i < ute_nticks(hdl); i++) {
			scom_t ti = ute_seek(hdl, i);
			if (ti) {
				print_tick_rTI(hdl, ti);
			}
		}
		ute_close(hdl);
	}
	return;
}

static int
ute_cmd_print_args(int argc, const char *argv[])
{
	struct sumux_opt_s opts[1] = {{0}};

	/* parse options */
	ute_cmd_print_popt(opts, argc, argv);
	/* now call the actual mux command */
	ute_cmd_print(opts);
	/* clear our resources */
	ute_cmd_print_unpopt(opts);
	return 0;
}

/* ute-print.c ends here */
