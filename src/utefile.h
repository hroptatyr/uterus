/*** utefile.h -- high level interface to ute files (r/w)
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
/**
 * CRUD API
 * - create: ute_touch()
 * - retrieve: ute_seek(), ute_tick(), ...
 * - update: nn
 * - delete: nn */

#if !defined INCLUDED_utefile_h_
#define INCLUDED_utefile_h_

#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "scommon.h"

#if !defined SIDX_T
typedef long int sidx_t;
# define SIDX_T
#endif	/* !SIDX_T */

#if !defined INCLUDED_utefile_private_h_
typedef void *utectx_t;
#endif	/* INCLUDED_utefile_private_h_ */


#define UO_RDONLY	(00)
#define UO_WRONLY	(01)
#define UO_RDWR		(02)
#define UO_CREAT	(0100)
#define UO_TRUNC	(01000)

/**
 * Open the file in PATH and create a ute context.
 * Argument OFLAGS is like FLAGS in `open()'. */
extern utectx_t ute_open(const char *path, int oflags);

/**
 * Close the context and free associated resources. */
extern void ute_close(utectx_t);

/**
 * Flush pending write operations. */
extern void ute_flush(utectx_t);


/* accessor */
/**
 * Return a pointer to the I-th tick in CTX. */
extern scom_t ute_seek(utectx_t ctx, sidx_t i);

/**
 * Obtain the I-th tick in ute file CTX, put its pointer into tgt and return the
 * tick size. */
extern size_t ute_tick(utectx_t ctx, scom_t *tgt, sidx_t i);
/**
 * Return a copy of the I-th tick in ute file CTX. */
extern size_t ute_tick2(utectx_t ctx, void *tgt, size_t tsz, sidx_t i);

extern scom_t ute_ffff2(utectx_t, sidx_t, uint16_t);

/**
 * Add the tick T to the ute context specified by CTX. */
extern void ute_add_tick(utectx_t ctx, scom_t t);

/**
 * Return the (total) number of ticks stored in CTX. */
extern size_t ute_nticks(utectx_t ctx);

/**
 * Return the number of symbols tracked in CTX. */
extern size_t ute_nsyms(utectx_t ctx);

/**
 * Sort all ticks pages in the file. */
extern void ute_sort(utectx_t ctx);

/**
 * Given a symbol SYM return the index in the look-up table. */
extern uint16_t ute_sym2idx(utectx_t ctx, const char *sym);

/**
 * Given an index IDX return the symbol in the look-up table. */
extern const char *ute_idx2sym(utectx_t ctx, uint16_t idx);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* INCLUDED_utefile_h_ */