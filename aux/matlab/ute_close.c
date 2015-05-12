/*** ute_close.c -- ute bottle lid for matlab
 *
 * Copyright (C) 2013-2015 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@ga-group.nl>
 *
 * This file is part of uterus.
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
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
/* matlab stuff */
#include <mex.h>
/* our stuff */
#include "uterus.h"
#include "nifty.h"
#include "ute_handle.h"

static void
wipe_handle(const mxArray *arr)
{
	umx_put_handle(arr, NULL);
	return;
}

static int
ute_close1(const mxArray *mxhdl)
{
	utectx_t hdl;

	if ((hdl = umx_get_handle(mxhdl)) == NULL) {
		mexWarnMsgTxt("ute handle seems buggered");
		return -1;
	}
	ute_close(hdl);
	wipe_handle(mxhdl);
	return 0;
}


void
mexFunction(
	int UNUSED(nlhs), mxArray *UNUSED(plhs[]),
	int nrhs, const mxArray *prhs[])
{
	if (nrhs != 1) {
		mexErrMsgTxt("invalid usage, see `help ute_close'");
		return;
	}

	if (LIKELY(!mxIsCell(*prhs))) {
		if (UNLIKELY(ute_close1(*prhs) < 0)) {
			mexErrMsgTxt("");
		}
		return;
	}

	/* otherwise loop over the cell array */
	const mxArray **rhs = mxGetData(*prhs);
	const size_t nfn = mxGetNumberOfElements(*prhs);

	for (size_t i = 0U; i < nfn; i++) {
		if (LIKELY(rhs[i] != NULL)) {
			(void)ute_close1(rhs[i]);
		}
	}
	return;
}

/* ute_close.c ends here */
