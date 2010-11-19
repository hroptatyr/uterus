/*** tbcoll.h -- collection of tick buckets
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

#if !defined INCLUDED_tbcoll_h_
#define INCLUDED_tbcoll_h_

#include "sushi.h"

#if !defined MAX_CONTRACTS
# error "Need to define MAX_CONTRACTS first."
#endif	/* !MAX_CONTRACTS */

typedef void *tbcoll_t;


/* bucket collection opers */
static void init_tbcoll(tbcoll_t tbc, size_t bucket_size);
static void fini_tbcoll(tbcoll_t tbc);

static inline time_t tbcoll_current_stamp(tbcoll_t tbc);
static inline tbucket_t *tbcoll_buckets(tbcoll_t tbc);
static inline void tbcoll_rinse_buckets(tbcoll_t tbc);
static inline size_t tbcoll_nunrinsed_buckets(tbcoll_t tbc);

static inline void tbcoll_set_current_stamp(tbcoll_t tbc, time_t ts);

/* normally extern, but for speed's sake ... */
static tbucket_t
tbcoll_find_bucket(tbcoll_t tbc, cid_t cid);
static tbucket_t
tbcoll_find_create_bucket(tbcoll_t tbc, cid_t cid);

#endif	/* INCLUDED_tbcoll_h_ */
