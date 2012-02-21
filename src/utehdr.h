/*** utehdr.h -- header handling for ute files
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

#if !defined INCLUDED_utehdr_h_
#define INCLUDED_utehdr_h_

#include <stdint.h>
#include <stdbool.h>
#include "secu.h"

/**
 * Maximum size of an ute header in file mode. */
#define UTEHDR_MAX_SIZE	(4096)
#define UTEHDR_MAX_SECS	(UTEHDR_MAX_SIZE / sizeof(su_secu_t))
#define UTEHDR_MAX_SYMS	(UTEHDR_MAX_SECS / 2)
#define UTEHDR_SYMLEN	(UTEHDR_MAX_SIZE / UTEHDR_MAX_SYMS)

typedef const struct utehdr_s *utehdr_t;
typedef const struct utehdr2_s *utehdr2_t;

/* SoA */
struct utehdr_s {
	union {
		struct {
			char magic[4];
			char version[4];
			uint32_t flags;
			uint32_t ploff;
		};
#define UTEHDR_FIRST_SECIDX	2
		su_secu_t sec[UTEHDR_MAX_SECS];
#define UTEHDR_FIRST_SYMIDX	1
		char sym[UTEHDR_MAX_SYMS][UTEHDR_SYMLEN];
	};
};

#define UTEHDR_FLAG_SYMTBL	1
#define UTEHDR_FLAG_ORDERED	2
#define UTEHDR_FLAG_TRNGTBL	4

struct utehdr2_s {
	char magic[4];
	char version[4];
	uint32_t flags;
	uint32_t ploff;
	/* slut info, off:16 len:8  */
	uint32_t slut_sz;
	uint16_t slut_nsyms;
	uint16_t slut_version;
	/* bollocks, off:24, len:8 */
	uint32_t dummy[2];
	char pad[4096 - 32];
};


/* file header predicates
 * we deliberately provide no setters here as most of the time
 * the header is in kernel space */
static inline size_t
utehdr_payload_offset(utehdr_t fhdr)
{
	return fhdr->ploff;
}

static inline bool
utehdr_symtbl_p(utehdr_t fhdr)
{
	return (fhdr->flags & UTEHDR_FLAG_SYMTBL) != 0;
}

static inline void
utehdr_set_symtbl(struct utehdr_s *fhdr)
{
	fhdr->flags |= UTEHDR_FLAG_SYMTBL;
	return;
}

static inline void
utehdr_unset_symtbl(struct utehdr_s *fhdr)
{
	fhdr->flags &= ~UTEHDR_FLAG_SYMTBL;
	return;
}

static inline bool
utehdr_ordered_p(utehdr_t fhdr)
{
	return (fhdr->flags & UTEHDR_FLAG_ORDERED) != 0;
}

static inline void
utehdr_set_ordered(struct utehdr_s *fhdr)
{
	fhdr->flags |= UTEHDR_FLAG_ORDERED;
	return;
}

static inline void
utehdr_unset_ordered(struct utehdr_s *fhdr)
{
	fhdr->flags &= ~UTEHDR_FLAG_ORDERED;
	return;
}

static inline bool
utehdr_trngtbl_p(utehdr_t fhdr)
{
	return (fhdr->flags & UTEHDR_FLAG_TRNGTBL) != 0;
}

static inline void
utehdr_set_trngtbl(struct utehdr_s *fhdr)
{
	fhdr->flags |= UTEHDR_FLAG_TRNGTBL;
	return;
}

static inline void
utehdr_unset_trngtbl(struct utehdr_s *fhdr)
{
	fhdr->flags &= ~UTEHDR_FLAG_TRNGTBL;
	return;
}

#endif	/* INCLUDED_utehdr_h_ */
