/*** utehdr.h -- header handling for ute files
 *
 * Copyright (C) 2009-2012 Sebastian Freundt
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

#if !defined INCLUDED_utehdr_h_
#define INCLUDED_utehdr_h_

#include <stdint.h>
#include <stdbool.h>

typedef const struct utehdr2_s *utehdr2_t;

#define UTEHDR_FLAG_SYMTBL	1
#define UTEHDR_FLAG_ORDERED	2
#define UTEHDR_FLAG_TRNGTBL	4

struct utehdr2_s {
	char magic[4];
	char version[4];
	/* endianness indicator, should be 0x9c35 on be and 359c on le */
	union {
		uint16_t endin;
		char endia[sizeof(uint16_t) / sizeof(char)];
	};
	uint16_t flags;
	uint32_t ploff;
	/* slut info, off:16 len:8  */
	uint32_t slut_sz;
	uint16_t slut_nsyms;
	uint16_t slut_version;
	/* bollocks, off:24, len:8 */
	uint32_t dummy[2];
	char pad[4096 - 32];
};

typedef enum {
	UTE_VERSION_UNK,
	UTE_VERSION_01,
	UTE_VERSION_02,
} ute_ver_t;

typedef enum {
	UTE_ENDIAN_UNK,
	UTE_ENDIAN_LITTLE,
	UTE_ENDIAN_BIG,
} ute_end_t;


/* public api */
extern ute_ver_t utehdr_version(utehdr2_t);

/**
 * Return the endianness used in the file that's headed by HDR. */
extern ute_end_t utehdr_endianness(utehdr2_t hdr);

/**
 * Set the endianness used in the file that's headed by HDR. */
extern void utehdr_set_endianness(struct utehdr2_s *hdr, ute_end_t);

/**
 * Check the magic string in HDR.  Return -1 if the magic string is broken. */
extern int utehdr_check_magic(utehdr2_t hdr);

/**
 * Check the endianness of HDR.  Return -1 if it doesn't match. */
extern int utehdr_check_endianness(utehdr2_t hdr);

#endif	/* INCLUDED_utehdr_h_ */
