/*** ute-print.c -- ute tick printer
 *
 * Copyright (C) 2010 - 2012 Sebastian Freundt
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
#include <stdio.h>
#include <assert.h>
#include "utefile.h"
#include "sl1t.h"
#define DEFINE_GORY_STUFF
#include "m30.h"
#include "m62.h"

#include "timegm.c"
#include "strftime.c"

#include "ute-print.h"

#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif	/* !UNLIKELY */
#if !defined UNUSED
# define UNUSED(_x)	__attribute__((unused)) _x
#endif	/* !UNUSED */
#if !defined countof
# define countof(x)	(sizeof(x) / sizeof(*x))
#endif	/* !countof */


struct printer_s {
	const char *opt;
	ssize_t(*prf)(pr_ctx_t, scom_t);
};


static struct printer_s supported_printers[] = {
	{.opt = "ibrti", .prf = ibrti_pr},
};

static ssize_t
(*find_printer(const char *opt))(pr_ctx_t, scom_t)
{
	for (size_t i = 0; i < countof(supported_printers); i++) {
		/* at the moment we make use of the fact that
		 * all muxers begin with different letters. */
		if (strcmp(opt, supported_printers[i].opt) == 0) {
			return supported_printers[i].prf;
		}
	}
	return NULL;
}


#if defined STANDALONE
#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#endif	/* __INTEL_COMPILER */
#include "ute-print-clo.h"
#include "ute-print-clo.c"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
# pragma warning (default:181)
#endif	/* __INTEL_COMPILER */

int
main(int argc, char *argv[])
{
	struct print_args_info argi[1];
	struct pr_ctx_s ctx[1] = {{0}};
	ssize_t(*prf)(pr_ctx_t, scom_t) = NULL;
	int res = 0;

	if (print_parser(argc, argv, argi)) {
		res = 1;
		goto out;
	}

	for (unsigned int j = 0; j < argi->inputs_num; j++) {
		const char *f = argi->inputs[j];
		void *hdl;

		if ((hdl = ute_open(f, UO_RDONLY)) == NULL) {
			continue;
		}
		/* otherwise print all them ticks */
		ctx->uctx = hdl;
		for (size_t i = 0; i < ute_nticks(hdl); i++) {
			scom_t ti = ute_seek(hdl, i);
			if (ti) {
				prf(ctx, ti);
			}
		}
		/* oh right, close the handle */
		ute_close(hdl);
	}

out:
	print_parser_free(argi);
	return res;
}
#endif	/* STANDALONE */

/* ute-print.c ends here */
