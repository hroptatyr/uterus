/*** ute_tick.c -- take a sip of an ute bottle
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
#if defined HAVE_OCTAVE_MEX_H
# include <octave/mex.h>
#else  /* !HAVE_OCTAVE_MEX_H */
# include <mex.h>
#endif	/* HAVE_OCTAVE_MEX_H */
/* our stuff */
#include "uterus.h"
#include "sl1t.h"
#include "scdl.h"
#include "ssnp.h"
#include "m30.h"
#include "m62.h"
#include "nifty.h"
#include "ute_handle.h"

static __attribute__((pure)) size_t
scom_dims(scom_t s)
{
	size_t dims = 3U;

	if (UNLIKELY(s == NULL)) {
		return 0U;
	}

	switch (scom_thdr_ttf(s)) {
	case SL1T_TTF_BID ... SL1T_TTF_G32:
	case SL2T_TTF_BID ... SL2T_TTF_ASK:
	case SL1T_TTF_BIDASK:
		dims += 2;
		break;
	case SL1T_TTF_VOL ... SL1T_TTF_OI:
	case SL1T_TTF_G64:
		dims += 1;
		break;

		/* candles */
	case SCDL_FLAVOUR | SL1T_TTF_BID ... SCDL_FLAVOUR | SL1T_TTF_G32:
		dims += 4;
		break;
	case SCDL_FLAVOUR | SL1T_TTF_VOL:
		dims += 3;
		break;

		/* snaps */
	case SSNP_FLAVOUR:
		dims += 6;
		break;
	default:
		break;
	}
	return dims;
}

static double
matlabify_stamp(scom_t s)
{
	double dsec = (double)scom_thdr_sec(s);
	double dmsec = (double)scom_thdr_msec(s);
	return (dsec + dmsec / 1000.0) / 86400.0 + 719529.0;
}

static mxArray*
umx_render_scom(scom_t s)
{
	mwSize dims[] = {scom_dims(s)};
	mxArray *res = mxCreateNumericArray(
		countof(dims), dims, mxDOUBLE_CLASS, mxREAL);
	struct {
		double dt;
		double si;
		double tt;
		double t[];
	} *t = mxGetData(res);
	unsigned int si;
	uint16_t ttf;

	if (UNLIKELY(s == NULL)) {
		return res;
	}

	/* otherwise things should go smoothly here */
	si = scom_thdr_tblidx(s);
	ttf = scom_thdr_ttf(s);

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

		/* candles */
	case SCDL_FLAVOUR | SL1T_TTF_BID ... SCDL_FLAVOUR | SL1T_TTF_G32:
		t->t[0] = ffff_m30_d(AS_CONST_SCDL(s)->o);
		t->t[1] = ffff_m30_d(AS_CONST_SCDL(s)->h);
		t->t[2] = ffff_m30_d(AS_CONST_SCDL(s)->l);
		t->t[3] = ffff_m30_d(AS_CONST_SCDL(s)->c);
		break;
	case SCDL_FLAVOUR | SL1T_TTF_VOL:
		t->t[0] = ffff_m62_d(AS_CONST_SCDL(s)->vol);
		t->t[1] = ffff_m30_d(AS_CONST_SCDL(s)->vwap);
		t->t[2] = ffff_m30_d(AS_CONST_SCDL(s)->twap);
		break;

		/* snaps */
	case SSNP_FLAVOUR:
		t->t[0] = ffff_m30_d(AS_CONST_SSNP(s)->bp);
		t->t[1] = ffff_m30_d(AS_CONST_SSNP(s)->bq);
		t->t[2] = ffff_m30_d(AS_CONST_SSNP(s)->ap);
		t->t[3] = ffff_m30_d(AS_CONST_SSNP(s)->aq);
		t->t[4] = ffff_m30_d(AS_CONST_SSNP(s)->tvpr);
		t->t[5] = ffff_m30_d(AS_CONST_SSNP(s)->tq);
		break;

	default:
		break;
	}
	return res;
}


void
mexFunction(int UNUSED(nlhs), mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
	utectx_t hdl;
	scom_t s;

	if (nrhs != 1) {
		mexErrMsgTxt("invalid usage, see `help ute_nticks'");
		return;
	} else if ((hdl = umx_get_handle(prhs[0])) == NULL) {
		mexErrMsgTxt("ute handle seems buggered");
		return;
	}

	s = ute_iter(hdl);
	plhs[0] = umx_render_scom(s);
	return;
}

/* ute_tick.c ends here */
