/*** expobuf.h -- buffer for large block access
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

#if !defined INCLUDED_expobuf_h_
#define INCLUDED_expobuf_h_

#if !defined __USE_XOPEN2K
# define __USE_XOPEN2K
#endif	/* !XOPEN2K */
#include <stdbool.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>
#include "mem.h"

#define NET_BUF_LEN		1280
#define DISK_BUF_LEN		65536
#define NBUFFERS		512
#define EB_PGSZ			(DISK_BUF_LEN * NBUFFERS)

#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif	/* !UNLIKELY */
#if !defined UNUSED
# define UNUSED(_x)	__attribute__((unused)) _x
#endif	/* !UNUSED */

#define STATIC_EB	1

typedef struct expobuf_s *expobuf_t;

struct expobuf_s {
	int fd;
	size_t tot;
	size_t idx;
	size_t pgno;
	char *data;
};

static size_t glob_pgsz;

static inline bool
mmapable(int fd)
{
	return fd > STDIN_FILENO;
}


static inline expobuf_t
make_expobuf(int fd)
{
#if defined STATIC_EB
	static struct expobuf_s res[1] = {0};
#else  /* !STATIC_EB */
	expobuf_t res = xnew(*res);
#endif	/* STATIC_EB */
	struct stat st;

	glob_pgsz = (size_t)sysconf(_SC_PAGESIZE);
	fstat(fd, &st);
	if (mmapable(res->fd = fd)) {
		res->tot = st.st_size;
		res->data = NULL;
	} else {
		res->tot = -1;
		/* get us a nice large page buffer here */
#define MAP_MEM		(MAP_ANONYMOUS | MAP_PRIVATE)
#define PROT_MEM	(PROT_READ | PROT_WRITE)
		res->data = mmap(NULL, EB_PGSZ, PROT_MEM, MAP_MEM, 0, 0);
	}
	res->idx = 0;
	res->pgno = 0;
	return res;
}

static inline void
free_expobuf(expobuf_t UNUSED(eb))
{
#if !defined STATIC_EB
	xfree(eb);
#endif	/* !STATIC_EB */
	return;
}

#define MMAP(_p, _fd, _offs)					\
	mmap(_p, EB_PGSZ, PROT_READ, MAP_PRIVATE, _fd, _offs)
static bool
eb_fetch_lines_df(expobuf_t eb)
{
	size_t offs = eb->pgno - (EB_PGSZ - eb->idx) % EB_PGSZ;
	size_t new_idx = offs % glob_pgsz;
	/* page-aligned offset */
	size_t offs_al = offs - new_idx;

	if (eb->pgno > eb->tot) {
		return false;
	}
	eb->data = MMAP(eb->data, eb->fd, offs_al);
	if (eb->data == MAP_FAILED) {
		/* could eval errno */
		return false;
	}
	posix_madvise(eb->data, EB_PGSZ, POSIX_MADV_SEQUENTIAL);
	eb->pgno = offs_al + EB_PGSZ;
	eb->idx = new_idx;
	return true;
}
#undef MMAP

static ssize_t __attribute__((unused))
read_safe(int fd, char *buf, size_t sz)
{
	ssize_t nrd = 0, tot = 0;
	while ((nrd = read(fd, buf, sz)) > 0) {
		tot += nrd;
		sz -= nrd;
		buf += nrd;
	}
	return nrd < 0 ? nrd : tot;
}

static bool
eb_fetch_lines_fd(expobuf_t eb)
{
	size_t bno = eb->idx;
	size_t eno = eb->tot - bno;

	if (eb->tot == 0) {
		return false;
	}

	/* memcpy left overs to the front
	 * we effectively divide the eb page into two halves and
	 * now we're swapping them:
	 * +----------+-+
	 * |pppppppppp|u|
	 * +----------+-+
	 * becomes:
	 * +-+----------+
	 * |u|nnnnnnnnnn|
	 * +-+----------+
	 * where p is processed, u is unprocessed and n is the
	 * new data we're about to read */
	if (LIKELY(bno > 0)) {
		memcpy(eb->data, eb->data + bno, eno);
	} else {
		bno = EB_PGSZ;
		eno = 0;
	}
	/* now read up to bno bytes */
	if ((eb->tot = read(eb->fd, eb->data + eno, bno)) == -1) {
		return false;
	}
	/* account for the stuff that has been there before */
	eb->tot += eno;
	/* set new target read rate */
	eb->pgno = 0;
	eb->idx = 0;
	return true;
}

static inline bool
eb_fetch_lines(expobuf_t eb)
{
	if (mmapable(eb->fd)) {
		return eb_fetch_lines_df(eb);
	} else {
		return eb_fetch_lines_fd(eb);
	}
}

static inline void
eb_unfetch_lines(expobuf_t eb)
{
	if (mmapable(eb->fd)) {
		munmap(eb->data, EB_PGSZ);
	}
	return;
}

static inline bool
eb_one_more_line_p(expobuf_t eb)
{
	if (mmapable(eb->fd)) {
		if (LIKELY(eb->pgno < eb->tot)) {
			if (LIKELY(EB_PGSZ - eb->idx > MAX_LINE_LEN)) {
				return true;
			}
			return memchr(eb->data + eb->idx, '\n',
				      EB_PGSZ - eb->idx);
		}
		/* bummer we're on the last page */
		return memchr(eb->data + eb->idx, '\n',
			      EB_PGSZ - (eb->pgno - eb->tot) - eb->idx);
	} else {
		return memchr(eb->data + eb->idx, '\n', eb->tot - eb->idx);
	}
}

static inline const char*
eb_current_line(expobuf_t eb)
{
	return &eb->data[eb->idx];
}

static inline void
eb_set_current_line_by_ptr(expobuf_t eb, const char *p)
{
	eb->idx = p - eb->data;
	return;
}

static inline void
eb_set_current_line_by_offs(expobuf_t eb, size_t len)
{
	eb->idx += len;
	return;
}

#endif	/* INCLUDED_expobuf_h_ */
