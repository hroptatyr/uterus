/*** ute.c -- command line tool for libuterus
 *
 * Copyright (C) 2010 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <sebastian.freundt@ga-group.nl>
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
#include <stddef.h>
#include <stdio.h>
#include "utefile.h"
#include "sl1t.h"

/* for our options parser */
#include "ute-opt.h"
#include "version.h"

/* simple all in one tool */
#include "ute-print.c"
#include "ute-mux.c"
#include "ute-shnot.h"

static int
ute_cmd_sort(ute_opt_t UNUSED(octx), int UNUSED(argc), const char *argv[])
{
	const char *file = argv[1];
	void *hdl = ute_open(file, UO_RDWR);
	ute_sort(hdl);
	ute_close(hdl);
	return 0;
}


#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#endif	/* __INTEL_COMPILER */
#include "ute-clo.h"
#include "ute-clo.c"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
# pragma warning (default:181)
#endif	/* __INTEL_COMPILER */

int
main(int argc, char *argv[])
{
	struct gengetopt_args_info argi[1];
	int res = 0;
	struct ute_opt_s octx[1];

	if (cmdline_parser(argc, argv, argi)) {
		res = 1;
		goto out;
	}

#if 0
	if (ute_popt(octx, argc, argv) < 0 || octx->cmd == NULL) {
		ute_cmd_help(octx, octx->argc, octx->argv);
		return 1;
	}

	return octx->cmd(octx, octx->argc, octx->argv);
#endif
out:
	cmdline_parser_free(argi);
	return res;
}

/* ute.c ends here */
