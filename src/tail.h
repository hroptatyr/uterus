/** tail.h -- trie tail for keeping suffixes
 *
 * libdatrie - Double-Array Trie Library
 * Copyright (C) 2006  Theppitak Karoonboonyanan <thep@linux.thai.net>
 * Copyright (C) 2010-2012  Sebastian Freundt  <hroptatyr@unserding.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*
 * Created: 2006-08-12
 * Author:  Theppitak Karoonboonyanan <thep@linux.thai.net>
 */

#ifndef __TAIL_H
#define __TAIL_H

#include <stdbool.h>
#include "triedefs.h"
#include "fmcmb.h"

#if !defined DECLF
# define DECLF		extern
#endif	/* !DECLF */

/**
 * @file tail.h
 * @brief trie tail for keeping suffixes
 */

/**
 * @brief Double-array structure type
 */
typedef struct tail_s *tail_t;
typedef const struct tail_s *const_tail_t;

/* ctor/dtor */
DECLF tail_t make_tail(void);
DECLF void free_tail(tail_t t);

/**
 * Clone SRC and return a deep-copy. */
DECLF tail_t clone_tail(const_tail_t src);

DECLF tail_t tail_fmread(fmcmb_t stream);
DECLF int tail_fmwrite(const_tail_t t, fmcmb_t stream);

DECLF const char *tail_get_suffix(const_tail_t t, trie_idx_t index);
DECLF int tail_set_suffix(tail_t t, trie_idx_t index, const char *suffix);

DECLF trie_idx_t tail_add_suffix(tail_t t, const char *suffix);

DECLF trie_data_t tail_get_data(const_tail_t t, trie_idx_t index);

DECLF int tail_set_data(tail_t t, trie_idx_t index, trie_data_t data);
DECLF void tail_delete(tail_t t, trie_idx_t index);

DECLF int
tail_walk_str(
	const_tail_t t, trie_idx_t s,
	short int*suffix_idx, const char *str, int len);

DECLF int
tail_walk_char(const_tail_t t, trie_idx_t s, short int *suffix_idx, char c);

/**
 * @brief Test walkability in tail with a character
 *
 * @param t          : the tail data
 * @param s          : the tail data index
 * @param suffix_idx : current character index in suffix
 * @param c          : the character to test walkability
 *
 * @return boolean indicating walkability
 *
 * Test if the character @a c can be used to walk from given character 
 * position @a suffix_idx of entry @a s of the tail data @a t.
 */
static inline bool
tail_walkable_char_p(tail_t t, trie_idx_t s, short int suffix_idx, char c)
{
	const char *sfx = tail_get_suffix(t, s);
	return sfx[suffix_idx] == c;
}

#endif  /* __TAIL_H */
