/*** ute_slut.c -- return list of ingredients of ute bottles
 *
 * Copyright (C) 2013-2014 Sebastian Freundt
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
#if defined HAVE_OCTAVE_MEX_H
# include <octave/mex.h>
#else  /* !HAVE_OCTAVE_MEX_H */
# include <mex.h>
#endif	/* HAVE_OCTAVE_MEX_H */
/* our stuff */
#include "uterus.h"
#include "nifty.h"
#include "ute_handle.h"


void
mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
	utectx_t hdl;
	size_t nsyms;

	if (nrhs != 1 || nlhs > 1) {
		mexErrMsgTxt("invalid usage, see `help ute_slut'");
		return;
	} else if ((hdl = umx_get_handle(prhs[0])) == NULL) {
		mexErrMsgTxt("ute handle seems buggered");
		return;
	}
	/* how many are we talking */
	nsyms = ute_nsyms(hdl);
	/* gen the cell array, ... */
	{
		mwSize dims[] = {nsyms};
		plhs[0] = mxCreateCellArray(countof(dims), dims);
	}
	/* ... and populate */
	for (size_t i = 0; i < nsyms; i++) {
		mxArray *sym = mxCreateString(ute_idx2sym(hdl, i + 1));

		mxSetCell(plhs[0], i, sym);
	}
	return;
}

/* ute_slut.c ends here */
