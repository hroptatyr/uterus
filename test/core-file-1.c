#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "utefile.h"

int
main(void)
{
	utectx_t ctx = ute_mktemp(UO_RDWR);
	char *fn = strdup(ute_fn(ctx));

	ute_close(ctx);
	puts(fn);
	free(fn);
	return 0;
}

/* core-file-1.c ends here */
