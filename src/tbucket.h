/*** tbucket.h -- tick buckets, to group by contract id
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

#if !defined INCLUDED_tbucket_h_
#define INCLUDED_tbucket_h_

typedef struct tbucket_s *tbucket_t;


/* bucket opers */
/* ctor, dtor */
static tbucket_t make_tbucket(size_t nbuckets);
static void free_tbucket(tbucket_t b);
/* modifier */
static inline void tbucket_rinse(tbucket_t b);
static inline void tbucket_flush(tbucket_t b);
static inline void tbucket_set_cid(tbucket_t b, cid_t cid);

static void
tbucket_add_tick(
	tbucket_t b, time_t ts,
	tick_type_t tt, m32_t p, q32_t q);

static void
tbucket_add_2ticks(
	tbucket_t b, time_t ts,
	tick_type_t tt1, m32_t p1, q32_t q1,
	tick_type_t tt2, m32_t p2, q32_t q2);

static void
tbucket_add_3ticks(
	tbucket_t b, time_t ts,
	tick_type_t tt1, m32_t p1, q32_t q1,
	tick_type_t tt2, m32_t p2, q32_t q2,
	tick_type_t tt3, m32_t p3, q32_t q3);

/* predicates, mainly for the tbcoll stuff */
static inline bool tbucket_rinsed_p(tbucket_t b);
static inline bool tbucket_cid_eq_p(tbucket_t b, cid_t cid);
/**
 * Return true if the tick bucket can take no more ticks. */
static inline bool tbucket_full_p(tbucket_t b);

/**
 * Dip a candle from ticks in B. */
static sushi_cmbcdl_t sushi_chandler(tbucket_t b);

#endif	/* INCLUDED_tbucket_h_ */
