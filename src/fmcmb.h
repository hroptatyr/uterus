/** fmcmb.h -- file utility functions
 *
 * libdatrie - Double-Array Trie Library
 * Copyright (C) 2006  Theppitak Karoonboonyanan <thep@linux.thai.net>
 * Copyright (C) 2010  Sebastian Freundt  <freundt@fresse.org>
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
 * Created: 2010-10-05
 * Author:  Sebastian Freundt <freundt@fresse.org>
 */

#ifndef __FMCMB_H
#define __FMCMB_H

#include <stdio.h>

typedef struct fmcmb_s *fmcmb_t;

struct fmcmb_s {
	union {
		FILE *f;
		char *m;
	};
	char *mem;
	size_t msz;
};

static inline long int
__mtell(fmcmb_t stream)
{
/* like ftell() for mem-block streams, doesn't check its arguments! */
	return (stream->m - stream->mem);
}

static inline long int
fmtell(fmcmb_t stream)
{
/* ftell() for FILEs and the offset for mem blocks */
	if (stream->msz == 0) {
		return ftell(stream->f);
	} else {
		return __mtell(stream);
	}
}

static inline int
fmseek(fmcmb_t stream, long int offs, int whence)
{
/* fseek() for FILEs and set the offset for mem blocks */
	if (stream->msz == 0) {
		return fseek(stream->f, offs, whence);
	}
	switch (whence) {
	case SEEK_CUR:
		/* rewrite offs in terms of SEEK_END */
		offs += __mtell(stream) - stream->msz;
	case SEEK_END:
		/* rewrite offs in terms of SEEK_SET */
		offs += stream->msz;
	case SEEK_SET:
		if (offs <= stream->msz) {
			stream->m = stream->mem + offs;
			return 0;
		}
	default:
		return -1;
	}
}

#endif /* __FMCMB_H */
