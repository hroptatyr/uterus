/*** tbcoll.c -- collection of tick buckets
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

#include "tbucket.h"
#include "tbcoll.h"

#if !defined MAX_CONTRACTS
# error "Need to define MAX_CONTRACTS first."
#endif	/* !MAX_CONTRACTS */

#if MAX_CONTRACTS <= 128
typedef uint8_t slot_t;
typedef uint8_t slfl_t;
#elif MAX_CONTRACTS <= 256
typedef uint8_t slot_t;
typedef uint16_t slfl_t;
#elif MAX_CONTRACTS <= 32768
typedef uint16_t slot_t;
typedef uint16_t slfl_t;
#elif MAX_CONTRACTS <= 65536
typedef uint16_t slot_t;
typedef uint32_t slfl_t;
#else
typedef uint32_t slot_t;
typedef uint32_t slfl_t;
#endif

struct tbcoll_s {
	time_t cur_ts;
	size_t bcktsz;
	index_t nbuckets;
	slot_t buckidx[MAX_CONTRACTS];
	tbucket_t buckets[MAX_CONTRACTS];
};

#define TBCOLL(_x)		((struct tbcoll_s*)_x)

#if !defined m32_t
# define m32_t			monetary32_t
#endif	/* !m32_t */
#if !defined q32_t
# define q32_t			quantity32_t
#endif	/* !q32_t */


/* bucket collection mumbo-jumbo */
/* ctor, dtor */
static void
init_tbcoll(tbcoll_t tbc, size_t bucket_size)
{
	memset(tbc, 0, sizeof(*TBCOLL(tbc)));
	TBCOLL(tbc)->bcktsz = bucket_size;
	return;
}

static void
fini_tbcoll(tbcoll_t tbc)
{
	for (index_t i = 0; i < MAX_CONTRACTS; i++) {
		free_tbucket(TBCOLL(tbc)->buckets[i]);
	}
	return;
}

/* accessors */
static inline tbucket_t*
tbcoll_buckets(tbcoll_t tbc)
{
	return TBCOLL(tbc)->buckets;
}

static inline time_t
tbcoll_current_stamp(tbcoll_t tbc)
{
	return TBCOLL(tbc)->cur_ts;
}

static inline void
tbcoll_set_current_stamp(tbcoll_t tbc, time_t ts)
{
	TBCOLL(tbc)->cur_ts = ts;
	return;
}

static inline void
tbcoll_rinse_buckets(tbcoll_t tbc)
{
#if defined RINSE_THOROUGHLY
	for (index_t i = 0; i < MAX_CONTRACTS; i++) {
		tbucket_t b = tbcoll_buckets(tbc)[i];
		if (b != NULL) {
			tbucket_rinse(b);
		}
	}
#else  /* !RINSE_THOROUGHLY */
/* only rinse buckets which we know were in use */
	for (index_t i = 0; i < TBCOLL(tbc)->nbuckets; i++) {
		slot_t s = TBCOLL(tbc)->buckidx[i];
		tbucket_t b = TBCOLL(tbc)->buckets[s];
		tbucket_rinse(b);
	}
#endif	/* RINSE_THOROUGHLY */
	TBCOLL(tbc)->nbuckets = 0;
	return;
}

static inline size_t
tbcoll_nunrinsed_buckets(tbcoll_t tbc)
{
	return TBCOLL(tbc)->nbuckets;
}


/* state-of-the-art hash seems to be murmur2, so let's adapt it for gaids */
static uint32_t
murmur2(cid_t cid)
{
	/* 'm' and 'r' are mixing constants generated offline.
	 * They're not really 'magic', they just happen to work well. */
	const unsigned int m = 0x5bd1e995;
	const int r = 24;
	/* initialise to a random value, originally passed as `seed'*/
#define seed	137173456
	unsigned int h = seed;
	uint32_t key =
		((uint32_t*)cid)[0] ^ ((uint32_t*)cid)[1] ^ ((uint32_t*)cid)[2];

	key *= m;
	key ^= key >> r;
	key *= m;

	h *= m;
	h ^= key;

	/* Do a few final mixes of the hash to ensure the last few
	 * bytes are well-incorporated. */
	h ^= h >> 13;
	h *= m;
	h ^= h >> 15;
	return h;
}

static inline bool
__bucket_empty_p(tbucket_t b)
{
	return b == NULL || tbucket_rinsed_p(b);
}

static inline slfl_t
__bckt_index_empty(slot_t idx)
{
	unsigned int tmp = (idx << 1) | 1;
	return (slfl_t)tmp;
}

static inline slfl_t
__bckt_index_full(slot_t idx)
{
	unsigned int tmp = idx << 1;
	return (slfl_t)tmp;
}

static inline bool
__bckt_index_empty_p(slfl_t s)
{
	return s & 1;
}

static inline slot_t
__bckt_index_recover(slfl_t s)
{
	return (slot_t)(s >> 1);
}

static inline slfl_t
__slot(tbcoll_t tbc, cid_t cid)
{
/* return the first slot in c->keys that either contains gaid or would
 * be a warm n cuddly place for it */
	slot_t res = (slot_t)(murmur2(cid) % MAX_CONTRACTS);

#define RETURN_IF_MATCHES						\
	if (__bucket_empty_p(tbcoll_buckets(tbc)[i])) {			\
		return __bckt_index_empty(i);				\
	} else if (tbucket_cid_eq_p(tbcoll_buckets(tbc)[i], cid)) {	\
		return __bckt_index_full(i);				\
	}

	for (slot_t i = res; i < MAX_CONTRACTS; i++) {
		RETURN_IF_MATCHES;
	}
	/* rotate round */
	for (slot_t i = 0; i < res; i++) {
		RETURN_IF_MATCHES;
	}
#undef RETURN_IF_MATCHES
	/* means we're full :O */
	fputs("hash table full\n", stderr);
	abort();
	return (slfl_t)-1;
}

static inline tbucket_t
__massage_bucket(tbcoll_t tbc, slfl_t s)
{
/* registers the bucket in our buckidx array and incs nbuckets slot */
	slot_t idx = __bckt_index_recover(s);
	tbucket_t res = tbcoll_buckets(tbc)[idx];

	if (__bckt_index_empty_p(s)) {
		TBCOLL(tbc)->buckidx[TBCOLL(tbc)->nbuckets++] = idx;
		if (res == NULL) {
			res = TBCOLL(tbc)->buckets[idx] =
				make_tbucket(TBCOLL(tbc)->bcktsz);
		}
	}
	return res;
}

static tbucket_t __attribute__((unused))
tbcoll_find_create_bucket(tbcoll_t tbc, cid_t cid)
{
	slfl_t s = __slot(tbc, cid);
	tbucket_t res = __massage_bucket(tbc, s);
	tbucket_set_cid(res, cid);
	return res;
}

static tbucket_t __attribute__((unused))
tbcoll_find_bucket(tbcoll_t tbc, cid_t cid)
{
	slfl_t slot = __slot(tbc, cid);

	if (__bckt_index_empty_p(slot)) {
		return NULL;
	} else {
		slot_t rec = __bckt_index_recover(slot);
		return tbcoll_buckets(tbc)[rec];
	}
}

static inline slot_t
__bcktidx(tbcoll_t tbc, slot_t i)
{
	return TBCOLL(tbc)->buckidx[i];
}

static inline tbucket_t
__bckt_from_idx(tbcoll_t tbc, slot_t idx)
{
	slot_t i = __bcktidx(tbc, idx);
	return tbcoll_buckets(tbc)[i];
}

static inline size_t
__nbuckets(tbcoll_t tbc)
{
	return TBCOLL(tbc)->nbuckets;
}

#define FOREACH_TBCOLL_BUCKET(_tbc, _x)					\
	for (slot_t __PORNO__ = 0;					\
	     __PORNO__ < __nbuckets(_tbc); __PORNO__++) {		\
	tbucket_t _x = __bckt_from_idx(_tbc, __PORNO__);

#define END_FOREACH_TBCOLL_BUCKET	}

#undef TBCOLL

/* tbcoll.c ends here */
