/** triedefs.h -- general typedefs for trie
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
 */

/*
 * Created: 2006-08-11
 * Author:  Theppitak Karoonboonyanan <thep@linux.thai.net>
 */

#ifndef __TRIEDEFS_H
#define __TRIEDEFS_H

#include <stdint.h>

/**
 * @file triedefs.h
 * @brief General typedefs for trie
 */

/**
 * @brief Trie terminator character
 */
#define TRIE_CHAR_TERM		(0x00)
#define TRIE_CHAR_MAX		(0xff)

/**
 * @brief Type of Trie index
 */
typedef int32_t trie_idx_t;
/**
 * @brief Trie error index
 */
#define TRIE_INDEX_ERROR	(0)
/**
 * @brief Maximum trie index value
 */
#define TRIE_INDEX_MAX		(0x7fffffff)

/**
 * @brief Type of value associated to trie entries
 */
typedef int32_t trie_data_t;
/**
 * @brief Trie error data
 */
#define TRIE_DATA_ERROR		(-1)

#endif  /* __TRIEDEFS_H */
