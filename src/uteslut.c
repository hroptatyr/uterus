/*** uteslut.c -- ute symbol look-up table
 *
 * Copyright (C) 2009 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <sebastian.freundt@ga-group.nl>
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

#include <stdint.h>
#include <sys/mman.h>
#include "uteslut.h"
/* symbol table stuff */
#include <datrie/trie.h>

#if !defined UNUSED
# define UNUSED(_x)	__attribute__((unused)) _x
#endif	/* !UNUSED */
#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif	/* !LIKELY */
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif	/* !LIKELY */

static AlphaMap *__glob_am = NULL;

DEFUN void
init_slut(void)
{
	if (LIKELY(__glob_am == NULL)) {
		__glob_am = alpha_map_new();
		alpha_map_add_range(__glob_am, ' ', 'z');
	}
	return;
}

DEFUN void
fini_slut(void)
{
	if (LIKELY(__glob_am != NULL)) {
		alpha_map_free(__glob_am);
		__glob_am = NULL;
	}
	return;
}

/* ctor, dtor */
#define MAP_MEM		(MAP_PRIVATE | MAP_ANONYMOUS)
#define PROT_MEM	(PROT_READ | PROT_WRITE)

static void
init_i2s(uteslut_t s, size_t initial_alloc_sz)
{
	/* initialise the i2s table */
	size_t itsz = (s->alloc_sz = initial_alloc_sz) * sizeof(slut_sym_t);

	s->nsyms = 0;
	/* just alloc some syms now
	 * alloc stepping is 128, 1024, 8192, 65536 */
	s->itbl = mmap(s->itbl, itsz, PROT_MEM, MAP_MEM, 0, 0);
	return;
}

static void
resize_i2s(uteslut_t s)
{
	size_t old, new;

	/* alloc stepping is 128, 1024, 8192, 65536 */
	old = s->alloc_sz * sizeof(slut_sym_t);
	new = old * 8;
	s->alloc_sz *= 8;
	s->itbl = mremap(s->itbl, old, new, MREMAP_MAYMOVE);
	return;
}

DEFUN void
make_slut(uteslut_t s)
{
	/* singleton */
	init_slut();

	/* initialise the i2s table */
	init_i2s(s, 128);

	/* init the s2i trie */
	s->stbl = trie_new(__glob_am);
	return;
}

DEFUN void
free_slut(uteslut_t s)
{
	/* s2i */
	trie_free(s->stbl);
	s->stbl = NULL;
	/* i2s */
	munmap(s->itbl, s->alloc_sz * sizeof(slut_sym_t));
	s->itbl = NULL;
	return;
}

static uint32_t
__crea(uteslut_t s, const char *sym, const AlphaChar *ac_sym)
{
	uint32_t res = ++s->nsyms;
	slut_sym_t *itbl;

	/* check for a resize */
	if (res >= s->alloc_sz) {
		resize_i2s(s);
	}

	/* store in the s2i table (trie) */
	trie_store(s->stbl, ac_sym, res);
	/* store in the i2s table */
	itbl = s->itbl;
	for (char *p = itbl[res]; *sym; p++, sym++) {
		*p = *sym;
	}
	return res;
}

static inline void
sym_to_ac(AlphaChar *ac, const char *sym)
{
	AlphaChar *acp, *eacp = ac + sizeof(slut_sym_t);
	const char *p;

	for (p = sym, acp = ac; *p && acp < eacp; p++, acp++) {
		*acp = (int32_t)(*p);
	}
	/* fill the rest with naughts */
	for (; acp < eacp; acp++) {
		*acp = 0;
	}
	return;
}

DEFUN uint16_t
slut_sym2idx(uteslut_t s, const char *sym)
{
	TrieData data[1];
	AlphaChar ac_sym[sizeof(slut_sym_t)];
	uint16_t res;

	/* make an alpha char array first */
	sym_to_ac(ac_sym, sym);
	if (!trie_retrieve(s->stbl, ac_sym, data)) {
		/* create a new entry */
		res = (uint16_t)__crea(s, sym, ac_sym);
	} else {
		res = (uint16_t)(int32_t)data[0];
	}
	return res;
}

DEFUN const char*
slut_idx2sym(uteslut_t s, uint16_t idx)
{
	slut_sym_t *itbl = s->itbl;
	if (UNLIKELY(idx == 0 || idx > s->nsyms)) {
		return NULL;
	}
	return itbl[idx];
}


/* (de)serialiser */
static Bool
tri_cb(const AlphaChar *sym, TrieData val, void *clo)
{
	uteslut_t s = clo;
	uint32_t slot = (uint32_t)val;

	/* check if we need a resize */
	while (slot >= s->alloc_sz) {
		resize_i2s(s);
	}

	for (char *tgt = ((slut_sym_t*)s->itbl)[slot]; *sym; tgt++, sym++) {
		*tgt = (char)(*sym);
	}
	s->nsyms++;
	return TRUE;
}

DEFUN void
slut_deser(uteslut_t s, void *data, size_t size)
{
	/* init the s2i trie */
	s->stbl = trie_mread(data, size);
	/* init the i2s guy */
	init_i2s(s, 128);
	/* traverse the trie and add them symbols */
	trie_enumerate(s->stbl, tri_cb, s);
	return;
}

DEFUN void
slut_seria(uteslut_t s, void **data, size_t *size)
{
	trie_mwrite(s->stbl, (void*)data, size);
	return;
}

/* uteslut.c ends here */
