/*** mat.c -- matlab mat file muxer
 *
 * Copyright (C) 2012  Sebastian Freundt
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
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include "utefile.h"
#include "ute-print.h"

/* so we know about ticks, candles and snapshots */
#include "sl1t.h"
#include "scdl.h"

#if defined USE_DEBUGGING_ASSERTIONS
# include <assert.h>
#endif	/* USE_DEBUGGING_ASSERTIONS */
#undef DEFINE_GORY_STUFF
#include "m30.h"
#include "m62.h"

#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif	/* !UNLIKELY */
#if !defined UNUSED
# define UNUSED(_x)	__attribute__((unused)) _x
#endif	/* !UNUSED */

#define MAT_VERSION	(0x100)

struct mathdr_s {
	char desc[116];
	uint64_t subsys_data_off;
	uint16_t version;
	uint16_t endian;
};

#define miINT8		(1)
#define miUINT8		(2)
#define miINT16		(3)
#define miUINT16	(4)
#define miINT32		(5)
#define miUINT32	(6)
#define miSINGLE	(7)
#define miDOUBLE	(9)
#define miINT64		(12)
#define miUINT64	(13)
#define miMATRIX	(14)
#define miCOMPRESSED	(15)
#define miUTF8		(16)
#define miUTF16		(17)
#define miUTF32		(18)

#define mxCELL_CLASS	(1)
#define mxSTRUCT_CLASS	(2)
#define mxOBJECT_CLASS	(3)
#define mxCHAR_CLASS	(4)
#define mxSPARSE_CLASS	(5)
#define mxDOUBLE_CLASS	(6)
#define mxSINGLE_CLASS	(7)
#define mxINT8_CLASS	(8)
#define mxUINT8_CLASS	(9)
#define mxINT16_CLASS	(10)
#define mxUINT16_CLASS	(11)
#define mxINT32_CLASS	(12)
#define mxUINT32_CLASS	(13)
#define mxINT64_CLASS	(14)
#define mxUINT64_CLASS	(15)

struct matdel_s {
	uint32_t dty;
	uint32_t nby;
	char data[];
};

struct mat_s {
	struct mathdr_s hdr;
	char data[];
};


/* public demuxer */
ssize_t
pr(pr_ctx_t pctx, scom_t st)
{
	return 0;
}

/* mat.c ends here */
