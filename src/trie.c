/** trie.c -- Trie data type and functions
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
 * Created: 2006-08-11
 * Author:  Theppitak Karoonboonyanan <thep@linux.thai.net>
 */

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "trie.h"
#include "fileutils.h"
#include "darray.h"
#include "tail.h"

/**
 * @brief Trie structure
 */
struct trie_s {
	darray_t da;
	tail_t tail;
	int dirtyp;
};

/**
 * @brief TrieState structure
 */
struct trie_state_s {
	/**< the corresponding trie */
	const_trie_t trie;
	/** index in double-array/tail structures */
	trie_idx_t index;
	/** suffix character offset, if in suffix */
	short int suffix_idx;
	/** whether it is currently in suffix part */
	short int suffixp;
};


/* quick alpha map replacement */
#define alpha_map_char_to_trie(c)	((char)((c) - ' ' + 1))
#define alpha_map_trie_to_char(c)	((char)((c) + ' ' - 1))

static char*
alpha_map_char_to_trie_str(const char *str)
{
	char *trie_str, *p;

	trie_str = malloc(strlen(str) + 1);
	for (p = trie_str; *str; p++, str++) {
		*p = alpha_map_char_to_trie(*str);
	}
	*p = 0;
	return trie_str;
}

static __attribute__((unused)) char*
alpha_map_trie_to_char_str(const char *str)
{
	char *alpha_str, *p;

	alpha_str = malloc(strlen(str) + 1);
	for (p = alpha_str; *str; p++, str++) {
		*p = alpha_map_trie_to_char(*str);
	}
	*p = 0;
	return alpha_str;
}


/*------------------------*
 *   INTERNAL FUNCTIONS   *
 *------------------------*/
#define trie_da_separate_p(da, s)	(da_get_base((da), (s)) < 0)
#define trie_da_get_tail_index(da, s)	(-da_get_base((da), (s)))
#define trie_da_set_tail_index(da, s, v)	(da_set_base((da), (s), -(v)))

static trie_state_t
make_trie_state(
	const_trie_t trie, trie_idx_t idx, short int sufxidx, short int sufxp);

static int
trie_store_maybe(trie_t trie, const char *key, trie_data_t d, int overwritep);

static int
trie_branch_in_branch(
	trie_t trie, trie_idx_t sep_node,
	const char *suffix, trie_data_t data);

static int
trie_branch_in_tail(
	trie_t trie, trie_idx_t sep_node,
	const char *suffix, trie_data_t data);

/*-----------------------*
 *   GENERAL FUNCTIONS   *
 *-----------------------*/

/**
 * @brief Create a new trie
 *
 * @param   alpha_map   : the alphabet set for the trie
 *
 * @return a pointer to the newly created trie, NULL on failure
 *
 * Create a new empty trie object based on the given @a alpha_map alphabet
 * set. The trie contents can then be added and deleted with trie_store() and
 * trie_delete() respectively.
 *
 * The created object must be freed with trie_free().
 */
trie_t
make_trie(void)
{
	trie_t trie;

	if ((trie = (trie_t)malloc(sizeof(*trie))) == NULL) {
		return NULL;
	}

	if ((trie->da = make_darray()) == NULL) {
		goto exit_trie_created;
	}

	if ((trie->tail = make_tail()) == NULL) {
		goto exit_da_created;
	}
 
	trie->dirtyp = 1;
	return trie;

exit_da_created:
	free_darray(trie->da);
exit_trie_created:
	free(trie);
	return NULL;
}

static trie_t
trie_fmread(fmcmb_t stream)
{
	trie_t trie;

	if ((trie = malloc(sizeof(*trie))) == NULL) {
		return NULL;
	}

	if ((trie->da = darray_fmread(stream)) == NULL) {
		goto exit_trie_created;
	}
	if ((trie->tail = tail_fmread(stream)) == NULL) {
		goto exit_da_created;
	}
	trie->dirtyp = 0;
	return trie;

exit_da_created:
	free_darray(trie->da);
exit_trie_created:
	free(trie);
	return NULL;
}

/**
 * @brief Create a new trie by reading from an open file
 *
 * @param file  : the handle of the open file
 *
 * @return a pointer to the created trie, NULL on failure
 *
 * Create a new trie and initialize its contents by reading from the open
 * @a file. After reading, the file pointer is left at the end of the trie data.
 * This can be useful for reading embedded trie index as part of a file data.
 *
 * The created object must be freed with trie_free().
 *
 * Available since: 0.2.4
 */
trie_t
trie_fread(FILE *file)
{
	struct fmcmb_s stream[1] = {{0}};
	stream->f = file;
	return trie_fmread(stream);
}

/**
 * @brief Create a new trie from a block of memory
 *
 * @param mem: a pointer to the memory block
 * @param msz: the size of the memory block
 *
 * @return a pointer to the created trie, NULL on failure
 *
 */
trie_t
trie_mread(char *mem, size_t msz)
{
	struct fmcmb_s stream[1] = {{0}};
	stream->m = stream->mem = mem;
	stream->msz = msz;
	return trie_fmread(stream);
}

/**
 * @brief Free a trie object
 *
 * @param trie  : the trie object to free
 *
 * Destruct the @a trie and free its allocated memory.
 */
void
free_trie(trie_t trie)
{
	free_darray(trie->da);
	free_tail(trie->tail);
	free(trie);
	return;
}

static int
trie_fmwrite(trie_t trie, fmcmb_t stream)
{
	if (darray_fmwrite(trie->da, stream) < 0) {
		return -1;
	}
	if (tail_fmwrite(trie->tail, stream) < 0) {
		return -1;
	}
	trie->dirtyp = 0;
	return 0;
}

/**
 * @brief Write trie data to an open file
 *
 * @param trie  : the trie
 *
 * @param file  : the open file
 *
 * @return 0 on success, non-zero on failure
 *
 * Write @a trie data to @a file which is opened for writing.
 * After writing, the file pointer is left at the end of the trie data.
 * This can be useful for embedding trie index as part of a file data.
 *
 * Available since: 0.2.4
 */
int
trie_fwrite(trie_t trie, FILE *file)
{
	struct fmcmb_s stream[1] = {{0}};
	stream->f = file;
	return trie_fmwrite(trie, stream);
}

/**
 * @brief Write trie data to a memory block
 *
 * @param trie  : the trie
 *
 * @param mem: a pointer to the memory block, if NULL memory will be
 *             allocated
 *
 * @param msz: a pointer to how large the current block of memory is
 *
 * @return 0 on success, non-zero on failure
 *
 */
int
trie_mwrite(trie_t trie, char **mem, size_t *msz)
{
	struct fmcmb_s stream[1];
	int res;

	if (*mem == NULL) {
		/* user is lazy, allocate some memory for them */
		stream->msz = 4096;
		stream->m = stream->mem = malloc(stream->msz);
	} else {
		stream->m = stream->mem = *mem;
		stream->msz = *msz;
	}
	res = trie_fmwrite(trie, stream);
	/* set the final size and back off */
	*mem = stream->mem;
	*msz = __mtell(stream);
	return res;
}

/**
 * @brief Check pending changes
 *
 * @param trie  : the trie object
 *
 * @return TRUE if there are pending changes, FALSE otherwise
 *
 * Check if the @a trie is dirty with some pending changes and needs saving
 * to synchronize with the file.
 */
bool
trie_dirty_p(const_trie_t trie)
{
	return trie->dirtyp;
}


/*------------------------------*
 *   GENERAL QUERY OPERATIONS   *
 *------------------------------*/

/**
 * @brief Retrieve an entry from trie
 *
 * @param trie   : the trie
 * @param key    : the key for the entry to retrieve
 * @param o_data : the storage for storing the entry data on return
 *
 * @return boolean value indicating the existence of the entry.
 *
 * Retrieve an entry for the given @a key from @a trie. On return,
 * if @a key is found and @a o_data is not NULL, @a *o_data is set
 * to the data associated to @a key.
 */
int
trie_retrieve(const_trie_t trie, const char *key, trie_data_t *o_data)
{
	trie_idx_t s;
	short int suffix_idx;
	const char *p;

	/* walk through branches */
	s = da_get_root(trie->da);
	for (p = key; !trie_da_separate_p(trie->da, s); p++) {
		if (da_walk(trie->da, &s, alpha_map_char_to_trie(*p)) < 0) {
			return -1;
		}
		if (*p == 0) {
			break;
		}
	}

	/* walk through tail */
	s = trie_da_get_tail_index(trie->da, s);
	suffix_idx = 0;
	for ( ; ; p++) {
		if (tail_walk_char(
			    trie->tail, s, &suffix_idx,
			    alpha_map_char_to_trie(*p)) < 0) {
			return -1;
		}
		if (*p == 0) {
			break;
		}
	}

	/* found, set the val and return */
	if (o_data) {
		*o_data = tail_get_data(trie->tail, s);
	}
	return 0;
}

/**
 * @brief Store a value for an entry to trie
 *
 * @param trie  : the trie
 * @param key   : the key for the entry to retrieve
 * @param data  : the data associated to the entry
 *
 * @return boolean value indicating the success of the process
 *
 * Store a @a data for the given @a key in @a trie. If @a key does not 
 * exist in @a trie, it will be appended. If it does, its current data will
 * be overwritten.
 */
int
trie_store(trie_t trie, const char *key, trie_data_t data)
{
	return trie_store_maybe(trie, key, data, true);
}

/**
 * @brief Store a value for an entry to trie only if the key is not present
 *
 * @param trie  : the trie
 * @param key   : the key for the entry to retrieve
 * @param data  : the data associated to the entry
 *
 * @return boolean value indicating the success of the process
 *
 * Store a @a data for the given @a key in @a trie. If @a key does not 
 * exist in @a trie, it will be appended. If it does, the function will
 * return failure and the existing value will not be touched.
 *
 * This can be useful for multi-thread applications, as race condition
 * can be avoided.
 *
 * Available since: 0.2.4
 */
int
trie_store_if_absent(trie_t trie, const char *key, trie_data_t data)
{
	return trie_store_maybe(trie, key, data, false);
}

static int
trie_store_maybe(trie_t trie, const char *key, trie_data_t d, int overwritep)
{
	trie_idx_t s, t;
	short int suffix_idx;
	const char *p, *sep;

	/* walk through branches */
	s = da_get_root(trie->da);
	for (p = key; !trie_da_separate_p(trie->da, s); p++) {
		if (da_walk(trie->da, &s, alpha_map_char_to_trie(*p)) < 0) {
			char *key_str;
			int res;

			key_str = alpha_map_char_to_trie_str(p);
			res = trie_branch_in_branch(trie, s, key_str, d);
			free(key_str);

			return res;
		}
		if (*p == '\0') {
			break;
		}
	}

	/* walk through tail */
	sep = p;
	t = trie_da_get_tail_index(trie->da, s);
	suffix_idx = 0;
	for ( ; ; p++) {
		if (tail_walk_char(
			    trie->tail, t, &suffix_idx,
			    alpha_map_char_to_trie(*p)) < 0) {
			char *tail_str;
			int res;

			tail_str = alpha_map_char_to_trie_str(sep);
			res = trie_branch_in_tail(trie, s, tail_str, d);
			free(tail_str);
			return res;
		}
		if (*p == '\0') {
			break;
		}
	}

	/* duplicated key, overwrite val if flagged */
	if (!overwritep) {
		return -1;
	}
	tail_set_data(trie->tail, t, d);
	trie->dirtyp = 1;
	return 0;
}

static int
trie_branch_in_branch(
	trie_t trie, trie_idx_t sep_node,
	const char *suffix, trie_data_t data)
{
	trie_idx_t new_da, new_tail;

	new_da = da_insert_branch(trie->da, sep_node, *suffix);
	if (new_da == TRIE_INDEX_ERROR) {
		return -1;
	}
	if (*suffix != '\0') {
		++suffix;
	}
	new_tail = tail_add_suffix(trie->tail, suffix);
	tail_set_data(trie->tail, new_tail, data);
	trie_da_set_tail_index(trie->da, new_da, new_tail);

	trie->dirtyp = 1;
	return 0;
}

static int
trie_branch_in_tail(
	trie_t trie, trie_idx_t sep_node,
	const char *suffix, trie_data_t data)
{
	trie_idx_t old_tail, old_da, s;
	const char *old_suffix, *p;

	/* adjust separate point in old path */
	old_tail = trie_da_get_tail_index(trie->da, sep_node);
	if ((old_suffix = tail_get_suffix(trie->tail, old_tail)) == NULL) {
		return -1;
	}

	for (p = old_suffix, s = sep_node; *p == *suffix; p++, suffix++) {
		trie_idx_t t = da_insert_branch(trie->da, s, *p);
		if (t == TRIE_INDEX_ERROR) {
			goto fail;
		}
		s = t;
	}

	old_da = da_insert_branch(trie->da, s, *p);
	if (old_da == TRIE_INDEX_ERROR) {
		goto fail;
	}

	if (*p != '\0') {
		p++;
	}
	tail_set_suffix(trie->tail, old_tail, p);
	trie_da_set_tail_index(trie->da, old_da, old_tail);

	/* insert the new branch at the new separate point */
	return trie_branch_in_branch(trie, s, suffix, data);

fail:
	/* failed, undo previous insertions and return error */
	da_prune_upto(trie->da, sep_node, s);
	trie_da_set_tail_index(trie->da, sep_node, old_tail);
	return -1;
}

/**
 * @brief Delete an entry from trie
 *
 * @param trie  : the trie
 * @param key   : the key for the entry to delete
 *
 * @return boolean value indicating whether the key exists and is removed
 *
 * Delete an entry for the given @a key from @a trie.
 */
int
trie_delete(trie_t trie, const char *key)
{
	trie_idx_t s, t;
	short int suffix_idx;
	const char *p;

	/* walk through branches */
	s = da_get_root(trie->da);
	for (p = key; !trie_da_separate_p(trie->da, s); p++) {
		if (da_walk(trie->da, &s, alpha_map_char_to_trie(*p)) < 0) {
			return -1;
		}
		if (*p == 0) {
			break;
		}
	}

	/* walk through tail */
	t = trie_da_get_tail_index(trie->da, s);
	suffix_idx = 0;
	for ( ; ; p++) {
		if (tail_walk_char(
			    trie->tail, t, &suffix_idx,
			    alpha_map_char_to_trie(*p)) < 0) {
			return -1;
		}
		if (*p == 0) {
			break;
		}
	}

	tail_delete(trie->tail, t);
	da_set_base(trie->da, s, TRIE_INDEX_ERROR);
	da_prune(trie->da, s);

	trie->dirtyp = 1;
	return 0;
}

typedef struct __walk_data_s {
	const_trie_t trie;
	trie_walk_f walkf;
	void *user_data;
} *__walk_data_t;

static int
trie_da_enum_func(const char *key, trie_idx_t sep_node, void *user_data)
{
	__walk_data_t walk_data;
	trie_idx_t t;
	const char *suffix;
	char *full_key, *p;
	int ret;
	trie_data_t data;
	size_t keyl, suffixl;

	walk_data = user_data;

	t = trie_da_get_tail_index(walk_data->trie->da, sep_node);
	suffix = tail_get_suffix(walk_data->trie->tail, t);

	keyl = strlen(key);
	suffixl = strlen(suffix);
	full_key = malloc(keyl + suffixl + 1);
	for (p = full_key; *key; p++, key++) {
		*p = alpha_map_trie_to_char(*key);
	}
	for ( ; *suffix; p++, suffix++) {
		*p = alpha_map_trie_to_char(*suffix);
	}
	*p = 0;

	data = tail_get_data(walk_data->trie->tail, t);
	ret = walk_data->walkf(full_key, data, walk_data->user_data);

	free(full_key);
	return ret;
}

/**
 * @brief Enumerate entries in trie
 *
 * @param trie: the trie
 * @param walkf: the callback function to be called on each key
 * @param closure: user-supplied data to send as an argument to @a walkf
 *
 * @return 0 if all keys have been visited, -1 otherwise.
 *
 * Enumerate all entries in trie. For each entry, the user-supplied 
 * @a walkf callback function is called, with the entry key and data.
 * When the callback returns -1 the traversal will stop.
 */
int
trie_walk(const_trie_t trie, trie_walk_f walkf, void *closure)
{
	struct __walk_data_s walk_data = {
		.trie = trie,
		.walkf = walkf,
		.user_data = closure,
	};
	return darray_walk(trie->da, trie_da_enum_func, &walk_data);
}


/*-------------------------------*
 *   STEPWISE QUERY OPERATIONS   *
 *-------------------------------*/

/**
 * @brief Get root state of a trie
 *
 * @param trie : the trie
 *
 * @return the root state of the trie
 *
 * Get root state of @a trie, for stepwise walking.
 *
 * The returned state is allocated and must be freed with trie_state_free()
 */
trie_state_t
trie_root(const_trie_t trie)
{
	return make_trie_state(trie, da_get_root(trie->da), 0, false);
}

/*----------------*
 *   TRIE STATE   *
 *----------------*/

static trie_state_t
make_trie_state(const_trie_t t, trie_idx_t idx, short int sfx, short int sfxp)
{
	trie_state_t s;

	if ((s = malloc(sizeof(*s))) == NULL) {
		return NULL;
	}

	s->trie = t;
	s->index = idx;
	s->suffix_idx = sfx;
	s->suffixp = sfxp;
	return s;
}

/**
 * @brief Copy trie state to another
 *
 * @param dst  : the destination state
 * @param src  : the source state
 *
 * Copy trie state data from @a src to @a dst. All existing data in @a dst
 * is overwritten.
 */
void
trie_state_copy(trie_state_t dst, const_trie_state_t src)
{
	/* May be deep copy if necessary, not the case for now */
	*dst = *src;
	return;
}

/**
 * @brief Clone a trie state
 *
 * @param s    : the state to clone
 *
 * @return an duplicated instance of @a s
 *
 * Make a copy of trie state.
 *
 * The returned state is allocated and must be freed with trie_state_free()
 */
trie_state_t
trie_state_clone(const_trie_state_t s)
{
	return make_trie_state(s->trie, s->index, s->suffix_idx, s->suffixp);
}

/**
 * @brief Free a trie state
 *
 * @param s    : the state to free
 *
 * Free the trie state.
 */
void
free_trie_state(trie_state_t s)
{
	free(s);
	return;
}

/**
 * @brief Rewind a trie state
 *
 * @param s    : the state to rewind
 *
 * Put the state at root.
 */
void
trie_state_rewind(trie_state_t s)
{
	s->index = da_get_root(s->trie->da);
	s->suffixp = false;
	return;
}

/**
 * @brief Walk the trie from the state
 *
 * @param s    : current state
 * @param c    : key character for walking
 *
 * @return boolean value indicating the success of the walk
 *
 * Walk the trie stepwise, using a given character @a c.
 * On return, the state @a s is updated to the new state if successfully walked.
 */
int
trie_state_walk(trie_state_t s, char c)
{
	char tc = alpha_map_char_to_trie(c);

	if (!s->suffixp) {
		int ret = da_walk(s->trie->da, &s->index, tc);

		if (ret == 0 && trie_da_separate_p(s->trie->da, s->index)) {
			s->index = trie_da_get_tail_index(s->trie->da, s->index);
			s->suffix_idx = 0;
			s->suffixp = true;
		}

		return ret;
	}
	/* otherwise */
	return tail_walk_char(s->trie->tail, s->index, &s->suffix_idx, tc);
}

/**
 * @brief Test walkability of character from state
 *
 * @param s    : the state to check
 * @param c    : the input character
 *
 * @return boolean indicating walkability
 *
 * Test if there is a transition from state @a s with input character @a c.
 */
bool
trie_state_walkable_p(const_trie_state_t s, char c)
{
	char tc = alpha_map_char_to_trie(c);

	if (!s->suffixp) {
		return da_walkable_p(s->trie->da, s->index, tc);
	}
	/* otherwise */
	return tail_walkable_char_p(s->trie->tail, s->index, s->suffix_idx, tc);
}

/**
 * @brief Check for single path
 *
 * @param s    : the state to check
 *
 * @return boolean value indicating whether it is in a single path
 *
 * Check if the given state is in a single path, that is, there is no other
 * branch from it to leaf.
 */
bool
trie_state_single_p(const_trie_state_t s)
{
	return s->suffixp;
}

/**
 * @brief Get data from leaf state
 *
 * @param s    : a leaf state
 *
 * @return the data associated with the leaf state @a s,
 *         or TRIE_DATA_ERROR if @a s is not a leaf state
 *
 * Get value from a leaf state of trie. Getting value from a non-leaf state
 * will result in TRIE_DATA_ERROR.
 */
trie_data_t
trie_state_get_data(const_trie_state_t s)
{
	return s->suffixp
		? tail_get_data(s->trie->tail, s->index)
		: TRIE_DATA_ERROR;
}

/* trie.c ends here */
