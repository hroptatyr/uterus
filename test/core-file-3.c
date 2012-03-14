#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <uterus.h>

/* like core-file-2 but writes sandwich-2 ticks */
int
main(void)
{
	utectx_t ctx;
	int res = 0;
	const char *cfn;
	char *fn;

	if ((ctx = ute_mktemp(UO_RDWR)) == NULL) {
		perror("core-file-3");
		res = 1;
		goto out;
	}
	if ((cfn = ute_fn(ctx)) == NULL) {
		perror("core-file-3");
		res = 1;
		goto out;
	}
	fn = strdup(cfn);

	for (size_t i = 0; i < 200; i++) {
		struct scdl_s foo;

		/* set at least the tick type so ute_add_tick()
		 * knows that this is a sl1t */
		scom_thdr_set_ttf(AS_SCOM_THDR(&foo), SL1T_TTF_UNK);
		scom_thdr_set_linked(AS_SCOM_THDR(&foo));
		ute_add_tick(ctx, AS_SCOM(&foo));
	}
	ute_close(ctx);
	puts(fn);
	free(fn);
out:
	return res;
}

/* core-file-3.c ends here */
