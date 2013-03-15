/** darray.c - Double-array trie structure
 *
 * libdatrie - Double-Array Trie Library
 * Copyright (C) 2006  Theppitak Karoonboonyanan <thep@linux.thai.net>
 * Copyright (C) 2010-2013  Sebastian Freundt  <hroptatyr@unserding.org>
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
 * Created: 2006-08-13
 * Author:  Theppitak Karoonboonyanan <thep@linux.thai.net>
 */

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "darray.h"
#include "fileutils.h"

#if !defined DEFUN
# define DEFUN
#endif	/* !DEFUN */

/**
 * @brief Minimum value macro
 */
#define MIN_VAL(a,b)	((a)<(b)?(a):(b))
/**
 * @brief Maximum value macro
 */
#define MAX_VAL(a,b)	((a)>(b)?(a):(b))


/*----------------------------------*
 *    INTERNAL TYPES DECLARATIONS   *
 *----------------------------------*/

typedef struct __symbols_s *symbols_t;
typedef const struct __symbols_s *const_symbols_t;

struct __symbols_s {
	short int num_symbols;
	char symbols[256];
};

static symbols_t make_symbols(void);
static void free_symbols(symbols_t syms);
static void symbols_add(symbols_t syms, char c);

#define symbols_num(s)		((s)->num_symbols)
#define symbols_get(s,i)	((s)->symbols[i])
#define symbols_add_fast(s,c)	((s)->symbols[(s)->num_symbols++] = c)

/*-----------------------------------*
 *    PRIVATE METHODS DECLARATIONS   *
 *-----------------------------------*/

#define da_get_free_list(d)	(1)

static bool da_check_free_cell(darray_t d, trie_idx_t s);
static bool da_has_children(darray_t d, trie_idx_t s);

static symbols_t da_output_symbols(const_darray_t d, trie_idx_t s);
static char *da_get_state_key(const_darray_t d, trie_idx_t state);
static trie_idx_t da_find_free_base(darray_t d, const_symbols_t  symbols);
static int da_fit_symbols(darray_t d, trie_idx_t base, const_symbols_t symbols);
static void da_relocate_base(darray_t d, trie_idx_t s, trie_idx_t new_base);
static int da_extend_pool(darray_t d, trie_idx_t to_index);
static void da_alloc_cell(darray_t d, trie_idx_t cell);
static void da_free_cell(darray_t d, trie_idx_t cell);
static int
da_walk_recursive(const_darray_t d, trie_idx_t st, darray_walk_f w, void *clo);


/*------------------------------------*
 *   INTERNAL TYPES IMPLEMENTATIONS   *
 *------------------------------------*/

static symbols_t 
make_symbols(void)
{
	symbols_t syms;

	if ((syms = malloc(sizeof(*syms))) == NULL) {
		return NULL;
	}
	syms->num_symbols = 0;
	return syms;
}

static void
free_symbols(symbols_t syms)
{
	free(syms);
	return;
}

static void
symbols_add(symbols_t syms, char c)
{
	short int lower, upper;

	lower = 0;
	upper = syms->num_symbols;
	while (lower < upper) {
		short int middle;

		middle = (short int)((lower + upper) / 2);
		if (c > syms->symbols[middle]) {
			lower = (short int)(middle + 1);
		} else if (c < syms->symbols[middle]) {
			upper = middle;
		} else {
			return;
		}
	}
	if (lower < syms->num_symbols) {
		memmove(syms->symbols + lower + 1,
			syms->symbols + lower,
			syms->num_symbols - lower);
	}
	syms->symbols[lower] = c;
	syms->num_symbols++;
	return;
}

/*------------------------------*
     PRIVATE DATA DEFINITONS   
 ------------------------------*/

typedef struct __dacell_s {
	trie_idx_t base;
	trie_idx_t check;
} *dacell_t;

struct darray_s {
	trie_idx_t num_cells;
	dacell_t cells;
};

/*-----------------------------*
     METHODS IMPLEMENTAIONS   
 -----------------------------*/

#define DA_SIGNATURE 0xDAFCDAFC

/* DA Header:
 * - Cell 0: SIGNATURE, number of cells
 * - Cell 1: free circular-list pointers
 * - Cell 2: root node
 * - Cell 3: DA pool begin
 */
#define DA_POOL_BEGIN 3

/**
 * @brief Create a new double-array object
 *
 * Create a new empty doubla-array object.
 */
DEFUN darray_t 
make_darray(void)
{
	darray_t d;

	if ((d = malloc(sizeof(*d))) == NULL) {
		return NULL;
	}
	d->num_cells = DA_POOL_BEGIN;
	if ((d->cells = malloc(d->num_cells * sizeof(*d->cells))) == NULL) {
		goto exit_da_created;
	}
	d->cells[0].base = DA_SIGNATURE;
	d->cells[0].check = d->num_cells;
	d->cells[1].base = -1;
	d->cells[1].check = -1;
	d->cells[2].base = DA_POOL_BEGIN;
	d->cells[2].check = 0;
	return d;

exit_da_created:
	free(d);
	return NULL;
}

DEFUN darray_t
clone_darray(const_darray_t src)
{
	darray_t res;
	const size_t tcsz = src->num_cells * sizeof(*src->cells);

	if ((res = malloc(sizeof(*res))) == NULL) {
		return NULL;
	}
	res->num_cells = src->num_cells;
	if ((res->cells = malloc(tcsz)) == NULL) {
		goto exit_da_created;
	}
	/* memcpy them cells */
	memcpy(res->cells, src->cells, tcsz);
	return res;

exit_da_created:
	free(res);
	return NULL;
}

/**
 * @brief Read double-array data from file
 *
 * @param file : the file to read
 *
 * @return a pointer to the openned double-array, NULL on failure
 *
 * Read double-array data from the opened file, starting from the current
 * file pointer until the end of double array data block. On return, the
 * file pointer is left at the position after the read block.
 */
DEFUN darray_t 
darray_fmread(fmcmb_t stream)
{
	long int save_pos;
	darray_t d = NULL;
	trie_idx_t n;

	/* check signature */
	save_pos = fmtell(stream);
	if (fm_read_uint32(stream, &n) < 0 || DA_SIGNATURE != (uint32_t)n) {
		goto exit_file_read;
	}
	if ((d = malloc(sizeof(*d))) == NULL) {
		goto exit_file_read;
	}
	/* read number of cells */
	if (fm_read_uint32(stream, &d->num_cells) < 0) {
		goto exit_da_created;
	}
	if ((size_t)d->num_cells > (1 << 31) / sizeof(*d->cells)) {
		goto exit_da_created;
	}
	if ((d->cells = malloc(d->num_cells * sizeof(*d->cells))) == NULL) {
		goto exit_da_created;
	}
	d->cells[0].base = DA_SIGNATURE;
	d->cells[0].check = d->num_cells;
	for (n = 1; n < d->num_cells; n++) {
		if (fm_read_uint32(stream, &d->cells[n].base) < 0 ||
		    fm_read_uint32(stream, &d->cells[n].check) < 0) {
			goto exit_da_cells_created;
		}
	}

	return d;

exit_da_cells_created:
	free(d->cells);
exit_da_created:
	free(d);
exit_file_read:
	fmseek(stream, save_pos, SEEK_SET);
	return NULL;
}

/**
 * @brief Free double-array data
 *
 * @param d : the double-array data
 *
 * Free the given double-array data.
 */
DEFUN void
free_darray(darray_t d)
{
	free(d->cells);
	free(d);
	return;
}

/**
 * @brief Write double-array data
 *
 * @param d     : the double-array data
 * @param file  : the file to write to
 *
 * @return 0 on success, non-zero on failure
 *
 * Write double-array data to the given @a file, starting from the current
 * file pointer. On return, the file pointer is left after the double-array
 * data block.
 */
DEFUN int
darray_fmwrite(const_darray_t d, fmcmb_t stream)
{
	trie_idx_t i;

	for (i = 0; i < d->num_cells; i++) {
		if (fm_write_uint32(stream, d->cells[i].base) < 0 ||
		    fm_write_uint32(stream, d->cells[i].check) < 0) {
			return -1;
		}
	}
	return 0;
}


/**
 * @brief Get root state
 *
 * @param d     : the double-array data
 *
 * @return root state of the @a index set, or TRIE_INDEX_ERROR on failure
 *
 * Get root state for stepwise walking.
 */
DEFUN trie_idx_t
da_get_root(const_darray_t __attribute__((unused)) d)
{
	/* can be calculated value for multi-index trie */
	return 2;
}


/**
 * @brief Get BASE cell
 *
 * @param d : the double-array data
 * @param s : the double-array state to get data
 *
 * @return the BASE cell value for the given state
 *
 * Get BASE cell value for the given state.
 */
DEFUN trie_idx_t
da_get_base(const_darray_t d, trie_idx_t s)
{
	return (s < d->num_cells) ? d->cells[s].base : TRIE_INDEX_ERROR;
}

/**
 * @brief Get CHECK cell
 *
 * @param d : the double-array data
 * @param s : the double-array state to get data
 *
 * @return the CHECK cell value for the given state
 *
 * Get CHECK cell value for the given state.
 */
DEFUN trie_idx_t
da_get_check(const_darray_t d, trie_idx_t s)
{
	return (s < d->num_cells) ? d->cells[s].check : TRIE_INDEX_ERROR;
}


/**
 * @brief Set BASE cell
 *
 * @param d   : the double-array data
 * @param s   : the double-array state to get data
 * @param val : the value to set
 *
 * Set BASE cell for the given state to the given value.
 */
DEFUN void
da_set_base(darray_t d, trie_idx_t s, trie_idx_t val)
{
	if (s < d->num_cells) {
		d->cells[s].base = val;
	}
	return;
}

/**
 * @brief Set CHECK cell
 *
 * @param d   : the double-array data
 * @param s   : the double-array state to get data
 * @param val : the value to set
 *
 * Set CHECK cell for the given state to the given value.
 */
DEFUN void
da_set_check(darray_t d, trie_idx_t s, trie_idx_t val)
{
	if (s < d->num_cells) {
		d->cells[s].check = val;
	}
	return;
}

/**
 * @brief Walk in double-array structure
 *
 * @param d : the double-array structure
 * @param s : current state
 * @param c : the input character
 *
 * @return boolean indicating success
 *
 * Walk the double-array trie from state @a s, using input character @a c.
 * If there exists an edge from @a s with arc labeled @a c, this function
 * returns TRUE and @a s is updated to the new state. Otherwise, it returns
 * FALSE and @a s is left unchanged.
 */
DEFUN int
da_walk(const_darray_t d, trie_idx_t *s, char c)
{
	trie_idx_t next;

	next = da_get_base(d, *s) + c;
	if (da_get_check(d, next) == *s) {
		*s = next;
		return 0;
	}
	return -1;
}

/**
 * @brief Insert a branch from trie node
 *
 * @param d : the double-array structure
 * @param s : the state to add branch to
 * @param c : the character for the branch label
 *
 * @return the index of the new node
 *
 * Insert a new arc labelled with character @a c from the trie node 
 * represented by index @a s in double-array structure @a d.
 * Note that it assumes that no such arc exists before inserting.
 */
DEFUN trie_idx_t
da_insert_branch(darray_t d, trie_idx_t s, char c)
{
	trie_idx_t base, next;

	base = da_get_base(d, s);

	if (base > 0) {
		next = base + c;

		/* if already there, do not actually insert */
		if (da_get_check(d, next) == s) {
			return next;
		}
		/* if (base + c) > TRIE_INDEX_MAX which means 'next' is
		 * overflow or cell [next] is not free, relocate to a free slot
		 */
		if (base > TRIE_INDEX_MAX - c ||
		    !da_check_free_cell(d, next)) {
			symbols_t symbols;
			trie_idx_t new_base;

			/* relocate BASE[s] */
			symbols = da_output_symbols(d, s);
			symbols_add(symbols, c);
			new_base = da_find_free_base(d, symbols);
			free_symbols(symbols);

			if (new_base == TRIE_INDEX_ERROR) {
				return TRIE_INDEX_ERROR;
			}
			da_relocate_base(d, s, new_base);
			next = new_base + c;
		}
	} else {
		symbols_t symbols;
		trie_idx_t new_base;

		symbols = make_symbols();
		symbols_add(symbols, c);
		new_base = da_find_free_base(d, symbols);
		free_symbols(symbols);

		if (new_base == TRIE_INDEX_ERROR) {
			return TRIE_INDEX_ERROR;
		}
		da_set_base (d, s, new_base);
		next = new_base + c;
	}
	da_alloc_cell(d, next);
	da_set_check(d, next, s);
	return next;
}

static bool
da_check_free_cell(darray_t d, trie_idx_t s)
{
	return da_extend_pool(d, s) == 0 && da_get_check(d, s) < 0;
}

static bool
da_has_children(darray_t d, trie_idx_t s)
{
	trie_idx_t base;
	trie_idx_t c, max_c;

	base = da_get_base(d, s);
	if (base == TRIE_INDEX_ERROR || base < 0) {
		return false;
	}
	max_c = MIN_VAL(TRIE_CHAR_MAX, TRIE_INDEX_MAX - base);
	for (c = 0; c < max_c; c++) {
		if (da_get_check(d, base + c) == s) {
			return true;
		}
	}
	return false;
}

static symbols_t 
da_output_symbols(const_darray_t d, trie_idx_t s)
{
	symbols_t syms;
	trie_idx_t base;
	trie_idx_t c, max_c;

	syms = make_symbols();

	base = da_get_base(d, s);
	max_c = MIN_VAL(TRIE_CHAR_MAX, TRIE_INDEX_MAX - base);
	for (c = 0; c < max_c; c++) {
		if (da_get_check(d, base + c) == s) {
			symbols_add_fast(syms, (char)c);
		}
	}
	return syms;
}

static char*
da_get_state_key(const_darray_t d, trie_idx_t state)
{
	char *key;
	int key_size, key_length;
	int i;

	key_size = 20;
	key_length = 0;
	key = malloc(key_size);

	/* trace back to root */
	while (da_get_root(d) != state) {
		trie_idx_t parent;

		if (key_length + 1 >= key_size) {
			key_size += 20;
			key = realloc(key, key_size);
		}
		parent = da_get_check(d, state);
		key[key_length++] = (char)(state - da_get_base(d, parent));
		state = parent;
	}
	key[key_length] = '\0';

	/* reverse the string */
	for (i = 0; i < --key_length; i++) {
		char temp;

		temp = key[i];
		key[i] = key[key_length];
		key[key_length] = temp;
	}
	return key;
}

static trie_idx_t
da_find_free_base(darray_t d, const_symbols_t symbols)
{
	char first_sym;
	trie_idx_t s;

	/* find first free cell that is beyond the first symbol */
	first_sym = symbols_get(symbols, 0);
	s = -da_get_check(d, da_get_free_list(d));
	while (s != da_get_free_list(d) &&
	       s < (trie_idx_t)first_sym + DA_POOL_BEGIN) {
		s = -da_get_check(d, s);
	}
	if (s == da_get_free_list(d)) {
		for (s = first_sym + DA_POOL_BEGIN; ; ++s) {
			if (da_extend_pool(d, s) < 0) {
				return TRIE_INDEX_ERROR;
			}
			if (da_get_check(d, s) < 0) {
				break;
			}
		}
	}

	/* search for next free cell that fits the symbols set */
	while (da_fit_symbols(d, s - first_sym, symbols) < 0) {
		/* extend pool before getting exhausted */
		if (-da_get_check(d, s) == da_get_free_list(d)) {
			if (da_extend_pool(d, d->num_cells) < 0) {
				return TRIE_INDEX_ERROR;
			}
		}
		s = -da_get_check(d, s);
	}
	return s - first_sym;
}

static int
da_fit_symbols(darray_t d, trie_idx_t base, const_symbols_t symbols)
{
	int i;

	for (i = 0; i < symbols_num(symbols); i++) {
		char sym = symbols_get(symbols, i);

		/* if (base + sym) > TRIE_INDEX_MAX which means it's overflow,
		 * or cell [base + sym] is not free, the symbol is not fit.
		 */
		if (base > TRIE_INDEX_MAX - sym ||
		    !da_check_free_cell(d, base + sym)) {
			return -1;
		}
	}
	return 0;
}

static void
da_relocate_base(darray_t d, trie_idx_t s, trie_idx_t new_base)
{
	trie_idx_t old_base;
	symbols_t symbols;
	int i;

	old_base = da_get_base(d, s);
	symbols = da_output_symbols(d, s);

	for (i = 0; i < symbols_num(symbols); i++) {
		trie_idx_t old_next, new_next, old_next_base;

		old_next = old_base + symbols_get(symbols, i);
		new_next = new_base + symbols_get(symbols, i);
		old_next_base = da_get_base(d, old_next);

		/* allocate new next node and copy BASE value */
		da_alloc_cell(d, new_next);
		da_set_check(d, new_next, s);
		da_set_base(d, new_next, old_next_base);

		/* old_next node is now moved to new_next
		 * so, all cells belonging to old_next
		 * must be given to new_next
		 */
		/* preventing the case of TAIL pointer */
		if (old_next_base > 0) {
			trie_idx_t c, max_c;

			max_c = MIN_VAL(
				TRIE_CHAR_MAX, TRIE_INDEX_MAX - old_next_base);
			for (c = 0; c < max_c; c++) {
				if (da_get_check(d, old_next_base + c) ==
				    old_next) {
					da_set_check(
						d, old_next_base + c, new_next);
				}
			}
		}
		/* free old_next node */
		da_free_cell(d, old_next);
	}
	free_symbols(symbols);

	/* finally, make BASE[s] point to new_base */
	da_set_base(d, s, new_base);
	return;
}

static int
da_extend_pool(darray_t d, trie_idx_t to_index)
{
	trie_idx_t new_begin;
	trie_idx_t i;
	/* free_tail index */
	trie_idx_t free_tidx;

	if (to_index <= 0 || TRIE_INDEX_MAX <= to_index) {
		return -1;
	}
	if (to_index < d->num_cells) {
		return 0;
	}
	d->cells = realloc(d->cells, (to_index + 1) * sizeof(*d->cells));
	new_begin = d->num_cells;
	d->num_cells = to_index + 1;

	/* initialize new free list */
	for (i = new_begin; i < to_index; i++) {
		da_set_check(d, i, -(i + 1));
		da_set_base(d, i + 1, -i);
	}

	/* merge the new circular list to the old */
	free_tidx = -da_get_base(d, da_get_free_list(d));
	da_set_check(d, free_tidx, -new_begin);
	da_set_base(d, new_begin, -free_tidx);
	da_set_check(d, to_index, -da_get_free_list(d));
	da_set_base(d, da_get_free_list(d), -to_index);

	/* update header cell */
	d->cells[0].check = d->num_cells;
	return 0;
}

/**
 * @brief Prune the single branch
 *
 * @param d : the double-array structure
 * @param s : the dangling state to prune off
 *
 * Prune off a non-separate path up from the final state @a s.
 * If @a s still has some children states, it does nothing. Otherwise, 
 * it deletes the node and all its parents which become non-separate.
 */
DEFUN void
da_prune(darray_t d, trie_idx_t s)
{
	da_prune_upto(d, da_get_root (d), s);
	return;
}

/**
 * @brief Prune the single branch up to given parent
 *
 * @param d : the double-array structure
 * @param p : the parent up to which to be pruned
 * @param s : the dangling state to prune off
 *
 * Prune off a non-separate path up from the final state @a s to the
 * given parent @a p. The prunning stop when either the parent @a p
 * is met, or a first non-separate node is found.
 */
DEFUN void
da_prune_upto(darray_t d, trie_idx_t p, trie_idx_t s)
{
	while (p != s && !da_has_children(d, s)) {
		trie_idx_t parent;

		parent = da_get_check(d, s);
		da_free_cell(d, s);
		s = parent;
	}
	return;
}

static void
da_alloc_cell(darray_t d, trie_idx_t cell)
{
	trie_idx_t prev, next;

	prev = -da_get_base(d, cell);
	next = -da_get_check(d, cell);

	/* remove the cell from free list */
	da_set_check(d, prev, -next);
	da_set_base(d, next, -prev);
	return;
}

static void
da_free_cell(darray_t d, trie_idx_t cell)
{
	trie_idx_t i, prev;

	/* find insertion point */
	i = -da_get_check(d, da_get_free_list(d));
	while (i != da_get_free_list(d) && i < cell) {
		i = -da_get_check(d, i);
	}

	prev = -da_get_base(d, i);

	/* insert cell before i */
	da_set_check(d, cell, -i);
	da_set_base(d, cell, -prev);
	da_set_check(d, prev, -cell);
	da_set_base(d, i, -cell);
	return;
}

/**
 * @brief Enumerate entries stored in double-array structure
 *
 * @param d          : the double-array structure
 * @param enum_func  : the callback function to be called on each separate node
 * @param user_data  : user-supplied data to send as an argument to @a enum_func
 *
 * @return boolean value indicating whether all the keys are visited
 *
 * Enumerate all keys stored in double-array structure. For each entry, the 
 * user-supplied @a enum_func callback function is called, with the entry key,
 * the separate node, and user-supplied data. Returning FALSE from such
 * callback will stop enumeration and return FALSE.
 */
DEFUN int
darray_walk(const_darray_t d, darray_walk_f walkf, void *closure)
{
	return da_walk_recursive(d, da_get_root(d), walkf, closure);
}

static int
da_walk_recursive(const_darray_t d, trie_idx_t st, darray_walk_f w, void *clo)
{
	int ret;
	trie_idx_t base;

	base = da_get_base(d, st);

	if (base < 0) {
		char *key;

		key = da_get_state_key(d, st);
		ret = w(key, st, clo);
		free(key);

	} else {
		symbols_t symbols;
		int i;

		ret = 0;
		symbols = da_output_symbols(d, st);
		for (i = 0; ret >= 0 && i < symbols_num(symbols); i++) {
			ret = da_walk_recursive(
				d, base + symbols_get(symbols, i), w, clo);
		}
		free_symbols(symbols);
	}
	return ret;
}

/* darray.c ends here */
