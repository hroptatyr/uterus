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


#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#endif	/* __INTEL_COMPILER */
#include "ute-clo.h"
#include "ute-clo.c"
#include "ute-shnot-clo.h"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
# pragma warning (default:181)
#endif	/* __INTEL_COMPILER */

static void
__pr_help(void)
{
#if 0
/* this would be the umpp way to do things */
#define SUBCMD(x)							\
	do {								\
		printf("  " #x "\n    %s\n", x##_args_info_description); \
		for (const char **p = x##_args_info_help + 2; *p; p++) { \
			putchar(' ');					\
			putchar(' ');					\
			puts(*p);					\
		}							\
	} while (0)

	puts(CMDLINE_PARSER_PACKAGE_NAME " " CMDLINE_PARSER_VERSION "\n");

	if (gengetopt_args_info_purpose && gengetopt_args_info_purpose[0]) {
		printf("%s\n\n", gengetopt_args_info_purpose);
	}
	if (gengetopt_args_info_usage && gengetopt_args_info_usage[0]) {
		printf("%s\n\n", gengetopt_args_info_usage);
	}
	if (gengetopt_args_info_description &&
	    gengetopt_args_info_description[0]) {
		printf("%s\n\n", gengetopt_args_info_description);
	}

	puts("Common options:");
	for (const char **p = gengetopt_args_info_help; *p; p++) {
		if (strncmp(*p, "      --full-help", 17)) {
			puts(*p);
		}
	}

	puts("\nSupported commands:");

	SUBCMD(help);
	SUBCMD(mux);
	SUBCMD(print);
	SUBCMD(shnot);
	//SUBCMD(sort);

#undef SUBCMD
#else  /* !0 */
	cmdline_parser_print_help();
#endif	/* 0 */
	return;
}

int
main(int argc, char *argv[])
{
	struct gengetopt_args_info argi[1];
	char *cmd_f = NULL;
	const char *cmd;
	int res = 0;

	if (cmdline_parser(argc, argv, argi)) {
		__pr_help();
		res = 1;
		goto out;
	} else if (argi->help_given) {
		__pr_help();
		res = 0;
		goto out;
	} else if (argi->inputs_num == 0) {
		fputs("COMMAND must be specified, see --help\n\n", stderr);
		__pr_help();
		res = 1;
		goto out;
	} else if ((cmd_f = build_cmd((cmd = argi->inputs[0]))) == NULL) {
		fprintf(stderr, "ute subcommand `%s' not found\n\n", cmd);
		__pr_help();
		res = 1;
		goto out;
	}

out:
	cmdline_parser_free(argi);

	if (cmd_f != NULL) {
		/* prepare the execve */
		argv[0] = cmd_f;
		res = execv(cmd_f, argv);
	}
	return res;
}

/* ute.c ends here */
