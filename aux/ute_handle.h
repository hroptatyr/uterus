/*** ute_handle.h -- ute bottle wraps
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
#if !defined INCLUDED_ute_handle_h_
#define INCLUDED_ute_handle_h_

#include <stdbool.h>
/* matlab stuff */
#include <mex.h>
/* our stuff */
#include "uterus.h"
#include "nifty.h"

static inline mxArray*
make_umx_handle(void)
{
	mwSize dims[] = {3};
	return mxCreateNumericArray(countof(dims), dims, mxINDEX_CLASS, mxREAL);
}

static inline bool
mxIsIndex(const mxArray *arr)
{
	if (mxINDEX_CLASS == mxUINT64_CLASS) {
		return mxIsUint64(arr);
	} else if (mxINDEX_CLASS == mxUINT32_CLASS) {
		return mxIsUint32(arr);
	}
	return false;
}

static inline utectx_t
umx_get_handle(const mxArray *arr)
{
	intptr_t *ptr;

	if (UNLIKELY(!mxIsIndex(arr))) {
		return NULL;
	} else if (UNLIKELY((ptr = mxGetData(arr)) == NULL)) {
		return NULL;
	}
	return (void*)ptr[0];
}

static inline void
umx_put_handle(const mxArray *tgt, utectx_t hdl)
{
	intptr_t *ptr;

	if (UNLIKELY(!mxIsIndex(tgt))) {
		return;
	} else if (UNLIKELY((ptr = mxGetData(tgt)) == NULL)) {
		return;
	}
	ptr[0] = (intptr_t)hdl;
	return;
}

static inline size_t
umx_get_nticks(const mxArray *src)
{
	intptr_t *ptr;

	if (UNLIKELY(!mxIsIndex(src))) {
		return 0UL;
	} else if (UNLIKELY((ptr = mxGetData(src)) == NULL)) {
		return 0UL;
	} else if (UNLIKELY(ptr[2] == 0)) {
		ptr[2] = ute_nticks((void*)ptr[0]);
	}
	return ptr[2];
}

static inline sidx_t
umx_get_cidx(const mxArray *src)
{
	intptr_t *ptr;

	if (UNLIKELY(!mxIsIndex(src))) {
		return -1UL;
	} else if (UNLIKELY((ptr = mxGetData(src)) == NULL)) {
		return -1UL;
	}
	return ptr[1];
}

static inline void
umx_set_cidx(const mxArray *src, sidx_t si)
{
	intptr_t *ptr;

	if (UNLIKELY(!mxIsIndex(src))) {
		return;
	} else if (UNLIKELY((ptr = mxGetData(src)) == NULL)) {
		return;
	}
	ptr[1] = si;
	return;
}

#endif	/* INCLUDED_ute_handle_h_ */