/*** uteslut-trie-glue.h -- glue ute and various trie implementations
 *
 * Copyright (C) 2010-2012 Sebastian Freundt
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
 **
 * Comments:
 * This glues various lookup data structures to uteslut.
 * This header is for internal use only.
 ***/

#if !defined INCLUDED_uteslut_trie_glue_h_
#define INCLUDED_uteslut_trie_glue_h_

#include <stdint.h>
#include "trie.h"

/* assumes we have a void* alias __slut_t */

static inline __slut_t
make_slut_tg(void)
{
	return make_trie();
}

static inline void
free_slut_tg(__slut_t t)
{
	free_trie(t);
	return;
}

/* should be uint32_t put(__slut_t, const char*) */
static inline void
slut_tg_put(__slut_t t, const char *sym, uint32_t data)
{
	trie_store(t, sym, data);
	return;
}

static inline int
slut_tg_get(__slut_t t, const char *sym, uint32_t *data)
{
	return trie_retrieve(t, sym, (int32_t*)data);
}

static inline __slut_t
slut_tg_deser(void *data, size_t dlen)
{
	return trie_mread(data, dlen);
}

static inline void
slut_tg_seria(__slut_t t, void **data, size_t *dlen)
{
	trie_mwrite(t, (void*)data, dlen);
	return;
}

typedef int(*slut_tg_walk_f)(const char *key, uint32_t val, void *clo);

static inline void
slut_tg_walk(__slut_t t, slut_tg_walk_f cb, void *closure)
{
	trie_walk(t, (trie_walk_f)cb, closure);
	return;
}

#endif	/* INCLUDED_trie_glue_h_ */
