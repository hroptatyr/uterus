#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "utefile.h"
#include "sl1t.h"

int
main(void)
{
	const int ofl = UO_RDWR | UO_CREAT | UO_TRUNC | UO_STREAM;
	utectx_t ctx = ute_open("stream-01.ute", ofl);
	struct sl1t_s t1 = {
		.hdr = {{
				 .sec = 123456789U,
				 .msec = 123,
				 .idx = 1,
				 .ttf = SL1T_TTF_BID,
			 }},
		.bid = 1234,
		.bsz = 0,
	};
	struct sl1t_s t2 = {
		.hdr = {{
				 .sec = 123456789U,
				 .msec = 123,
				 .idx = 1,
				 .ttf = SL1T_TTF_ASK,
			 }},
		.bid = 1236,
		.bsz = 0,
	};

	for (size_t i = 0; i < 70000; i++) {
		ute_add_tick(ctx, AS_SCOM(&t1));
		ute_add_tick(ctx, AS_SCOM(&t2));
		t1.hdr->sec++;
		t2.hdr->sec++;
		ute_add_tick(ctx, AS_SCOM(&t1));
		ute_add_tick(ctx, AS_SCOM(&t2));
		t1.hdr->sec++;
		t2.hdr->sec++;
	}

	/* just quit */
	ute_free(ctx);
	return 0;
}

/* stream-01.c ends here */
