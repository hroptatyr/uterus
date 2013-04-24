/*** gbs.c -- generic bitsets, static
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
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */

#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include "gbs.h"

#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif	/* !LIKELY */
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif	/* !UNLIKELY */
#if !defined UNUSED
# define UNUSED(_x)	_x __attribute__((unused))
#endif	/* !UNUSED */

#if !defined MAP_ANON && defined MAP_ANONYMOUS
# define MAP_ANON	MAP_ANONYMOUS
#elif defined MAP_ANON
/* all's good */
#else  /* !MAP_ANON && !MAP_ANONYMOUS */
# define MAP_ANON	(0U)
#endif	/* !MAP_ANON && MAP_ANONYMOUS */

#if !defined MAP_MEM
# define MAP_MEM	(MAP_ANON | MAP_PRIVATE)
#endif	/* !MAP_MEM */
#if !defined PROT_MEM
# define PROT_MEM	(PROT_READ | PROT_WRITE)
#endif	/* !PROT_MEM */

/* private view on things */
struct __gbs_s {
	size_t nbits;
	void *bits;
};


static inline __attribute__((const, pure)) size_t
nmemb_to_nbytes(size_t nmemb)
{
	return (nmemb - 1U) / 8U + 1U;
}

static inline __attribute__((const, pure)) size_t
nbytes_to_nmemb(size_t nbytes)
{
	return nbytes * 8U;
}


/* public API */
DEFUN void
init_gbs(gbs_t bs, size_t ninit_members)
{
	struct __gbs_s *p = (void*)bs;
	size_t mpsz = nmemb_to_nbytes(ninit_members);

	if (UNLIKELY(bs->bits != NULL)) {
		fini_gbs(bs);
	}
	p->nbits = nbytes_to_nmemb(mpsz);
	p->bits = mmap(NULL, mpsz, PROT_MEM, MAP_MEM, -1, 0);
	return;
}

DEFUN void
fini_gbs(gbs_t bs)
{
	if (LIKELY(bs->bits != NULL)) {
		struct __gbs_s *p = (void*)bs;
		size_t mpsz = nmemb_to_nbytes(p->nbits);

		munmap(p->bits, mpsz);
		/* reset the slots */
		p->nbits = 0UL;
		p->bits = NULL;
	}
	return;
}

static void
resz_gbs(gbs_t bs, size_t nnu_members)
{
	struct __gbs_s *p = (void*)bs;
	size_t olsz = nmemb_to_nbytes(p->nbits);
	size_t mpsz = nmemb_to_nbytes(nnu_members);

	if (olsz >= mpsz) {
		return;
	}
	/* otherwise there's really work to do */

#if defined MREMAP_MAYMOVE
	p->bits = mremap(p->bits, olsz, mpsz, MREMAP_MAYMOVE);
#else  /* !MREMAP_MAYMOVE */
	{
		void *nu = mmap(NULL, mpsz, PROT_MEM, MAP_MEM, -1, 0);
		memcpy(nu, p->bits, olsz);
	}
#endif	/* MREMAP_MAYMOVE */
	p->nbits = nbytes_to_nmemb(mpsz);
	return;
}

DEFUN void
gbs_set(gbs_t bs, unsigned int bit)
{
	unsigned int by = bit / 8U;
	unsigned int bb = bit % 8U;
	struct __gbs_s *p = (void*)bs;
	uint8_t *bits;

	resz_gbs(bs, bit + 1U);
	bits = p->bits;
	bits[by] |= (uint8_t)(1U << bb);
	return;
}

DEFUN void
gbs_unset(gbs_t bs, unsigned int bit)
{
	unsigned int by = bit / 8U;
	unsigned int bb = bit % 8U;
	struct __gbs_s *p = (void*)bs;
	uint8_t *bits;

	resz_gbs(bs, bit + 1U);
	bits = p->bits;
	bits[by] &= (uint8_t)(~(1U << bb));
	return;
}

DEFUN int
gbs_set_p(gbs_t bs, unsigned int bit)
{
	unsigned int by = bit / 8U;
	unsigned int bb = bit % 8U;
	struct __gbs_s *p = (void*)bs;
	uint8_t *bits;

	resz_gbs(bs, bit + 1U);
	bits = p->bits;
	return bits[by] & (1U << bb);
}

DEFUN void
gbs_shift_lsb(gbs_t bs, unsigned int bit)
{
	unsigned int by = bit / 8U;
	unsigned int bb = bit % 8U;
	struct __gbs_s *p = (void*)bs;
	uint8_t *bits = p->bits;
	unsigned int nby = bs->nbits / 8U + 1U;
	unsigned int rby;
	unsigned int rbb;
	unsigned int bbmsk;

	if (UNLIKELY(bit >= bs->nbits)) {
		memset(bits, 0, nby);
		return;
	}

	/* change the meaning of nby */
	rby = nby - by;
	rbb = 8 - bb;
	bbmsk = (1U << bb) - 1U;
	/* otherwise we start at 8BY and shift byte-wise first, then go
	 * bitwise for BB bits, i.e.
	 * 9876 54321098 76543210  by 11
	 * 9876 54321098
	 * 9 87654321 */
	memmove(bits, bits + by, rby);
	memset(bits + rby, 0, by);
	/* now bitwise */
	for (size_t i = 0; i < rby; i++) {
		bits[i] >>= bb;
		bits[i] |= (uint8_t)((bits[i + 1] & bbmsk) << rbb);
	}
	return;
}

/* gbs.c ends here */
