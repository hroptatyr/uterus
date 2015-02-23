/*** ute_open.c -- ute bottle opener for matlab
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
#include <stdint.h>
#include <sys/stat.h>
/* matlab stuff */
#if defined HAVE_OCTAVE_MEX_H
# include <octave/mex.h>
#else  /* !HAVE_OCTAVE_MEX_H */
# include <mex.h>
#endif	/* HAVE_OCTAVE_MEX_H */
#if defined HAVE_CURL_CURL_H
# include <curl/curl.h>
#endif	/* HAVE_CURL_CURL_H */
/* our stuff */
#include "uterus.h"
#include "nifty.h"
#include "ute_handle.h"

static char*
snarf_fname(const mxArray *fn)
{
	char *res = NULL;

	if (UNLIKELY(!mxIsChar(fn))) {
		/* big bugger */
		;
	} else if (UNLIKELY((res = mxArrayToString(fn)) == NULL)) {
		/* huh? */
		;
	} else {
		char *tmp = strdup(res);
		mxFree(res);
		res = tmp;
	}
	return res;
}

static bool
remote_fname_p(const char *fn)
{
	if (!strncmp(fn, "http://", 7U)) {
		return true;
	}
	return false;
}

#if defined HAVE_CURL_CURL_H
static size_t
_data_cb(void *buf, size_t chrz, size_t nchr, void *clo)
{
	const int fd = (intptr_t)clo;

	return write(fd, buf, chrz * nchr);
}

static int
mktempp(char *restrict tmpl[static 1U], int prefixlen)
{
	static mode_t umsk;
	char *bp = *tmpl + prefixlen;
	char *const ep = *tmpl + strlen(*tmpl);
	mode_t m;
	int fd;

	if (UNLIKELY(!umsk)) {
		umsk = umask(0022);
	}
	if (ep[-6] != 'X' || ep[-5] != 'X' || ep[-4] != 'X' ||
	    ep[-3] != 'X' || ep[-2] != 'X' || ep[-1] != 'X') {
		if ((fd = open(bp, O_RDWR | O_CREAT | O_EXCL, 0666)) < 0 &&
		    (bp -= prefixlen,
		     fd = open(bp, O_RDWR | O_CREAT | O_EXCL, 0666)) < 0) {
			/* fuck that then */
			return -1;
		}
	} else if (m = umask(S_IXUSR | S_IRWXG | S_IRWXO),
		   UNLIKELY((fd = mkstemp(bp), umask(m), fd < 0)) &&
		   UNLIKELY((bp -= prefixlen,
			     /* reset to XXXXXX */
			     memset(ep - 6, 'X', 6U),
			     fd = mkstemp(bp)) < 0)) {
		/* at least we tried */
		return -1;
	}
	/* store result */
	*tmpl = bp;
	return fd;
}
#endif	/* HAVE_CURL_CURL_H */


static char*
recv_remote_fname(char *uri)
{
#if defined HAVE_CURL_CURL_H
	char _tmpf[] = P_tmpdir "/" ".ute_remote.XXXXXXXX";
	char *tmpf = _tmpf;
	CURL *cctx;
	char *fn = NULL;
	int fd = -1;
	CURLcode rc = CURL_LAST;

	if (UNLIKELY((cctx = curl_easy_init()) == NULL)) {
		goto out;
	} else if (UNLIKELY((fd = mktempp(&tmpf, sizeof(P_tmpdir))) < 0)) {
		goto out;
	}

	/* hand-over to libcurl */
	curl_easy_setopt(cctx, CURLOPT_URL, uri);
	curl_easy_setopt(cctx, CURLOPT_WRITEFUNCTION, _data_cb);
	curl_easy_setopt(cctx, CURLOPT_WRITEDATA, (intptr_t)fd);
	curl_easy_setopt(cctx, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(cctx, CURLOPT_TIMEOUT, 10/*seconds*/);
	curl_easy_setopt(cctx, CURLOPT_ENCODING, "gzip");
	rc = curl_easy_perform(cctx);

	/* definitely close the file */
	close(fd);

	switch (rc) {
	case CURLE_OK:
		/* celebrate success */
		fn = strdup(tmpf);
		break;
	default:
		mexWarnMsgTxt("connection error");
		/* prepare for unlink */
		fd = -1;
	}

out:
	/* kill original uri, in either case */
	free(uri);

	if (UNLIKELY(fd < 0)) {
		if (tmpf != NULL) {
			/* already unlink the guy as later we won't
			 * have the file name handy anymore */
			(void)unlink(tmpf);
		}
	}
	if (LIKELY(cctx != NULL)) {
		curl_easy_cleanup(cctx);
	}
	return fn;
#else  /* !HAVE_CURL_CURL_H */
	(void)uri;
	return NULL;
#endif	/* HAVE_CURL_CURL_H */
}

static mxArray*
ute_open1(const mxArray *mxfn)
{
	bool remotep;
	utectx_t hdl;
	char *fn;
	mxArray *res = NULL;

	if ((fn = snarf_fname(mxfn)) == NULL) {
		mexWarnMsgTxt("cannot determine file name to open");
		return NULL;
	} else if ((remotep = remote_fname_p(fn)) &&
		   (fn = recv_remote_fname(fn)) == NULL) {
		mexWarnMsgTxt("cannot download remote resource");
		return NULL;
	} else if ((hdl = ute_open(fn, UO_RDONLY)) == NULL) {
		mexPrintf("file `%s' does not seem to be an ute file\n", fn);
		res = NULL;
	} else {
		/* otherwise just assign the handle */
		res = make_umx_handle();
		umx_put_handle(res, hdl);
	}

	/* unlink file name in case of remotes */
	if (remotep) {
		(void)unlink(fn);
	}
	/* free file name */
	free(fn);
	return res;
}


void
mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
	if (nrhs < 1 || nlhs != 1) {
		mexErrMsgTxt("invalid usage, see `help ute_open'");
		return;
	}

	/* now either prhs[0] is a string or a cell array of strings */
	if (LIKELY(!mxIsCell(*prhs))) {
		if (UNLIKELY((*plhs = ute_open1(*prhs)) == NULL)) {
			mexErrMsgTxt("");
		}
		return;
	}

	/* otherwise loop over the cell array */
	const mxArray **rhs = mxGetData(*prhs);
	const size_t nfn = mxGetNumberOfElements(*prhs);
	size_t nhdl = 0U;

	/* just create a nfn-by-1 cell array now */
	*plhs = mxCreateCellMatrix(nfn, 1U);
	for (size_t i = 0U; i < nfn; i++) {
		mxArray *tmp = ute_open1(rhs[i]);

		mxSetCell(*plhs, nhdl++, tmp);
	}
	return;
}

/* ute_open.c ends here */
