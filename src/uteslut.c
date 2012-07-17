/*** uteslut.c -- ute symbol look-up table
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

#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdint.h>
#include <sys/mman.h>
#include <string.h>
#include "nifty.h"
#include "mem.h"
#include "uteslut.h"
/* symbol table stuff */
#include "uteslut-trie-glue.h"

#include "utefile.h"

/* generic symbol, this is non-normative at the moment
 * actual length is defined in utefile.h */
typedef char slut_sym_t[SLUT_SYMLEN];


DEFUN void
init_slut(void)
{
	return;
}

DEFUN void
fini_slut(void)
{
	return;
}

/* ctor, dtor */
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
resize_i2s(uteslut_t s, size_t at_least)
{
	size_t old, new;

	/* alloc stepping is 128, 1024, 8192, 65536 */
	if (at_least >= 8192) {
		at_least = 65536;
	} else if (at_least >= 1024) {
		at_least = 8192;
	} else if (at_least >= 128) {
		at_least = 1024;
	} else {
		at_least = 128;
	}
	old = s->alloc_sz * sizeof(slut_sym_t);
	new = at_least * sizeof(slut_sym_t);
	s->itbl = mremap(s->itbl, old, new, MREMAP_MAYMOVE);
	s->alloc_sz = at_least;
	return;
}

static void
clone_i2s(uteslut_t tgt, uteslut_t src)
{
	const size_t clonsz = (src->nsyms + 1) * sizeof(slut_sym_t);
	init_i2s(tgt, tgt->alloc_sz = src->alloc_sz);
	memcpy(tgt->itbl, src->itbl, clonsz);
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
	s->stbl = make_slut_tg();
	return;
}

DEFUN void
free_slut(uteslut_t s)
{
	/* s2i */
	if (s->stbl != NULL) {
		free_slut_tg(s->stbl);
		s->stbl = NULL;
	}
	/* i2s */
	if (s->itbl != NULL) {
		munmap(s->itbl, s->alloc_sz * sizeof(slut_sym_t));
		s->itbl = NULL;
	}
	return;
}

DEFUN void
clone_slut(uteslut_t tgt, uteslut_t src)
{
	/* clone the s2i trie */
	tgt->stbl = clone_slut_tg(src->stbl);
	/* clone the i2s table */
	clone_i2s(tgt, src);
	/* and make sure we talk the same number of symbols */
	tgt->nsyms = src->nsyms;
	return;
}

static uint32_t
__crea(uteslut_t s, const char *sym)
{
	uint32_t res = ++s->nsyms;
	slut_sym_t *itbl;

	/* check for a resize */
	if (res >= s->alloc_sz) {
		resize_i2s(s, res);
	}

	/* store in the s2i table (trie) */
	slut_tg_put(s->stbl, sym, res);
	/* store in the i2s table */
	itbl = s->itbl;
	strcpy(itbl[res], sym);
	return res;
}

DEFUN uint16_t
slut_sym2idx(uteslut_t s, const char *sym)
{
	uint32_t data[1];
	uint16_t res;

	/* make an alpha char array first */
	if (slut_tg_get(s->stbl, sym, data) < 0) {
		/* create a new entry */
		res = (uint16_t)__crea(s, sym);
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

DEFUN uint16_t
slut_bang(uteslut_t s, const char *sym, uint16_t idx)
{
	uint32_t data;

	if (slut_tg_get(s->stbl, sym, &data) < 0) {
		/* check for a resize */
		if ((data = idx) > s->nsyms) {
			s->nsyms = data;
		}
		if (data >= s->alloc_sz) {
			resize_i2s(s, data);
		}

		/* store in the s2i table (trie) */
		slut_tg_put(s->stbl, sym, data);
		/* store in the i2s table */
		{
			slut_sym_t *itbl = s->itbl;
			strcpy(itbl[data], sym);
		}
		return idx;
	}
	/* otherwise just return what we've got */
	return (uint16_t)data;
}


/* (de)serialiser */
static int
tri_cb(const char *sym, uint32_t val, void *clo)
{
	uteslut_t s = clo;
	uint32_t slot = (uint32_t)val;

	/* check if we need a resize */
	if (slot >= s->alloc_sz) {
		resize_i2s(s, slot);
	}

	for (char *tgt = ((slut_sym_t*)s->itbl)[slot]; *sym; tgt++, sym++) {
		*tgt = (char)(*sym);
	}
	if (s->nsyms < val) {
		s->nsyms = val;
	}
	return 0;
}

DEFUN void
slut_deser(uteslut_t s, void *data, size_t size)
{
	if (LIKELY(size)) {
		/* init the s2i trie */
		s->stbl = slut_tg_deser(data, size);
	} else {
		s->stbl = make_slut_tg();
	}
	/* init the i2s guy */
	init_i2s(s, 128);
	/* traverse the trie and add them symbols */
	slut_tg_walk(s->stbl, tri_cb, s);
	return;
}

DEFUN void
slut_seria(uteslut_t s, void **data, size_t *size)
{
	slut_tg_seria(s->stbl, data, size);
	return;
}

/* uteslut.c ends here */
