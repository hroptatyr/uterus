/*** ute-slut.c -- symbol look-up table viewer and editor
 *
 * Copyright (C) 2012 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@ga-group.nl>
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
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>

#include "utefile-private.h"
/* we need some private titbits */
#include "uteslut.h"

/* we're just as good as rudi, aren't we? */
#if defined DEBUG_FLAG
# include <assert.h>
#else  /* !DEBUG_FLAG */
# define assert(args...)
#endif	/* DEBUG_FLAG */

#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif	/* !UNLIKELY */
#if !defined UNUSED
# define UNUSED(_x)	__attribute__((unused)) _x
#endif	/* !UNUSED */
#if !defined countof
# define countof(x)	(sizeof(x) / sizeof(*x))
#endif	/* !countof */


#if !defined DEFAULT_EDITOR
# define DEFAULT_EDITOR	"vi"
#endif	/* DEFAULT_EDITOR */

static const char*
ute_editor(void)
{
/* stolen from git's editor.c */
	const char *editor = getenv("UTE_EDITOR");
	const char *terminal = getenv("TERM");
	int terminal_dumb_p = !terminal || strcmp(terminal, "dumb") == 0;

	if (!editor && !terminal_dumb_p) {
		editor = getenv("VISUAL");
	}
	if (!editor) {
		editor = getenv("EDITOR");
	}
	if (!editor && terminal_dumb_p) {
		return NULL;
	}
	if (!editor) {
		editor = DEFAULT_EDITOR;
	}
	return editor;
}

static const char**
prep_sh_cmd(const char **argv)
{
	int argc, nargc = 0;
	const char **res;
	size_t a0len;

	/* count nargs */
	for (argc = 0; argv[argc]; argc++);

	/* +1 for NULL, +3 for "sh -c" plus extra $0 */
	res = malloc(sizeof(*res) * (argc + 1 + 3));
	a0len = strlen(argv[0]);

	if (argc < 1) {
		fputs("BUG: shell command is empty\n", stderr);
		return NULL;
	} else if (strcspn(argv[0], "|&;<>()$`\\\"' \t\n*?[#~=%") != a0len) {
		res[nargc++] = "sh";
		res[nargc++] = "-c";

		if (argc < 2) {
			res[nargc++] = argv[0];
		} else {
			static const char glue[] = " \"$@\"";
			char *a0 = malloc(a0len + sizeof(glue));
			memcpy(a0, argv[0], a0len);
			memcpy(a0 + a0len, glue, sizeof(glue));

			res[nargc++] = a0;
		}
	}

	for (argc = 0; argv[argc]; argc++) {
		res[nargc++] = argv[argc];
	}
	res[nargc] = NULL;
	return res;
}

static int
run_cmd(const char **argv, const char *const UNUSED(env)[])
{
	pid_t pid, wp;
	int status;
	int res = 0;

	if ((pid = fork()) < 0) {
		/* FFF: fork failed, fuck! */
		fprintf(stderr, "cannot fork(): %s: %s\n",
			argv[0], strerror(errno));
		return -1;
	} else if (pid == 0) {
		/* in child */
		const char **nu_argv = prep_sh_cmd(argv);

		execvp(nu_argv[0], (char**)nu_argv);
		if (argv[1]) {
			/* nu_argv[2] is alloc'd obviously */
			free(((char**)nu_argv)[2]);
		}
		free(nu_argv);
	}
	/* in parent */
	while ((wp = waitpid(pid, &status, 0)) < 0 && errno == EINTR);

	if (wp < 0) {
		fprintf(stderr, "waitpid for %s failed: %s\n",
			argv[0], strerror(errno));
	} else if (wp != pid) {
		fprintf(stderr, "waitpid is confused (%s)\n", argv[0]);
	} else if (WIFSIGNALED(status)) {
		res = WTERMSIG(status);
		fprintf(stderr, "%s died of signal %d\n", argv[0], res);
		/* This return value is chosen so that code & 0xff
		 * mimics the exit code that a POSIX shell would report for
		 * a program that died from this signal. */
		res -= 128;
	} else if (WIFEXITED(status)) {
		if ((res = WEXITSTATUS(status)) == 127) {
			/* Convert special exit code when execvp failed. */
			res = -1;
		}
	} else {
		fprintf(stderr, "waitpid is confused (%s)\n", argv[0]);
	}
	return res;
}

static int
dump_slut(int outfd, utectx_t hdl)
{
	/* per line max 5 for idx, 1 for \t, 64 for the sym and 1 for \n */
	char buf[5 + 1 + 64 + 1 + 1/*\nul*/];

	for (uint16_t j = 1; j <= hdl->slut->nsyms; j++) {
		const char *sym = ute_idx2sym(hdl, j);
		int sz = snprintf(buf, sizeof(buf), "%hu\t%s\n", j, sym);

		if (write(outfd, buf, sz) < 0) {
			return -1;
		}
	}
	return 0;
}

static int
slut1(utectx_t hdl, int editp)
{
	static char tmpf[] = "/tmp/slut.XXXXXX";
	int outfd = !editp ? STDOUT_FILENO : mkstemp(tmpf);
	int res = 0;

	/* dump slut first */
	if (dump_slut(outfd, hdl) < 0) {
		res = -1;
		goto out;
	} else if (!editp) {
		return 0;
	}

	/* otherwise launch the editor */
	{
		const char *editor = ute_editor();
		const char *args[] = {
			editor,
			tmpf,
			NULL
		};

		if (editor == NULL) {
			/* error message? */
			res = -1;
		} else if (strcmp(editor, ":") == 0) {
			/* no-op */
			;
		} else if (run_cmd(args, NULL) < 0) {
			fprintf(stderr, "\
There was a problem with the editor '%s'.", editor);
		}
	}
out:
	/* unprocess */
	close(outfd);
	unlink(tmpf);
	return res;
}


#if defined STANDALONE
#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#endif	/* __INTEL_COMPILER */
#include "ute-slut-clo.h"
#include "ute-slut-clo.c"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
# pragma warning (default:181)
#endif	/* __INTEL_COMPILER */

int
main(int argc, char *argv[])
{
	struct slut_args_info argi[1];
	int res = 0;

	if (slut_parser(argc, argv, argi)) {
		res = 1;
		goto out;
	} else if (argi->help_given) {
		slut_parser_print_help();
		res = 0;
		goto out;
	}

	for (unsigned int i = 0; i < argi->inputs_num; i++) {
		/* just quickly do it here */
		const char *fn = argi->inputs[i];
		utectx_t hdl;

		if ((hdl = ute_open(fn, UO_RDONLY)) == NULL) {
			fprintf(stderr, "cannot open file '%s'\n", fn);
			continue;
		} else if (argi->inputs_num > 1) {
			/* print file names when more than 1 */
			puts(fn);
		}

		/* process this file */
		slut1(hdl, argi->edit_given);

		/* we worship the ute god by giving back what belongs to him */
		ute_close(hdl);
	}

out:
	slut_parser_free(argi);
	return res;
}
#endif	/* STANDALONE */

/* ute-slut.c ends here */
