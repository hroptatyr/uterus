/** tail.c -- trie tail for keeping suffices
 *
 * libdatrie - Double-Array Trie Library
 * Copyright (C) 2006  Theppitak Karoonboonyanan <thep@linux.thai.net>
 * Copyright (C) 2010-2015 Sebastian Freundt  <hroptatyr@unserding.org>
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
 * tail.c - trie tail for keeping suffixes
 * Created: 2006-08-15
 * Author:  Theppitak Karoonboonyanan <thep@linux.thai.net>
 */

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "tail.h"
#include "fileutils.h"

#if !defined DEFUN
# define DEFUN
#endif	/* !DEFUN */

static trie_idx_t tail_alloc_block(tail_t t);
static void tail_free_block(tail_t t, trie_idx_t block);


typedef struct tail_block_s {
	trie_idx_t next_free;
	trie_data_t data;
	char *suffix;
} *tail_block_t;

struct tail_s {
	trie_idx_t num_tails;
	tail_block_t tails;
	trie_idx_t first_free;
};

#define TAIL_SIGNATURE      0xDFFCDFFCL
#define TAIL_START_BLOCKNO  1

/* tail_t Header:
 * INT32: signature
 * INT32: pointer to first free slot
 * INT32: number of tail blocks
 *
 * tail_t Blocks:
 * INT32: pointer to next free block (-1 for allocated blocks)
 * INT32: data for the key
 * INT16: length
 * BYTES[length]: suffix string (no terminating '\0')
 */

/**
 * @brief Create a new tail object
 *
 * Create a new empty tail object.
 */
DEFUN tail_t 
make_tail(void)
{
	tail_t t;

	if ((t = malloc(sizeof(*t))) == NULL) {
		return NULL;
	}
	t->first_free = 0;
	t->num_tails = 0;
	t->tails = NULL;
	return t;
}

DEFUN tail_t
clone_tail(const_tail_t src)
{
	tail_t res;

	if ((res = malloc(sizeof(*res))) == NULL) {
		return NULL;
	}
	res->first_free = src->first_free;
	res->num_tails = src->num_tails;

	if ((res->tails = malloc(
		     res->num_tails * sizeof(*res->tails))) == NULL) {
		goto exit_tail_created;
	}
	for (trie_idx_t i = 0; i < res->num_tails; i++) {
		res->tails[i].next_free = src->tails[i].next_free;
		res->tails[i].data = src->tails[i].data;
		res->tails[i].suffix = strdup(src->tails[i].suffix);
	}
	return res;

exit_tail_created:
	free(res);
	return NULL;
}

/**
 * @brief Read tail data from file
 *
 * @param file : the file to read
 *
 * @return a pointer to the openned tail data, NULL on failure
 *
 * Read tail data from the opened file, starting from the current
 * file pointer until the end of tail data block. On return, the
 * file pointer is left at the position after the read block.
 */
DEFUN tail_t 
tail_fmread(fmcmb_t stream)
{
	long int save_pos;
	tail_t t;
	trie_idx_t i;
	uint32_t sig;

	/* check signature */
	save_pos = fmtell(stream);
	if (fm_read_uint32(stream, &sig) < 0 || TAIL_SIGNATURE != sig) {
		goto exit_file_read;
	}
	if ((t = malloc(sizeof(*t))) == NULL) {
		goto exit_file_read;
	}
	if (fm_read_uint32(stream, &t->first_free) < 0 ||
	    fm_read_uint32(stream, &t->num_tails) < 0) {
		goto exit_tail_created;
	}
	if ((size_t)t->num_tails > (1 << 31) / sizeof(*t->tails)) {
		goto exit_tail_created;
	}
	if ((t->tails = malloc(t->num_tails * sizeof(*t->tails))) == NULL) {
		goto exit_tail_created;
	}
	for (i = 0; i < t->num_tails; i++) {
		int16_t length;

		if (fm_read_uint32(stream, &t->tails[i].next_free) < 0 ||
		    fm_read_uint32(stream, &t->tails[i].data) < 0 ||
		    fm_read_uint16(stream, &length) < 0) {
			goto exit_in_loop;
		}

		t->tails[i].suffix = (char*)malloc(length + 1);
		if (length > 0) {
			if (fm_read_chars(
				    stream, t->tails[i].suffix, length) < 0) {
				free(t->tails[i].suffix);
				goto exit_in_loop;
			}
		}
		t->tails[i].suffix[length] = '\0';
	}
	return t;

exit_in_loop:
	while (i > 0) {
		free(t->tails[--i].suffix);
	}
	free (t->tails);
exit_tail_created:
	free (t);
exit_file_read:
	fmseek(stream, save_pos, SEEK_SET);
	return NULL;
}

/**
 * @brief Free tail data
 *
 * @param t : the tail data
 *
 * @return 0 on success, non-zero on failure
 *
 * Free the given tail data.
 */
DEFUN void
free_tail(tail_t t)
{
	trie_idx_t i;

	if (t->tails) {
		for (i = 0; i < t->num_tails; i++) {
			if (t->tails[i].suffix) {
				free (t->tails[i].suffix);
			}
		}
		free (t->tails);
	}
	free (t);
	return;
}

/**
 * @brief Write tail data
 *
 * @param t     : the tail data
 * @param file  : the file to write to
 *
 * @return 0 on success, non-zero on failure
 *
 * Write tail data to the given @a file, starting from the current file
 * pointer. On return, the file pointer is left after the tail data block.
 */
DEFUN int
tail_fmwrite(const_tail_t t, fmcmb_t stream)
{
	trie_idx_t i;

	if (fm_write_uint32(stream, TAIL_SIGNATURE) < 0 ||
	    fm_write_uint32(stream, t->first_free) < 0 ||
	    fm_write_uint32(stream, t->num_tails) < 0) {
		return -1;
	}
	for (i = 0; i < t->num_tails; i++) {
		int16_t length = 0;

		if (fm_write_uint32(stream, t->tails[i].next_free) < 0 ||
		    fm_write_uint32(stream, t->tails[i].data) < 0) {
			return -1;
		}

		if (t->tails[i].suffix) {
			length = (int16_t)strlen(t->tails[i].suffix);
		}
		if (fm_write_uint16(stream, length) < 0) {
			return -1;
		}
		if (length > 0 &&
		    fm_write_chars(stream, t->tails[i].suffix, length) < 0) {
			return -1;
		}
	}
	return 0;
}


/**
 * @brief Get suffix
 *
 * @param t     : the tail data
 * @param index : the index of the suffix
 *
 * @return an allocated string of the indexed suffix.
 *
 * Get suffix from tail with given @a index. The returned string is allocated.
 * The caller should free it with free().
 */
DEFUN const char*
tail_get_suffix(const_tail_t t, trie_idx_t idx)
{
	idx -= TAIL_START_BLOCKNO;
	return (idx < t->num_tails) ? t->tails[idx].suffix : NULL;
}

/**
 * @brief Set suffix of existing entry
 *
 * @param t      : the tail data
 * @param index  : the index of the suffix
 * @param suffix : the new suffix
 *
 * Set suffix of existing entry of given @a index in tail.
 */
DEFUN int
tail_set_suffix(tail_t t, trie_idx_t idx, const char *suffix)
{
	idx -= TAIL_START_BLOCKNO;
	if (idx < t->num_tails) {
		/* suffix and t->tails[idx].suffix may overlap;
		 * so, dup it before it's overwritten
		 */
		char *tmp = NULL;
		if (suffix) {
			tmp = (char*)strdup((const char*)suffix);
		}
		if (t->tails[idx].suffix) {
			free (t->tails[idx].suffix);
		}
		t->tails[idx].suffix = tmp;
		return 0;
	}
	return -1;
}

/**
 * @brief Add a new suffix
 *
 * @param t      : the tail data
 * @param suffix : the new suffix
 *
 * @return the index of the newly added suffix.
 *
 * Add a new suffix entry to tail.
 */
DEFUN trie_idx_t
tail_add_suffix(tail_t t, const char *suffix)
{
	trie_idx_t new_block;

	new_block = tail_alloc_block(t);
	tail_set_suffix(t, new_block, suffix);
	return new_block;
}

static trie_idx_t
tail_alloc_block(tail_t t)
{
	trie_idx_t block;

	if (t->first_free != 0) {
		block = t->first_free;
		t->first_free = t->tails[block].next_free;
	} else {
		block = t->num_tails;
		t->tails = realloc(
			t->tails, ++t->num_tails * sizeof(*t->tails));
	}
	t->tails[block].next_free = -1;
	t->tails[block].data = TRIE_DATA_ERROR;
	t->tails[block].suffix = NULL;
    
	return block + TAIL_START_BLOCKNO;
}

static void
tail_free_block(tail_t t, trie_idx_t block)
{
	trie_idx_t i, j;

	block -= TAIL_START_BLOCKNO;

	if (block >= t->num_tails) {
		return;
	}

	t->tails[block].data = TRIE_DATA_ERROR;
	if (t->tails[block].suffix != NULL) {
		free(t->tails[block].suffix);
		t->tails[block].suffix = NULL;
	}

	/* find insertion point */
	j = 0;
	for (i = t->first_free; i != 0 && i < block;
	     i = t->tails[i].next_free) {
		j = i;
	}

	/* insert free block between j and i */
	t->tails[block].next_free = i;
	if (j != 0) {
		t->tails[j].next_free = block;
	} else {
		t->first_free = block;
	}
	return;
}

/**
 * @brief Get data associated to suffix entry
 *
 * @param t      : the tail data
 * @param index  : the index of the suffix
 *
 * @return the data associated to the suffix entry
 *
 * Get data associated to suffix entry @a index in tail data.
 */
DEFUN trie_data_t
tail_get_data(const_tail_t t, trie_idx_t idx)
{
	idx -= TAIL_START_BLOCKNO;
	return (idx < t->num_tails) ? t->tails[idx].data : TRIE_DATA_ERROR;
}

/**
 * @brief Set data associated to suffix entry
 *
 * @param t      : the tail data
 * @param index  : the index of the suffix
 * @param data   : the data to set
 *
 * @return boolean indicating success
 *
 * Set data associated to suffix entry @a index in tail data.
 */
DEFUN int
tail_set_data(tail_t t, trie_idx_t idx, trie_data_t data)
{
	idx -= TAIL_START_BLOCKNO;
	if (idx < t->num_tails) {
		t->tails[idx].data = data;
		return 0;
	}
	return -1;
}

/**
 * @brief Delete suffix entry
 *
 * @param t      : the tail data
 * @param index  : the index of the suffix to delete
 *
 * Delete suffix entry from the tail data.
 */
DEFUN void
tail_delete(tail_t t, trie_idx_t idx)
{
	tail_free_block(t, idx);
	return;
}

/**
 * @brief Walk in tail with a string
 *
 * @param t          : the tail data
 * @param s          : the tail data index
 * @param suffix_idx : pointer to current character index in suffix
 * @param str        : the string to use in walking
 * @param len        : total characters in @a str to walk
 *
 * @return total number of characters successfully walked
 *
 * Walk in the tail data @a t at entry @a s, from given character position
 * @a *suffix_idx, using @a len characters of given string @a str. On return,
 * @a *suffix_idx is updated to the position after the last successful walk,
 * and the function returns the total number of character succesfully walked.
 */
DEFUN int
tail_walk_str(
	const_tail_t t, trie_idx_t s,
	short int*suffix_idx, const char *str, int len)
{
	const char *suffix;
	int i;
	short int j;

	if ((suffix = tail_get_suffix(t, s)) == NULL) {
		return -1;
	}
	i = 0;
	j = *suffix_idx;
	while (i < len) {
		if (str[i] != suffix[j]) {
			break;
		}
		++i;
		/* stop and stay at null-terminator */
		if (suffix[j] == 0) {
			break;
		}
		++j;
	}
	*suffix_idx = j;
	return i;
}

/**
 * @brief Walk in tail with a character
 *
 * @param t          : the tail data
 * @param s          : the tail data index
 * @param suffix_idx : pointer to current character index in suffix
 * @param c          : the character to use in walking
 *
 * @return boolean indicating success
 *
 * Walk in the tail data @a t at entry @a s, from given character position
 * @a *suffix_idx, using given character @a c. If the walk is successful,
 * it returns TRUE, and @a *suffix_idx is updated to the next character.
 * Otherwise, it returns FALSE, and @a *suffix_idx is left unchanged.
 */
DEFUN int
tail_walk_char(const_tail_t t, trie_idx_t s, short int *suffix_idx, char c)
{
	const char *suffix;
	char suffix_char;

	if ((suffix = tail_get_suffix(t, s)) == NULL) {
		return -1;
	}
	suffix_char = suffix[*suffix_idx];
	if (suffix_char == c) {
		if (suffix_char != 0) {
			++*suffix_idx;
		}
		return 0;
	}
	return -1;
}

/* tail.c ends here */
