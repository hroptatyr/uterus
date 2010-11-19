/*** pageio.h -- buffers for large block access
 *
 * Copyright (C) 2009 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <sebastian.freundt@ga-group.nl>
 *
 * This file is part of unserding.
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

#if !defined INCLUDED_pageio_h_
#define INCLUDED_pageio_h_

#include <stddef.h>
#include <stdbool.h>

#if !defined MAX_LINE_LEN
# define MAX_LINE_LEN		512
#endif	/* !MAX_LINE_LEN */
#define NET_BUF_LEN		1280
#define DISK_BUF_LEN		65536
#define NBUFFERS		512
#define PRDB_PGSZ		(DISK_BUF_LEN * NBUFFERS)

#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif	/* !UNLIKELY */
#if !defined UNUSED
# define UNUSED(_x)	__attribute__((unused)) _x
#endif	/* !UNUSED */

#if !defined STATIC_GUTS
# define FDECL		extern
# define FDEFU
#else  /* STATIC_GUTS */
# define FDECL		static
# define FDEFU		static
#endif	/* !STATIC_GUTS */

#if defined DEBUG_FLAG
# include <stdio.h>
# define PIO_DEBUG(args...)	fprintf(stderr, "[pageio] " args)
#else
# define PIO_DEBUG(args...)
#endif	/* DEBUG_FLAG */

typedef struct prdbuf_s *prdbuf_t;


FDECL prdbuf_t make_prdbuf(int fd);
FDECL void free_prdbuf(prdbuf_t prdb);

FDECL bool prdb_fetch_lines(prdbuf_t prdb);
FDECL void prdb_unfetch_lines(prdbuf_t prdb);
FDECL bool prdb_one_more_line_p(prdbuf_t prdb);
FDECL const char *prdb_current_line(prdbuf_t prdb);
FDECL void prdb_set_current_line_by_ptr(prdbuf_t prdb, const char *p);
FDECL void prdb_set_current_line_by_offs(prdbuf_t prdb, size_t len);

#if defined STATIC_GUTS
/* amazing innit? */
# include "pageio.c"
#endif	/* STATIC_GUTS */

#endif	/* INCLUDED_pageio_h_ */
