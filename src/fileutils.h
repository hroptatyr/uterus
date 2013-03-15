/** fileutils.h -- File utility functions
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
 *
 **/
/*
 * Created: 2006-08-14
 * Author:  Theppitak Karoonboonyanan <thep@linux.thai.net>
 */
#if !defined INCLUDED_fileutils_h_
#define INCLUDED_fileutils_h_

#include <stdint.h>
#include <stdio.h>
#include "fmcmb.h"

#if !defined DECLF
# define DECLF		extern
#endif	/* !DECLF */

/* real API */
DECLF int fm_read_uint32(fmcmb_t filemem, void *o_val);
DECLF int fm_write_uint32(fmcmb_t filemem, uint32_t val);

DECLF int fm_read_uint16(fmcmb_t filemem, void *o_val);
DECLF int fm_write_uint16(fmcmb_t filemem, uint16_t val);

DECLF int fm_read_uint8(fmcmb_t filemem, void *o_val);
DECLF int fm_write_uint8(fmcmb_t filemem, uint8_t val);

DECLF int fm_read_chars(fmcmb_t filemem, void *buff, size_t len);
DECLF int fm_write_chars(fmcmb_t filemem, const char *buff, size_t len);

#endif	/* INCLUDED_fileutils_h_ */
