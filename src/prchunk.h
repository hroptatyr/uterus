/*** prchunk.h -- guessing line oriented data formats
 *
 * Copyright (C) 2010 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <sebastian.freundt@ga-group.nl>
 *
 * This file is part of glod.
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

#if !defined INCLUDED_prchunk_h_
#define INCLUDED_prchunk_h_

#if !defined STATIC_GUTS
# define FDECL		extern
# define FDEFU
#else  /* STATIC_GUTS */
# define FDECL		static
# define FDEFU		static
#endif	/* !STATIC_GUTS */

typedef struct prch_ctx_s *prch_ctx_t;

/* non-reentrant! */
FDECL void init_prchunk(int fd);
FDECL void free_prchunk(void);

FDECL int prchunk_fill(void);

FDECL size_t prchunk_get_nlines(void);
FDECL size_t prchunk_get_ncols(void);

FDECL size_t prchunk_getlineno(char **p, int lno);
FDECL size_t prchunk_getline(char **p);
FDECL void prchunk_reset(void);
FDECL int prchunk_haslinep(void);

FDECL void prchunk_rechunk(char delim, int ncols);
FDECL size_t prchunk_getcolno(char **p, int lno, int cno);

#endif	/* INCLUDED_prchunk_h_ */
