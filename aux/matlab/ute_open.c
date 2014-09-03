/*** ute_open.c -- ute bottle opener for matlab
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
#include <stdint.h>
#include <stdbool.h>
/* matlab stuff */
#include <mex.h>
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
	char *tmp = mxArrayToString(fn);
	char *res = strdup(tmp);
	mxFree(tmp);
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

static char*
recv_remote_fname(char *uri)
{
#if defined HAVE_CURL_CURL_H
	static char tmpf[] = ".ute_remote.XXXXXXXX";
	CURL *cctx;
	char *fn = NULL;
	int fd;
	FILE *fp;
	int rc;

	if (UNLIKELY((cctx = curl_easy_init()) == NULL)) {
		goto out;
	} else if (UNLIKELY((fd = mkstemp(tmpf)) < 0)) {
		goto out;
	}

	/* hand-over to libcurl */
	if (LIKELY((fp = fdopen(fd, "w+")) != NULL)) {
		curl_easy_setopt(cctx, CURLOPT_URL, uri);
		curl_easy_setopt(cctx, CURLOPT_WRITEDATA, fp);
		curl_easy_setopt(cctx, CURLOPT_NOSIGNAL, 1);
		curl_easy_setopt(cctx, CURLOPT_TIMEOUT, 10/*seconds*/);
		curl_easy_setopt(cctx, CURLOPT_ENCODING, "gzip");
		rc = curl_easy_perform(cctx);

		/* definitely close the file */
		fclose(fp);
	}

	if (rc == CURLE_OK) {
		/* celebrate success */
		fn = strdup(tmpf);
	} else {
		/* prepare for unlink */
		close(fd);
		fd = -1;
	}

out:
	/* kill original uri, in either case */
	mxFree(uri);

	if (UNLIKELY(fd < 0)) {
		/* already unlink the guy as later we won't
		 * have the file name handy anymore */
		(void)unlink(fn);
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


void
mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
	bool remotep;
	utectx_t hdl;
	char *fn;

	if (nrhs != 1 || nlhs != 1) {
		mexErrMsgTxt("invalid usage, see `help ute_open'");
		return;
	} else if ((fn = snarf_fname(prhs[0])) == NULL) {
		mexErrMsgTxt("cannot determine file name to open");
		return;
	} else if ((remotep = remote_fname_p(fn)) &&
		   (fn = recv_remote_fname(fn)) == NULL) {
		mexErrMsgTxt("cannot download remote resource");
		return;
	} else if ((hdl = ute_open(fn, UO_RDONLY)) == NULL) {
		mexErrMsgTxt("could not open ute file");
	} else {
		/* otherwise just assign the handle */
		plhs[0] = make_umx_handle();
		umx_put_handle(plhs[0], hdl);
	}

	/* unlink file name in case of remotes */
	if (remotep) {
		(void)unlink(fn);
	}
	/* free file name */
	free(fn);
	return;
}

/* ute_open.c ends here */
