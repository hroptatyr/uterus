/*** ute_tick.c -- take a sip of an ute bottle
 *
 * Copyright (C) 2013 Sebastian Freundt
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
#include "m30.h"
#include "m62.h"
#include "nifty.h"
#include "ute_handle.h"

static double
matlabify_stamp(scom_t s)
{
	double dsec = (double)scom_thdr_sec(s);
	double dmsec = (double)scom_thdr_msec(s);
	return (dsec + dmsec / 1000.0) / 86400.0 + 719529.0;
}

static void
umx_fill_scom(mxArray *arr, scom_t s)
{
	struct {
		double dt;
		double si;
		double tt;
		double t[];
	} *t = mxGetData(arr);
	uint16_t si = scom_thdr_tblidx(s);
	uint16_t ttf = scom_thdr_ttf(s);

	t->dt = matlabify_stamp(s);
	t->si = (double)si;
	t->tt = (double)ttf;

	switch (ttf) {
	case SL1T_TTF_BID ... SL1T_TTF_G32:
	case SL2T_TTF_BID ... SL2T_TTF_ASK:
	case SL1T_TTF_BIDASK:
		t->t[0] = ffff_m30_d(AS_CONST_SL1T(s)->v[0]);
		t->t[1] = ffff_m30_d(AS_CONST_SL1T(s)->v[1]);
		break;
	case SL1T_TTF_VOL ... SL1T_TTF_OI:
	case SL1T_TTF_G64:
		t->t[0] = ffff_m62_d(AS_CONST_SL1T(s)->w[0]);
		break;
	default:
		break;
	}
	return;
}

static mxArray*
umx_render_scom(scom_t s)
{
	mwSize dims[] = {3};
	mxArray *res;

	switch (scom_thdr_ttf(s)) {
	case SL1T_TTF_BID ... SL1T_TTF_G32:
	case SL2T_TTF_BID ... SL2T_TTF_ASK:
	case SL1T_TTF_BIDASK:
		dims[0] += 2;
		break;
	case SL1T_TTF_VOL ... SL1T_TTF_OI:
	case SL1T_TTF_G64:
		dims[0] += 1;
		break;
	default:
		break;
	}

	res = mxCreateNumericArray(countof(dims), dims, mxDOUBLE_CLASS, mxREAL);
	umx_fill_scom(res, s);
	return res;
}


void
mexFunction(int UNUSED(nlhs), mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
	utectx_t hdl;
	size_t nt;
	sidx_t ci;
	scom_t s;

	if (nrhs != 1) {
		mexErrMsgTxt("invalid usage, see `help ute_nticks'");
		return;
	} else if ((hdl = umx_get_handle(prhs[0])) == NULL) {
		mexErrMsgTxt("ute handle seems buggered");
		return;
	} else if ((nt = umx_get_nticks(prhs[0])) == 0) {
		/* no ticks in there */
		return;
	} else if ((ci = umx_get_cidx(prhs[0])) >= nt) {
		/* bottle empty */
		return;
	}

	if ((s = ute_seek(hdl, ci)) != NULL) {
		size_t tsz = scom_tick_size(s);

		/* update ci counter */
		umx_set_cidx(prhs[0], ci + tsz);

		plhs[0] = umx_render_scom(s);
	}
	return;
}

/* ute_tick.c ends here */
