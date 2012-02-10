/*** scommon.h -- common definitions for sparse storage
 *
 * Copyright (C) 2009 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <sebastian.freundt@ga-group.nl>
 *
 * This file is part of sushi.
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
typedef struct scom_thdr_s *scom_thdr_t;
#define AS_SCOM_THDR_T(x)	((scom_thdr_t)(x))
typedef const struct scom_thdr_s *scom_t;
#define AS_SCOM(x)		((scom_t)(x))

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

struct scom_thdr_s {
	/* +64 */
	uint32_t sec;
	/* millisecs is the standard these days */
	uint16_t msec:10;
	/* flags and shite */
	uint16_t ttf:6;
	/* an index back into the symtbl */
	uint16_t tblidx:16;
} __attribute__((packed));


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
	return h->msec;
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
	return h->tblidx;
}

static inline void
scom_thdr_set_tblidx(scom_thdr_t h, uint16_t idx)
{
	h->tblidx = idx;
	return;
}


/* ttf, tick type, flavour and flags */
#define SL1T_TTF_UNK	0
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

/* flag that denotes double tick size, used for candles et al. */
#define SCOM_FLAG_LM	16
/* flag that denotes quadruple tick size(?) used for level2 ticks */
#define SCOM_FLAG_L2M	32

static inline __attribute__((pure)) uint16_t
scom_thdr_ttf(scom_t h)
{
	return h->ttf;
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

static inline size_t
scom_size(scom_t t)
{
	return !scom_thdr_linked(t) ? 1 : 2;
}

static inline size_t
scom_byte_size(scom_t t)
{
	return scom_size(t) * 4 * sizeof(uint32_t);
}


/* msec overloading */
#define SCOM_NEXIST	(1023)
/* 1022 is reserved for unserding, means ONHOLD there */
#define SCOM_HALTED	(1021)
#define SCOM_PADDING	(1020)
#define SCOM_MSEC_VALI	(1000)

static inline __attribute__((pure)) bool
scom_thdr_nexist_p(scom_t h)
{
	return scom_thdr_msec(h) == SCOM_NEXIST;
}

static inline __attribute__((pure)) bool
scom_thdr_halted_p(scom_t h)
{
	return scom_thdr_msec(h) == SCOM_HALTED;
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
scom_thdr_mark_halted(scom_thdr_t t)
{
	scom_thdr_set_msec(t, SCOM_HALTED);
	return;
}

static inline void
scom_thdr_mark_padding(scom_thdr_t t)
{
	scom_thdr_set_msec(t, SCOM_PADDING);
	return;
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* INCLUDED_scommon_h_ */
