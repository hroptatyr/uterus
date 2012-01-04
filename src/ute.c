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
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include "utefile.h"
#include "sl1t.h"

/* for our options parser */
#include "version.h"


static int
ute_cmd_sort(const char *file)
{
	void *hdl = ute_open(file, UO_RDWR);
	ute_sort(hdl);
	ute_close(hdl);
	return 0;
}


/* we expect UTEDIR to be defined at this point */
#if !defined UTEDIR
# error define UTEDIR
#endif	/* UTEDIR */

static char*
__find_cmd(int argc, char *argv[])
{
	int dash_dash_seen_p = 0;

	for (int i = 1; i < argc; i++) {
		if (argv[i][0] != '-' || dash_dash_seen_p) {
			char *res = argv[i];
			argv[i] = NULL;
			return res;
		} else if (argv[i][1] == '-' && argv[i][2] == '\0') {
			dash_dash_seen_p = 1;
		}
	}
	return NULL;
}

static bool
__exep(const char *file)
{
	struct stat st;
	return (stat(file, &st) == 0) && (st.st_mode & S_IXUSR);
}

static char*
build_cmd(const char *cmd)
{
	const char myself[] = "/proc/self/exe";
	const char prefix[] = "ute-";
	char wd[PATH_MAX];
	char *dp;
	size_t sz;

	sz = readlink(myself, wd, sizeof(wd));
	wd[sz] = '\0';

	if ((dp = strrchr(wd, '/')) == NULL) {
		return NULL;
	}
	/* search the path where the binary resides */
	strncpy(dp + 1, prefix, sizeof(prefix) - 1);
	strcpy(dp + sizeof(prefix), cmd);
	if (__exep(wd)) {
		/* found one ... */
		goto succ;
	}
	/* otherwise try UTEDIR */
	if ((dp = stpcpy(wd, UTEDIR))[-1] != '/') {
		*dp++ = '/';
	}
	strncpy(dp, prefix, sizeof(prefix) - 1);
	strcpy(dp + sizeof(prefix) - 1, cmd);
	if (__exep(wd)) {
		/* found one ... */
		goto succ;
	}
	return NULL;

succ:
	return strdup(wd);
}


static void
__pr_help(void)
{
/* this would be the umpp way to do things */
#define SUBCMD(x, line)					\
	do {						\
		printf("  % -20s " line "\n", #x);	\
	} while (0)

	puts("ute " PACKAGE_VERSION "\n\
\n\
Usage: ute [OPTION]... COMMAND ARGS...\n\
\n\
Create, access and modify ute files.\n\
\n\
Common options:\n\
  -h, --help           Print help and exit\n\
  -V, --version        Print version and exit\n\
");

	puts("Supported commands:");

	SUBCMD(help, "Print a help screen like this");
	SUBCMD(mux, "Generate an ute file from a tick or candle source");
	SUBCMD(print, "Print the contents of an ute file");
	SUBCMD(shnot, "Generate all-level snapshots from ute files");
	//SUBCMD(sort, "Sort ute file chronologically");
	puts("");
	return;
}

int
main(int argc, char *argv[])
{
	char *cmd_f = NULL;
	const char *cmd;
	int res = 0;

	if ((cmd = __find_cmd(argc, argv)) == NULL) {
		__pr_help();
		if (argv[1] &&
		    (strcmp(argv[1], "-h") == 0 ||
		     strcmp(argv[1], "--help") == 0)) {
			res = 0;
		} else {
			res = 1;
		}

	} else if ((cmd_f = build_cmd(cmd)) == NULL) {
		fprintf(stderr, "ute subcommand `%s' invalid\n\n", cmd);
		__pr_help();
		res = 1;

	} else {
		/* prepare the execve */
		argv[0] = cmd_f;
		res = execv(cmd_f, argv);
	}
	return res;
}

/* ute.c ends here */
