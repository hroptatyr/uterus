/*** utehdr.c -- header handling for ute files
 *
 * Copyright (C) 2009-2015 Sebastian Freundt
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
#include <stdbool.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>
#include "utehdr.h"
#include "utefile-private.h"

#if !defined countof
# define countof(x)	(sizeof(x) / sizeof(*x))
#endif	/* countof */

/* printed representations of the UTE_VERSION */
static const size_t magic_len = 4;
static const char ute_vers[][8] = {
	"UTE+v0.0",
	"UTE+v0.1",
	"UTE+v0.2",
};

/* endian-ness indicator <> on big-E machines, >< on little-Es */
static const uint16_t endian_indicator = 0x3c3e;

void
bump_header(struct utehdr2_s *hdr)
{
	const char *ver = ute_vers[UTE_VERSION_02];
	const size_t vsz = sizeof(ute_vers[UTE_VERSION_02]);
	memcpy(hdr, ver, vsz);
	hdr->endin = endian_indicator;
	return;
}

ute_ver_t
utehdr_version(utehdr2_t hdr)
{
	const size_t vsz = sizeof(ute_vers[0]);
	for (size_t i = countof(ute_vers); --i > 0; ) {
		const char *ver = ute_vers[i];
		if (memcmp(hdr->magic, ver, vsz) == 0) {
			return (ute_ver_t)(i);
		}
	}
	return UTE_VERSION_UNK;
}

ute_end_t
utehdr_endianness(utehdr2_t hdr)
{
	if (hdr->endia[0] == '>' && hdr->endia[1] == '<') {
		return UTE_ENDIAN_LITTLE;
	} else if (hdr->endia[0] == '<' && hdr->endia[1] == '>') {
		return UTE_ENDIAN_BIG;
	}
	return UTE_ENDIAN_UNK;
}

void
utehdr_set_endianness(struct utehdr2_s *hdr, ute_end_t en)
{
	switch (en) {
	case UTE_ENDIAN_BIG:
		hdr->endia[0] = '<';
		hdr->endia[1] = '>';
		break;
	case UTE_ENDIAN_LITTLE:
		hdr->endia[0] = '>';
		hdr->endia[1] = '<';
		break;
	case UTE_ENDIAN_UNK:
	default:
		/* do fuckall */
		break;
	}
	return;
}

int
utehdr_check_magic(utehdr2_t hdr)
{
	return !memcmp(hdr->magic, *ute_vers, magic_len) ? 0 : -1;
}

int
utehdr_check_endianness(utehdr2_t hdr)
{
#if !defined WORDS_BIGENDIAN
	/* for a moment we accept files without endianness indicator too
	 * only when we're on a little-E box though */
	if (UNLIKELY(!hdr->endin)) {
		return 0;
	}
#endif	/* WORDS_BIGENDIAN */
	return hdr->endin == endian_indicator ? 0 : -1;
}

/* utehdr.c ends here */
