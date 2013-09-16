/*** ut-test.c -- just to work around getopt issues in ut-test.sh */

#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <unistd.h>
#include <stdio.h>

#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#endif	/* __INTEL_COMPILER */
#include "ut-test.h"
#include "ut-test.x"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
# pragma warning (default:181)
#endif	/* __INTEL_COMPILER */

int
main(int argc, char *argv[])
{
	struct gengetopt_args_info argi[1];
	const char *srcdir;
	const char *blddir;
	char *shpart;

	if (cmdline_parser(argc, argv, argi)) {
		goto out;
	} else if (argi->inputs_num != 1) {
		print_help_common();
		goto out;
	}

	if (argi->builddir_given) {
		blddir = argi->builddir_arg;
	} else {
		blddir = getenv("builddir");
	}
	if (argi->srcdir_given) {
		srcdir = argi->srcdir_arg;
	} else {
		srcdir = getenv("srcdir");
	}
	if (argi->hash_given) {
		setenv("hash", argi->hash_arg, 1);
	}
	if (argi->husk_given) {
		setenv("husk", argi->husk_arg, 1);
	}

	/* just to be clear about this */
#if defined WORDS_BIGENDIAN
	setenv("endian", "big", 1);
#else  /* !WORDS_BIGENDIAN */
	setenv("endian", "little", 1);
#endif	/* WORDS_BIGENDIAN */

	/* bang the actual testfile */
	setenv("testfile", argi->inputs[0], 1);

	/* and our pwd */
	{
		static char cwd[4096U];
		setenv("testdir", getcwd(cwd, sizeof(cwd)), 1);
	}

	/* promote srcdir */
	if (srcdir) {
		static char buf[4096U];
		ssize_t res;

		if ((res = readlink(srcdir, buf, sizeof(buf))) >= 0) {
			buf[res] = '\0';
			srcdir = buf;
		}
		if (realpath(srcdir, buf) != NULL) {
			srcdir = buf;
		}

		/* bang the srcdir */
		setenv("srcdir", srcdir, 1);
	}

	/* promot builddir */
	if (blddir) {
		static char buf[4096U];
		ssize_t res;

		if ((res = readlink(blddir, buf, sizeof(buf))) >= 0) {
			buf[res] = '\0';
			blddir = buf;
		}
		if (realpath(blddir, buf) != NULL) {
			blddir = buf;
		}

		/* bang builddir */
		setenv("builddir", blddir, 1);
	}

	/* build the command */
	{
		static const char fn[] = "ut-test.sh";
		static char buf[4096];
		size_t idx = 0UL;

		if (srcdir && (idx = strlen(srcdir))) {
			memcpy(buf, srcdir, idx);
			if (buf[idx - 1] != '/') {
				buf[idx++] = '/';
			}
		}
		memcpy(buf + idx, fn, sizeof(fn));
		shpart = buf;
	}

	/* exec the test script */
	{
		char *const new_argv[] = {"ut-test.sh", shpart, NULL};

		if (execv("/bin/sh", new_argv)) {
			perror("shell part not found");
		}
	}

out:
	cmdline_parser_free(argi);
	/* never succeed */
	return 1;
}

/* ut-test.c ends here */
