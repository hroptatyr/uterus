/** trie.h -- trie data type and functions
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
 */

/*
 * trie.h - Trie data type and functions
 * Created: 2006-08-11
 * Author:  Theppitak Karoonboonyanan <thep@linux.thai.net>
 */

#ifndef __TRIE_H
#define __TRIE_H

#include "triedefs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file trie.h
 * @brief Trie data type and functions
 */

/**
 * @brief Trie data type
 */
typedef struct trie_s *trie_t;
typedef const struct trie_s *const_trie_t;

/**
 * @brief Trie enumeration function
 *
 * @param key  : the key of the entry
 * @param data : the data of the entry
 *
 * @return 0 to continue enumeration, -1 to stop
 */
typedef int(*trie_walk_f)(const char *key, trie_data_t val, void *closure);

/**
 * @brief Trie walking state
 */
typedef struct trie_state_s *trie_state_t;
typedef const struct trie_state_s *const_trie_state_t;

/* ctor/dtor */
trie_t make_trie(void);

trie_t trie_new_from_file(const char *path);

trie_t trie_mread(char *mem, size_t msz);

void trie_free(trie_t trie);

int trie_save(trie_t trie, const char *path);

int trie_mwrite(trie_t trie, char **mem, size_t *msz);

int trie_dirty_p(const_trie_t trie);

/* accessors */
int trie_retrieve(const_trie_t trie, const char *key, trie_data_t *o_data);
int trie_store(trie_t trie, const char *key, trie_data_t data);
int trie_store_if_absent(trie_t trie, const char *key, trie_data_t data);
int trie_delete(trie_t trie, const char *key);
int trie_walk(const_trie_t trie, trie_walk_f walkf, void *closure);

/* introspection and state */
trie_state_t trie_root(const_trie_t trie);
trie_state_t trie_state_clone(const_trie_state_t s);
void trie_state_copy(trie_state_t dst, const_trie_state_t src);
void trie_state_free(trie_state_t s);
void trie_state_rewind(trie_state_t s);
int trie_state_walk(trie_state_t s, char c);
int trie_state_walkable_p(const_trie_state_t s, char c);

/**
 * @brief Check for terminal state
 *
 * @param s    : the state to check
 *
 * @return boolean value indicating whether it is a terminal state
 *
 * Check if the given state is a terminal state. A terminal state is a trie
 * state that terminates a key, and stores a value associated with it.
 */
#define trie_state_terminal_p(s)			\
	trie_state_walkable_p((s), TRIE_CHAR_TERM)
int trie_state_single_p(const_trie_state_t s);

/**
 * @brief Check for leaf state
 *
 * @param s    : the state to check
 *
 * @return boolean value indicating whether it is a leaf state
 *
 * Check if the given state is a leaf state. A leaf state is a terminal state 
 * that has no other branch.
 */
#define trie_state_leaf_p(s)					\
	(trie_state_single_p(s) && trie_state_terminal_p(s))

trie_data_t trie_state_get_data(const_trie_state_t s);

#ifdef __cplusplus
}
#endif

#endif  /* __TRIE_H */
