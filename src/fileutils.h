/** fileutils.h -- File utility functions
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
 * Created: 2006-08-14
 * Author:  Theppitak Karoonboonyanan <thep@linux.thai.net>
 */

#ifndef __FILEUTILS_H
#define __FILEUTILS_H

#include <stdint.h>
#include <stdio.h>
#include "fmcmb.h"

/* real API */
extern int fm_read_int32(fmcmb_t filemem, int32_t *o_val);
extern int fm_write_int32(fmcmb_t filemem, int32_t val);

extern int fm_read_int16(fmcmb_t filemem, int16_t *o_val);
extern int fm_write_int16(fmcmb_t filemem, int16_t val);

extern int fm_read_int8(fmcmb_t filemem, int8_t *o_val);
extern int fm_write_int8(fmcmb_t filemem, int8_t val);

extern int fm_read_chars(fmcmb_t filemem, char *buff, int len);
extern int fm_write_chars(fmcmb_t filemem, const char *buff, int len);

#endif /* __FILEUTILS_H */
