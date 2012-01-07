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

#if !defined UNUSED
# define UNUSED(_x)	__attribute__((unused)) _x
#endif	/* !UNUSED */

/* sub-command handling */
typedef int(*subcmd_f)(int, char *argv[]);

struct subcmd_s {
	const char *name;
	subcmd_f cmdf;
};

static size_t nsubcmd = 0;
static struct subcmd_s subcmd[32];

#define CMDFUN(x)				\
	ute_cmd_##x

#define DEFCMD(x)				\
	static int CMDFUN(x)

#define PUSHCMD(x)				\
	subcmd[nsubcmd].name = #x;		\
	subcmd[nsubcmd].cmdf = CMDFUN(x);	\
	nsubcmd++


DEFCMD(sort)(int UNUSED(argc), char *argv[])
{
	const char *file = argv[1];
	void *hdl = ute_open(file, UO_RDWR);
	ute_sort(hdl);
	ute_close(hdl);
	return 0;
}

DEFCMD(version)(int UNUSED(argc), char *UNUSED(argv)[])
{
	puts("ute " UTE_VERSION);
	return 0;
}

DEFCMD(help)(int UNUSED(argc), char *UNUSED(argv)[])
{
#define SUBCMD(x, line)					\
	do {						\
		printf("  % -20s " line "\n", #x);	\
	} while (0)

	/* output version first */
	ute_cmd_version(0, NULL);

	puts("\
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
	return 0;
}


/* we expect UTEDIR to be defined at this point */
#if !defined UTEDIR
# error define UTEDIR
#endif	/* UTEDIR */

static void
init_cmds(void)
{
	PUSHCMD(help);
	PUSHCMD(version);
	PUSHCMD(sort);
	return;
}

static subcmd_f
get_subcmd(const char *cmd)
{
	for (size_t i = 0; i < nsubcmd; i++) {
		if (strcmp(cmd, subcmd[i].name) == 0) {
			return subcmd[i].cmdf;
		}
	}
	return NULL;
}

static void
rem_hole(int argc, char *argv[], int from)
{
	for (int i = from; i < argc; i++) {
		argv[i] = argv[i + 1];
	}
	return;
}

static char*
extract_cmd(int argc, char *argv[])
{
/* try and find a subcommand and rearrange argv to leave no holes */
	int dash_dash_seen_p = 0;

	for (int i = 1; i < argc; i++) {
		if (argv[i][0] != '-' || dash_dash_seen_p) {
			char *res = argv[i];
			rem_hole(argc, argv, i);
			return res;
		} else if (argv[i][1] == '-' && argv[i][2] == '\0') {
			dash_dash_seen_p = 1;
		}
	}
	return NULL;
}

static subcmd_f
rewrite_subcmd(int argc, char *argv[])
{
/* try and rewrite subcommands and rearrange argv to leave no holes */
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--help") == 0 ||
		    strcmp(argv[i], "-h") == 0) {
			rem_hole(argc, argv, i);
			return CMDFUN(help);
		} else if (strcmp(argv[i], "--version") == 0 ||
			   strcmp(argv[i], "-V") == 0) {
			rem_hole(argc, argv, i);
			return CMDFUN(version);
		} else if (strcmp(argv[i], "--") == 0) {
			break;
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


int
main(int argc, char *argv[])
{
	char *cmd_f = NULL;
	subcmd_f icmd = NULL;
	const char *cmd;
	int res = 0;

	init_cmds();
	if ((cmd = extract_cmd(argc, argv)) == NULL &&
	    /* try the rewriter */
	    (icmd = rewrite_subcmd(argc, argv)) == NULL) {
		ute_cmd_help(argc, argv);
		res = 1;

	} else if (cmd != NULL && (cmd_f = build_cmd(cmd)) != NULL) {
		/* prepare the execve */
		argv[0] = cmd_f;
		res = execv(cmd_f, argv);

	} else if (icmd != NULL || (icmd = get_subcmd(cmd)) != NULL) {
		/* call the internal command */
		res = icmd(argc, argv);

	} else {
		fprintf(stderr, "ute subcommand `%s' invalid\n\n", cmd);
		ute_cmd_help(argc, argv);
		res = 1;
	}
	return res;
}

/* ute.c ends here */
