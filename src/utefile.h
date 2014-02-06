/*** utefile.h -- high level interface to ute files (r/w)
 *
 * Copyright (C) 2010-2013 Sebastian Freundt
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
#include <unistd.h>
#include <fcntl.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "scommon.h"
#include "utehdr.h"

#if !defined INCLUDED_utefile_private_h_
typedef void *utectx_t;
#endif	/* INCLUDED_utefile_private_h_ */


#if defined O_RDONLY
# define UO_RDONLY	(O_RDONLY)
#else
# define UO_RDONLY	(00)
#endif	/* O_RDONLY */
#if defined O_WRONLY
# define UO_WRONLY	(O_WRONLY)
#else
# define UO_WRONLY	(01)
#endif	/* O_WRONLY */
#if defined O_RDWR
# define UO_RDWR	(O_RDWR)
#else
# define UO_RDWR	(02)
#endif	/* O_RDWR */
#if defined O_CREAT
# define UO_CREAT	(O_CREAT)
#else
# define UO_CREAT	(0100)
#endif	/* O_CREAT */
#if defined O_TRUNC
# define UO_TRUNC	(O_TRUNC)
#else
# define UO_TRUNC	(01000)
#endif	/* O_TRUNC */
#if defined O_APPEND
# define UO_APPEND	(O_APPEND)
#else
# define UO_APPEND	(02000)
#endif	/* O_APPEND */
/* not in fcntl.h mehopes */
#define UO_ANON		(010)
#define UO_NO_HDR_CHK	(020)
#define UO_NO_LOAD_TPC	(040)
/* when used in conjunction with UO_CREAT no initial TPC will be created */
#define UO_NO_CREAT_TPC	(040)
/* open a file for streaming writes, i.e. the file is open for a long
 * time and written to occasionally allowing for it to be opened `live'
 * this coincides with O_SYNC */
#define UO_STREAM	(010000)

/**
 * Open the file in PATH and create a ute context.
 * Argument OFLAGS is like FLAGS in `open()':
 * UO_RDONLY  open the file read-only
 * UO_WRONLY  open the file write-only
 * UO_RDWR    open the file read-write
 * UO_CREAT   call creat(3) before opening the file
 * UO_TRUNC   truncate the file to 0 size */
extern utectx_t ute_open(const char *path, int oflags);

/**
 * Close the context, do final work and free associated resources.
 * This is in fact short for:
 * ute_flush(...);
 * ute_sort(...);
 * <flush_slut(...);>
 * ute_free(...); */
extern void ute_close(utectx_t);

/**
 * Free all associated resources immediatedly.
 * If changes are meant to end up on disk call `ute_close()'. */
extern void ute_free(utectx_t);

/**
 * Flush pending write operations. */
extern void ute_flush(utectx_t);

/**
 * Open a (new) temporary file in TMPDIR and create a ute context.
 * The file will be opened with UO_CREAT and OFLAGS will be ignored
 * except for
 * UO_ANON    open a file without a reference in the file system
 *            (anonymous inode) */
extern utectx_t ute_mktemp(int oflags);


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

extern scom_t ute_ffff2(utectx_t, sidx_t, unsigned int);

/**
 * Add the tick T to the ute context specified by CTX. */
extern void ute_add_tick(utectx_t ctx, scom_t t);

/**
 * Return the (total) number of ticks stored in CTX. */
extern size_t ute_nticks(utectx_t ctx);

/**
 * Convenience macro to traverse all ticks in a file CTX.
 * This version assumes that the number of ticks in CTX does not change
 * during the traversal.  The current tick is I. */
#define UTE_ITER(i, __ctx)						\
	for (sidx_t __i = 0, __tsz, __nt = ute_nticks(__ctx);		\
	     __tsz = 1/*just in case*/, __i < __nt;			\
	     __i += __tsz)						\
		for (scom_t i = ute_seek(__ctx, __i);			\
		     i/*will be NULL for illegal ticks*/;		\
		     __tsz = scom_tick_size(i), i = 0)

#define UTE_ITER_CUST(i, __tsz, __ctx)					\
	for (sidx_t __i = 0, __tsz, __nt = ute_nticks(__ctx);		\
	     __tsz = 1/*just in case*/, __i < __nt;			\
	     __i += __tsz)						\
		for (scom_t i = ute_seek(__ctx, __i);			\
		     i/*will be NULL for illegal ticks*/;		\
		     i = 0)

/**
 * A programmatic version of UTE_ITER. */
extern scom_t ute_iter(utectx_t hdl);

/**
 * Return the number of symbols tracked in CTX. */
extern size_t ute_nsyms(utectx_t ctx);

/**
 * Sort all ticks pages in the file. */
extern void ute_sort(utectx_t ctx);

/**
 * Clone the symbol look-up table (slut) from SRC to TGT. */
extern void ute_clone_slut(utectx_t tgt, utectx_t src);

/**
 * Wipe out CTX's slut leaving a clean empty version behind. */
extern void ute_empty_slut(utectx_t ctx);

#define SLUT_SYMLEN	(64)

/**
 * Given a symbol SYM return the index in the look-up table. */
extern unsigned int ute_sym2idx(utectx_t ctx, const char *sym);

/**
 * Given an index IDX return the symbol in the look-up table. */
extern const char *ute_idx2sym(utectx_t ctx, unsigned int idx);

/**
 * Associate SYM with index IDX in CTX's symbol look-up table.
 * Return the newly or previously associated index. */
extern unsigned int
ute_bang_symidx(utectx_t ctx, const char *sym, unsigned int idx);

/**
 * Return the file name (if any) associated with CTX. */
extern const char *ute_fn(utectx_t ctx);

/**
 * Set FN as the new file name for CTX.
 * Caution, this performs a rename() too. */
extern void ute_set_fn(utectx_t ctx, const char *fn);

/**
 * Return the ute version of the file associated with CTX. */
extern ute_ver_t ute_version(utectx_t ctx);

/**
 * Return the endianness used in the file associated with CTX. */
extern ute_end_t ute_endianness(utectx_t ctx);

/**
 * Check the endianness of CTX.  Return -1 if it doesn't match. */
extern int ute_check_endianness(utectx_t ctx);

/**
 * Set the endianness used in the file associated with CTX. */
extern void ute_set_endianness(utectx_t ctx, ute_end_t);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* INCLUDED_utefile_h_ */
