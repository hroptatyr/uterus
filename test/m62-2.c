#include <stdio.h>
#include <string.h>
#include "m62.h"

int
main(void)
{
	static char foo[] = "1.32, 1.44";
	const char *q = foo;
	m62_t x = ffff_m62_get_s(&q);
	char buf[64];
	int res = 0;

	if (__m62_mant(x) != 13200) {
		fputs("mantissa wrong\n", stderr);
		res = 1;
	}
	if (*q != ',' || q != foo + 4) {
		fputs("target pointer points to rubbish\n", stderr);
		res = 1;
	}
	if (x.expo != 1) {
		fputs("exponent wrong\n", stderr);
		res = 1;
	}
#define PR	"1.3200"
	if (ffff_m62_s(buf, x) != sizeof(PR) - 1 || strcmp(buf, PR)) {
		fprintf(stderr, "\
printed representation is `%s'  v  `"PR"'\n", buf);
		res = 1;
	}
	return res;
}
