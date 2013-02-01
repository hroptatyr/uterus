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
#include <mex.h>
#include "nifty.h"

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

static inline void*
umx_get_handle(const mxArray *arr)
{
	void **ptr;

	if (UNLIKELY(!mxIsIndex(arr))) {
		return NULL;
	} else if (UNLIKELY((ptr = (void**)mxGetData(arr)) == NULL)) {
		return NULL;
	}
	return ptr[0];
}

static inline void
umx_put_handle(const mxArray *tgt, void *hdl)
{
	void **ptr;

	if (UNLIKELY(!mxIsIndex(tgt))) {
		return;
	} else if (UNLIKELY((ptr = (void**)mxGetData(tgt)) == NULL)) {
		return;
	}
	ptr[0] = hdl;
	return;
}

#endif	/* INCLUDED_ute_handle_h_ */
