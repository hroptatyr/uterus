/*** pageio.c -- buffer for large block access
 *
 * Copyright (C) 2009 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <sebastian.freundt@ga-group.nl>
 *
 * This file is part of unserding.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the author nor the names of any contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ***/

#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "pageio.h"
#include "mem.h"

#if !defined _INDEXT
# define _INDEXT
typedef size_t index_t;
#endif	/* !_INDEXT */

struct prdbuf_s {
	int fd;
	size_t tot;
	size_t idx;
	size_t pgno;
	size_t mpsz;
	char *data;
};

static size_t glob_pgsz;


FDEFU prdbuf_t
make_prdbuf(int fd)
{
	prdbuf_t res = xnew(struct prdbuf_s);
	struct stat st;

	glob_pgsz = (size_t)sysconf(_SC_PAGESIZE);
	fstat(fd, &st);
	res->fd = fd;
	res->tot = st.st_size;
	res->idx = 0;
	res->pgno = 0;
	res->mpsz = 0;
	res->data = NULL;
	return res;
}

FDEFU void
free_prdbuf(prdbuf_t prdb)
{
	xfree(prdb);
	return;
}

FDEFU bool
prdb_fetch_lines(prdbuf_t prdb)
{
	index_t offs = prdb->pgno + prdb->idx;
	index_t new_idx = offs % glob_pgsz;
	index_t offs_al = offs - new_idx;
	size_t sz = PRDB_PGSZ;
	int fd = prdb->fd;

	if (offs >= prdb->tot) {
		return false;
	}
	if (offs_al + sz > prdb->tot) {
		sz = prdb->tot - offs_al;
	}
	prdb->data = mmap(prdb->data, sz, PROT_READ, MAP_SHARED, fd, offs_al);
#if defined USE_DEBUGGING_ASSERTIONS
	if (prdb->data == MAP_FAILED) {
		/* could eval errno */
		abort();
	}
#endif	/* USE_DEBUGGING_ASSERTIONS */
	prdb->mpsz = sz;
	prdb->pgno = offs_al;
	prdb->idx = new_idx;
	PIO_DEBUG("fetched of:%zu sz:%zu ix:%zu\n", prdb->pgno, sz, offs);
	posix_madvise(prdb->data, prdb->mpsz, POSIX_MADV_SEQUENTIAL);
	return true;
}

FDEFU void
prdb_unfetch_lines(prdbuf_t prdb)
{
	if (prdb->data != NULL && prdb->data != MAP_FAILED) {
		PIO_DEBUG("munmap()ing %p of size %zu\n",
			  prdb->data, prdb->mpsz);
		munmap(prdb->data, prdb->mpsz);
	}
	return;
}

FDEFU bool
prdb_one_more_line_p(prdbuf_t prdb)
{
	if (LIKELY(prdb->pgno + prdb->mpsz < prdb->tot)) {
		if (LIKELY(prdb->mpsz - prdb->idx > MAX_LINE_LEN)) {
			return true;
		}
	}
#if defined DEBUG_FLAG
	if (prdb->idx >= prdb->mpsz) {
		PIO_DEBUG("hit map boundary\n");
	}
#endif	/* DEBUG_FLAG */
	/* bummer we're on the last page */
	for (index_t i = prdb->idx; i < prdb->mpsz; i++) {
		if (prdb->data[i] == '\n') {
			return true;
		}
	}
	return false;
}

FDEFU const char*
prdb_current_line(prdbuf_t prdb)
{
	return prdb->data + prdb->idx;
}

FDEFU void
prdb_set_current_line_by_ptr(prdbuf_t prdb, const char *p)
{
	index_t new = p - prdb->data;

#if defined USE_DEBUGGING_ASSERTIONS
	if (new > prdb->mpsz) {
		abort();
	}
#endif
	prdb->idx = new;
	return;
}

FDEFU void
prdb_set_current_line_by_offs(prdbuf_t prdb, size_t len)
{
	prdb->idx += len;
	return;
}

/* pageio.c ends here */
