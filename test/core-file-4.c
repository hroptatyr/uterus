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
		/* at least 200, at most ~8M */
		return 200 + random() % 8388608;
	}
	return res;
}

static void
my_add_tick(utectx_t ctx, scom_t t, size_t n __attribute__((unused)))
{
	return (void)ute_add_tick(ctx, t);
}

/* like core-file-2 and -3 but write single/double sandwich ticks randomly */
int
main(int argc, char *argv[])
{
/* Usage: core-file-4 #TICKS_AT_LEAST */
	utectx_t ctx;
	int res = 0;
	const char *cfn;
	char *fn;
	size_t max;

	max = snarf_nticks(argc, argv);
	UDEBUG("generating %zu ticks\n", max);

	if ((ctx = ute_mktemp(UO_RDWR)) == NULL) {
		perror("core-file-4");
		res = 1;
		goto out;
	}
	if ((cfn = ute_fn(ctx)) == NULL) {
		perror("core-file-4");
		res = 1;
		goto out;
	}
	fn = strdup(cfn);

	for (sidx_t tot = 0, totnt = 0; tot < max;) {
		/* make sure we have breathing space */
		struct sndwch_s stor[4];
		size_t sngl = 0, dbl = 0, quad = 0;

		/* initialise */
		memset(stor, 0x93 + tot, sizeof(stor));

#define YIELD	(size_t)random() % 256U && tot < max
#define ute_add_tick(c, t)	my_add_tick(c, t, tot)
		/* start off with single sandwiches */
		scom_thdr_set_ttf(AS_SCOM_THDR(stor), SL1T_TTF_UNK);
		for (sidx_t i = 0; i < YIELD; i++, tot++, sngl++) {
			ute_add_tick(ctx, AS_SCOM(stor));
		}

		/* go for double sandwiches now */
		scom_thdr_set_linked(AS_SCOM_THDR(stor));
		for (sidx_t i = 0; i < YIELD; i++, tot++, dbl++) {
			ute_add_tick(ctx, AS_SCOM(stor));
		}

		/* and double-doubles */
		scom_thdr_set_ttf(AS_SCOM_THDR(stor), SCOM_FLAG_L2M);
		for (sidx_t i = 0; i < YIELD; i++, tot++, quad++) {
			ute_add_tick(ctx, AS_SCOM(stor));
		}

		totnt += sngl + 2 * dbl + 4 * quad;
		UDEBUG("%zus %zud %zuq\t%zut\t%zu\n",
		       sngl, dbl, quad, totnt, tot);
#undef YIELD
	}
	ute_close(ctx);
	puts(fn);
	free(fn);
out:
	return res;
}

/* core-file-4.c ends here */
