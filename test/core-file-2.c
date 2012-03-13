#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <uterus.h>

int
main(void)
{
	utectx_t ctx;
	int res = 0;
	const char *cfn;
	char *fn;

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

	for (size_t i = 0; i < 200; i++) {
		struct sl1t_s foo;
		ute_add_tick(ctx, AS_SCOM(&foo));
	}
	ute_close(ctx);
	puts(fn);
	free(fn);
out:
	return res;
}

/* core-file-2.c ends here */
