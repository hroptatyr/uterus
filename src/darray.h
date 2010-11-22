/** darray.h -- double array trie structure
 *
 * libdatrie - Double-Array Trie Library
 * Copyright (C) 2006  Theppitak Karoonboonyanan <thep@linux.thai.net>
 * Copyright (C) 2010  Sebastian Freundt  <hroptatyr@unserding.org>
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
 *
 **/

/*
 * Created: 2006-08-11
 * Author:  Theppitak Karoonboonyanan <thep@linux.thai.net>
 */

#ifndef __DARRAY_H
#define __DARRAY_H

#include "triedefs.h"
#include "fmcmb.h"

/**
 * @file darray.h
 * @brief Double-array trie structure
 */

/**
 * @brief Double-array structure type
 */
typedef struct darray_s *darray_t;
typedef const struct darray_s *const_darray_t;

/**
 * @brief Double-array entry enumeration function
 *
 * @param key: the key of the entry, up to @a sep_node
 * @param sep_node: the separate node of the entry
 * @param clo: user-supplied data
 *
 * @return 0 to continue enumeration, -1 to stop
 */
typedef int(*darray_walk_f)(const char *key, trie_idx_t sep_node, void *clo);

/* ctor/dtor */
extern darray_t make_darray(void);
extern void free_darray(darray_t d);

extern darray_t darray_fmread(fmcmb_t filemem);
extern int darray_fmwrite(const_darray_t d, fmcmb_t filemem);

trie_idx_t da_get_root(const_darray_t d);
trie_idx_t da_get_base(const_darray_t d, trie_idx_t s);
trie_idx_t da_get_check(const_darray_t d, trie_idx_t s);

void da_set_base(darray_t d, trie_idx_t s, trie_idx_t val);
void da_set_check(darray_t d, trie_idx_t s, trie_idx_t val);
int da_walk(const_darray_t d, trie_idx_t s, char c);

/**
 * @brief Test walkability in double-array structure
 *
 * @param d : the double-array structure
 * @param s : current state
 * @param c : the input character
 *
 * @return boolean indicating walkability
 *
 * Test if there is a transition from state @a s with input character @a c.
 */
/*
  Bool       da_is_walkable (DArray *d, TrieIndex s, TrieChar c);
*/
#define da_walkable_p(d, s, c)					\
	(da_get_check((d), da_get_base((d), (s)) + (c)) == (s))

trie_idx_t da_insert_branch(darray_t d, trie_idx_t s, char c);
void da_prune(darray_t d, trie_idx_t s);
void da_prune_upto(darray_t d, trie_idx_t p, trie_idx_t s);
extern int darray_walk(const_darray_t d, darray_walk_f walkf, void *closure);

#endif  /* __DARRAY_H */
