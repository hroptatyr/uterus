/*** gbs.h -- generic bitsets, static
 *
 * Copyright (C) 2013 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@ga-group.nl>
 *
 * This file is part of truffle.
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
#if !defined INCLUDED_gbs_h_
#define INCLUDED_gbs_h_

#include <stddef.h>

#if !defined DECLF
# define DECLF		extern
#endif	/* !DECLF */
#if !defined DEFUN
# define DEFUN
#endif	/* !DECLF */

/* generic bitsets */
typedef struct gbs_s *gbs_t;

struct gbs_s {
	const size_t nbits;
	const void *bits;
};

DECLF void init_gbs(gbs_t, size_t ninit_members);
DECLF void fini_gbs(gbs_t);

/**
 * Set the BIT-th bit in GBS given. */
DECLF void gbs_set(gbs_t, unsigned int bit);

/**
 * Unset the BIT-th bit in GBS given. */
DECLF void gbs_unset(gbs_t, unsigned int bit);

/**
 * Return non-0 if BIT-th bit in GBS is set. */
DECLF int gbs_set_p(gbs_t, unsigned int bit);

/**
 * Shift GBS LSB-wards by X bits. */
DECLF void gbs_shift_lsb(gbs_t, unsigned int bit);

#endif	/* INCLUDED_gbs_h_ */
