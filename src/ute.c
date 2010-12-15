/*** ute.c -- command line tool for libuterus
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
#include <stddef.h>
#include <stdio.h>
#include "utefile.h"
#include "sl1t.h"

/* simple all in one tool */

#include "ute-print.c"
#include "ute-mux.c"
#include "ute-shnot.h"

/* for our options parser */
#include "ute-opt.h"
#include "version.h"

static void
ute_cmd_sort(const char *file)
{
	void *hdl = ute_open(file, UO_RDWR);
	ute_sort(hdl);
	ute_close(hdl);
	return;
}

static int
ute_cmd_version(ute_opt_t UNUSED(o), int UNUSED(ac), const char *UNUSED(av[]))
{
	fputs("ute v", stdout);
	fputs(UTE_VERSION, stdout);
	fputc('\n', stdout);
	return 0;
}

static const char usage[] = "\
Usage: ute COMMAND ARGS\n\
\n\
Supported commands:\n\
  help    This help screen\n\
  mux     Generate an ute file from a tick source\n\
  print   Print the contents of an ute file\n\
  shnot   Generate top-level snapshots from ute files\n\
  sort    Test sort the file\n\
\n\
Use 'ute help COMMAND' for more information \
on a specific command\n\
\n\
Options common to all commands:\n\
-q, --quiet       Suppress all output.\n\
-v, --verbose     Be more talkative.\n\
-z, --zone NAME   Treat date/time strings as in time zone NAME.\n\
\n\
-h, --help        Print this help screen.\n\
-V, --version     Print version information.\n\
\n";

static int
ute_cmd_help(ute_opt_t UNUSED(octx), int UNUSED(argc), const char *argv[])
{
	if (argv == NULL || argv[1] == NULL) {
		goto fallthrough;
	} else if (strcmp(argv[1], "mux") == 0) {
		fputs(ute_cmd_mux_help, stdout);
	} else if (strcmp(argv[1], "print") == 0) {
		fputs(ute_cmd_print_help, stdout);
	} else {
	fallthrough:
		fputs(usage, stdout);
	}
	return 0;
}

static int
ute_popt(ute_opt_t octx, int argc, char *argv[])
{
	int i = 0;

	memset(octx, 0, sizeof(*octx));
	if (argc <= 1) {
		return -1;
	}

	/* common options */
again:
	for (i++; i < argc; i++) {
		/* check options */
		if (strcmp(argv[i], "-q") == 0 ||
		    strcmp(argv[i], "--quiet") == 0) {
			argv[i] = NULL;
			octx->verbosity--;

		} else if (strcmp(argv[i], "-v") == 0 ||
			   strcmp(argv[i], "--verbose") == 0) {
			argv[i] = NULL;
			octx->verbosity++;

		} else if (strcmp(argv[i], "-z") == 0 ||
			   strcmp(argv[i], "--zone") == 0) {
			argv[i] = NULL;
			octx->zone = argv[++i];
			argv[i] = NULL;

		} else if (strcmp(argv[i], "-V") == 0 ||
			   strcmp(argv[i], "--version") == 0) {
			octx->cmd = ute_cmd_version;
			break;

		} else if (strcmp(argv[i], "-h") == 0 ||
			   strcmp(argv[i], "--help") == 0) {
			if (octx->cmd == NULL) {
				/* someone just ute --help */
				octx->cmd = ute_cmd_help;
				break;
			}
			/* command already set, must be called like
			 *   ute CMD --help
			 * rewrite that to ute help CMD */
			octx->argv--;
			octx->argc++;
			octx->cmd = ute_cmd_help;

		} else {
			break;
		}
	}

	if (octx->cmd) {
		return 0;
	}

	/* command checking */
	octx->argc = argc - i;
	octx->argv = (const char**)(argv + i);
	if (strcmp(argv[i], "help") == 0) {
		octx->cmd = ute_cmd_help;

	} else if (strcmp(argv[i], "version") == 0) {
		octx->cmd = ute_cmd_version;

	} else if (strcmp(argv[i], "mux") == 0) {
		octx->cmd = ute_cmd_mux_args;

	} else if (strcmp(argv[i], "print") == 0) {
		octx->cmd = ute_cmd_print_args;

	} else if (strcmp(argv[i], "shnot") == 0) {
		octx->cmd = ute_cmd_shnot_args;

	} else if (strcmp(argv[i], "sort") == 0) {
		octx->cmd = ute_cmd_sort;
	}

	goto again;
}

int
main(int argc, char *argv[])
{
	struct ute_opt_s octx[1];

	if (ute_popt(octx, argc, argv) < 0 || octx->cmd == NULL) {
		ute_cmd_help(octx, octx->argc, octx->argv);
		return 1;
	}

	return octx->cmd(octx, octx->argc, octx->argv);
}

/* ute.c ends here */
