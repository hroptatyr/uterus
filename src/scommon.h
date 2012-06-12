/*** scommon.h -- common definitions for sparse storage
 *
 * Copyright (C) 2009-2012 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@ga-group.nl>
 *
 * This file is part of uterus.
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

#if !defined INCLUDED_scommon_h_
#define INCLUDED_scommon_h_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct time_range_s *time_range_t;
typedef union scom_thdr_u scidx_t;
typedef union scom_thdr_u *scom_thdr_t;
#define AS_SCOM_THDR(x)		((scom_thdr_t)(x))
typedef const union scom_thdr_u *scom_t;
#define AS_SCOM(x)		((scom_t)(x))

typedef const struct sndwch_s *sndwch_t;

/* this time baloney belongs to somewhere else */
struct time_sms_s {
	uint32_t sec;
	uint16_t msec:10;
};

struct time_sns_s {
	uint32_t sec;
	uint32_t nsec;
};

struct time_range_s {
	uint32_t lo;
	uint32_t hi;
};

union scom_thdr_u {
	uint64_t u;
	/* struct in order of significance */
	struct {
#if defined WORDS_BIGENDIAN
		/* secs first */
		uint32_t sec;
		/* millisecs is the standard these days */
		uint32_t msec:10;
		/* an index back into the symtbl */
		uint32_t idx:16;
		/* tick type and flags */
		uint32_t ttf:6;
#else  /* !WORDS_BIGENDIAN */
		/* tick type and flags */
		uint32_t ttf:6;
		/* an index back into the symtbl */
		uint32_t idx:16;
		/* millisecs is the standard these days */
		uint32_t msec:10;
		/* +64 */
		uint32_t sec;
#endif	/* WORDS_BIGENDIAN */
	} __attribute__((packed));

	/* microsecond resolution, can only store up to 64 securities */
	struct {
#if defined WORDS_BIGENDIAN
		/* +64 */
		uint32_t sec;
		/* microseconds */
		uint32_t usec:20;
		/* an index back into the symtbl */
		uint32_t idx:6;
		/* tick type and flags */
		uint32_t ttf:6;
#else  /* !WORDS_BIGENDIAN */
		/* tick type and flags */
		uint32_t ttf:6;
		/* an index back into the symtbl */
		uint32_t idx:6;
		/* microseconds */
		uint32_t usec:20;
		/* +64 */
		uint32_t sec;
#endif	/* WORDS_BIGENDIAN */
	} __attribute__((packed)) us;

	/* nanosecond resolution, can only store one security and
	 * only the tick types bid, ask and tra */
	struct {
#if defined WORDS_BIGENDIAN
		/* +64 */
		uint32_t sec;
		/* nano-seconds */
		uint32_t nsec:30;
		/* tick type and flags */
		uint32_t ttf:2;
#else  /* !WORDS_BIGENDIAN */
		/* tick type and flags */
		uint32_t ttf:2;
		/* nano-seconds */
		uint32_t nsec:30;
		/* +64 */
		uint32_t sec;
#endif	/* WORDS_BIGENDIAN */
	} __attribute__((packed)) ns;

	/* seeing as that we're a union now ...
	 * stuff this thing with the 0.1 version of the header */
	struct {
#if defined WORDS_BIGENDIAN
		/* an index back into the symtbl */
		uint16_t idx;
		/* tick type and flags */
		uint16_t ttf:6;
		/* millisecs is the standard these days */
		uint16_t msec:10;
		/* +64 */
		uint32_t sec;
#else  /* !WORDS_BIGENDIAN */
		/* +64 */
		uint32_t sec;
		/* millisecs is the standard these days */
		uint16_t msec:10;
		/* tick type and flags */
		uint16_t ttf:6;
		/* an index back into the symtbl */
		uint16_t idx;
#endif	/* WORDS_BIGENDIAN */
	} __attribute__((packed)) v01;
} __attribute__((transparent_union));

/* just so nobody has to include sl1t.h to get the sandwich size */
struct sndwch_s {
	/** keying info */
	uint64_t key;
	/** satellite */
	uint64_t sat;
};

/** an indexer type to refer to the i-th sandwich in a pile */
typedef uint64_t sidx_t;


/* accessors */
static inline __attribute__((pure)) uint32_t
scom_thdr_sec(scom_t h)
{
	return h->sec;
}

static inline void
scom_thdr_set_sec(scom_thdr_t h, uint32_t sec)
{
	h->sec = sec;
	return;
}

static inline __attribute__((pure)) uint16_t
scom_thdr_msec(scom_t h)
{
/* we ought to cleanse this one, there are special values for the msec slot */
	return (uint16_t)h->msec;
}

static inline void
scom_thdr_set_msec(scom_thdr_t h, uint16_t msec)
{
/* check this? */
	h->msec = msec;
	return;
}

static inline __attribute__((pure)) uint16_t
scom_thdr_tblidx(scom_t h)
{
	return (uint16_t)h->idx;
}

static inline void
scom_thdr_set_tblidx(scom_thdr_t h, uint16_t idx)
{
	h->idx = idx;
	return;
}


/* ttf, tick type, flavour and flags */
#define SCOM_TTF_UNK	0
/* the next 3 consist of price and size, ui32 each */
#define SL1T_TTF_BID	1
#define SL1T_TTF_ASK	2
#define SL1T_TTF_TRA	3
/* these consist only of price, the second ui32 slot is 0 */
#define SL1T_TTF_FIX	4
/* settlement, is 'x' in char representations for exercise */
#define SL1T_TTF_STL	5
/* auction price offer, is 'k' in char representations */
#define SL1T_TTF_AUC	6
/* little `candle', just a bid and an ask price */
#define SL1T_TTF_BIDASK	7
/* 64 bit values start here, these aren't real ticks as their
 * values could actually be computed */
/* sheer volume, i.e. number of securites traded, is 'v' in char repr */
#define SL1T_TTF_VOL	8
/* price volume, i.e. number of securities times their price, 'V' */
#define SL1T_TTF_VPR	9
/* vwap, VPR by VOL, may be used to save transmission of VOL and VPR */
#define SL1T_TTF_VWP	10
/* split, a tick that denotes a ratio used for splitting securities */
#define SL1T_TTF_SPL	11
/* oi, open interest tick, the number of all open long contracts */
#define SL1T_TTF_OI	12

/* level2 ticks
 * level2 works just like level1 but is edge triggered, so to build a
 * position in the depth, send a price (determines the slot to build)
 * and a quantity.  To free that slot, send the price to free and
 * quantity 0. */
#define SL2T_TTF_BID	13
#define SL2T_TTF_ASK	14

/* flag that denotes double tick size, used for candles et al. */
#define SCOM_FLAG_LM	16
/* flag that denotes quadruple tick size(?) used for level2 ticks */
#define SCOM_FLAG_L2M	32

static inline __attribute__((pure)) uint16_t
scom_thdr_ttf(scom_t h)
{
	return (uint16_t)h->ttf;
}

static inline void
scom_thdr_set_ttf(scom_thdr_t h, uint16_t ttf)
{
	h->ttf = ttf;
	return;
}

static inline bool
scom_thdr_linked(scom_t h)
{
	return h->ttf & SCOM_FLAG_LM;
}

static inline void
scom_thdr_set_linked(scom_thdr_t h)
{
	h->ttf |= SCOM_FLAG_LM;
	return;
}

static inline void
scom_thdr_unset_linked(scom_thdr_t h)
{
	h->ttf &= ~SCOM_FLAG_LM;
	return;
}


/* weird stuff */
static inline void
scom_thdr_set_stmp_from_sns(scom_thdr_t h, const struct time_sns_s sns)
{
	uint32_t nsec = sns.nsec / 1000000;
	h->sec = sns.sec;
	h->msec = (uint16_t)nsec;
	return;
}

static inline void
scom_thdr_set_stmp_from_sms(scom_thdr_t h, const struct time_sms_s sms)
{
	h->sec = sms.sec;
	h->msec = sms.msec;
	return;
}

/* ctor */
static inline void
scom_thdr_fill(scom_thdr_t h, uint32_t sec, uint16_t msec, uint8_t idx)
{
	scom_thdr_set_sec(h, sec);
	scom_thdr_set_msec(h, msec);
	scom_thdr_set_tblidx(h, idx);
	return;
}

static inline void
scom_thdr_copy(scom_thdr_t tgt, scom_t src)
{
	*tgt = *src;
	return;
}


/* msec overloading */
#define SCOM_NEXIST	(1023)
/** market continues
 * The idea is to be sorted to the end of a second. */
#define SCOM_CONT	(1022)
/** market halts
 * The idea is to be sorted to the end of a second. */
#define SCOM_HALT	(1021)
#define SCOM_PADDING	(1020)
#define SCOM_MSEC_VALI	(1000)

static inline __attribute__((pure)) bool
scom_thdr_nexist_p(scom_t h)
{
	return scom_thdr_msec(h) == SCOM_NEXIST;
}

static inline __attribute__((const, pure)) bool
scom_thdr_cont_p(scom_t h)
{
	return scom_thdr_msec(h) == SCOM_CONT;
}

static inline __attribute__((const, pure)) bool
scom_thdr_halt_p(scom_t h)
{
	return scom_thdr_msec(h) == SCOM_HALT;
}

static inline __attribute__((pure)) bool
scom_thdr_padding_p(scom_t h)
{
	return scom_thdr_msec(h) == SCOM_PADDING;
}

/**
 * Return true if tick actually contains tick data, as opposed to mere
 * control or flow data. */
static inline __attribute__((pure)) bool
scom_thdr_proper_p(scom_t t)
{
	return scom_thdr_msec(t) < SCOM_MSEC_VALI;
}

/* setters */
static inline void
scom_thdr_mark_nexist(scom_thdr_t t)
{
	scom_thdr_set_msec(t, SCOM_NEXIST);
	return;
}

static inline void
scom_thdr_mark_cont(scom_thdr_t t)
{
	scom_thdr_set_msec(t, SCOM_CONT);
	return;
}

static inline void
scom_thdr_mark_halt(scom_thdr_t t)
{
	scom_thdr_set_msec(t, SCOM_HALT);
	return;
}

static inline void
scom_thdr_mark_padding(scom_thdr_t t)
{
	scom_thdr_set_msec(t, SCOM_PADDING);
	return;
}


/* helpers for variadically-sized ticks */
/**
 * Return the size of T in ticks (multiples of struct sndwch_s). */
static inline __attribute__((pure)) size_t
scom_tick_size(scom_t t)
{
	if (!(scom_thdr_ttf(t) & (SCOM_FLAG_LM | SCOM_FLAG_L2M))) {
		return 1UL;
	} else if (scom_thdr_ttf(t) & SCOM_FLAG_LM) {
		return 2UL;
	} else if (scom_thdr_ttf(t) & SCOM_FLAG_L2M) {
		return 4UL;
	} else {
		return 0UL;
	}
}

/**
 * Return the size of T in bytes. */
static inline __attribute__((pure)) size_t
scom_byte_size(scom_t t)
{
	return scom_tick_size(t) * sizeof(struct sndwch_s);
}


/* promotion for ute versions */
static inline void
scom_promote_v01(scom_thdr_t tgt, scom_t t)
{
/* given a v01 scom T return the current version's idea of the scom in TGT */
	union scom_thdr_u res;
#if !defined HAVE_ANON_STRUCTS || 1
	res.sec = t->v01.sec;
	res.msec = t->v01.msec;
	res.idx = t->v01.idx;
	res.ttf = t->v01.ttf;
#endif	/* !HAVE_ANON_STRUCTS || 1 */
	*tgt = res;
	return;
}

static inline scidx_t
make_scidx(scom_t t)
{
	scidx_t res;
	res.u = t->u;
	return res;
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* INCLUDED_scommon_h_ */
