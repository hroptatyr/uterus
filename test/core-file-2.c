#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <uterus.h>

#if defined DEBUG_FLAG
# define UDEBUG(args...)	fprintf(stderr, args)
#else
# define UDEBUG(args...)
#endif	/* DEBUG_FLAG */

static unsigned int
snarf_nticks(int argc, char *argv[])
{
	unsigned int res;

	if (argc == 1 || (res = strtol(argv[1], NULL, 0)) == 0) {
		/* at least 200 */
		return 200;
	}
	return res;
}

int
main(int argc, char *argv[])
{
	utectx_t ctx;
	int res = 0;
	const char *cfn;
	char *fn;
	size_t max;

	max = snarf_nticks(argc, argv);
	UDEBUG("generating %zu ticks\n", max);

	if ((ctx = ute_mktemp(UO_RDWR)) == NULL) {
		perror("core-file-2");
		res = 1;
		goto out;
	}
	if ((cfn = ute_fn(ctx)) == NULL) {
		perror("core-file-2");
		res = 1;
		goto out;
	}
	fn = strdup(cfn);

	for (size_t i = 0; i < max; i++) {
		/* make sure we have breathing space */
		struct sndwch_s stor[1];

		/* initialise */
		memset(stor, 0x93 + i, sizeof(stor));

		/* set at least the tick type so ute_add_tick()
		 * knows that this is a sl1t */
		scom_thdr_set_ttf(AS_SCOM_THDR(stor), SCOM_TTF_UNK);
		ute_add_tick(ctx, AS_SCOM(stor));
	}
	ute_close(ctx);
	puts(fn);
	free(fn);
out:
	return res;
}

/* core-file-2.c ends here */
