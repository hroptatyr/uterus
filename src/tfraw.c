/* tenfore (morningstar) option quotes
 * SYMBOL \t TIMESTAMP \t MILLISECONDS \t SEQ \t PRICE \t SIZE \t TICKTYPE
 * The files are in chronological order. */

/* hope it's ours, the one that defines index_t */
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
//#include "pageio.h"
#include "prchunk.h"
#include "secu.h"
#include "date.h"
#include "sl1t.h"
#include "utefile.h"
#include "ute-mux.h"
#include "ute-print.h"

#if defined USE_DEBUGGING_ASSERTIONS || 1
# include <assert.h>
#endif	/* USE_DEBUGGING_ASSERTIONS */
#define DEFINE_GORY_STUFF
#include "m30.h"
#include "m62.h"

#if defined __INTEL_COMPILER
# pragma warning(disable:981)
#endif	/* __INTEL_COMPILER */
#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif	/* !UNLIKELY */
#if !defined UNUSED
# define UNUSED(_x)	__attribute__((unused)) _x
#endif	/* !UNUSED */

#define countof(x)	(sizeof(x) / sizeof(*x))

typedef uint8_t symidx_t;
typedef struct symtbl_s *symtbl_t;
/* ariva tick lines */
typedef struct tfraw_tl_s *tfraw_tl_t;

typedef enum {
	TT_UNKNOWN,
	TT_BID,
	TT_ASK,
	TT_TRA,
	TT_BIDASK,
	TT_BIDASKTRA,
} tick_type_t;

/* should coincide with slut_sym_t */
typedef char cid_t[32];

/* key is p for price, P for trade size, a for ask, A for ask size
 * b for bid, B for bid size and t for price stamp and T for BA stamp.
 * Made for type punning, tfraw_tl_s structs are also su_sl1t_t's */
struct tfraw_tl_s {
	struct scom_thdr_s t[1];

	struct {
		m30_t p;
		m30_t P;
	};
	struct {
		m30_t b;
		m30_t B;
	};
	struct {
		m30_t a;
		m30_t A;
	};
	m62_t v;
	m62_t oi;
	uint32_t seq;
	uint16_t tt;
	/* just the lowest bit is used, means bad tick */
	uint16_t flags;

	cid_t cid;
};

/* 'nother type extension */
#define NSYMS	(16384)
struct metrsymtbl_s {
	/* fuck ugly, this points into kernel space */
	utehdr_t st;
	/* caches */
	m30_t tra[NSYMS];
	m30_t tsz[NSYMS];
	m30_t bid[NSYMS];
	m30_t bsz[NSYMS];
	m30_t ask[NSYMS];
	m30_t asz[NSYMS];
};


static struct metrsymtbl_s symtbl;
/* convenience macroes for the additional tables */
#define SYMTBL_SEC	(symtbl.st->sec)
#define SYMTBL_TRA	(symtbl.tra)
#define SYMTBL_TSZ	(symtbl.tsz)
#define SYMTBL_BID	(symtbl.bid)
#define SYMTBL_BSZ	(symtbl.bsz)
#define SYMTBL_ASK	(symtbl.ask)
#define SYMTBL_ASZ	(symtbl.asz)
static size_t lno;

/* convenience bindings to access the sl1t_t slot in there */
static inline __attribute__((unused, pure)) uint32_t
tftl_ts_sec(const struct tfraw_tl_s *l)
{
	return scom_thdr_sec(AS_SCOM(l));
}

static inline __attribute__((unused)) void
tftl_set_ts_sec(tfraw_tl_t l, uint32_t s)
{
	return scom_thdr_set_sec(AS_SCOM_THDR_T(l), s);
}

static inline __attribute__((unused, pure)) uint16_t
tftl_ts_msec(const struct tfraw_tl_s *l)
{
	return scom_thdr_msec(AS_SCOM(l));
}

static inline __attribute__((unused)) void
tftl_set_ts_msec(tfraw_tl_t l, uint16_t s)
{
	return scom_thdr_set_msec(AS_SCOM_THDR_T(l), s);
}

static inline __attribute__((unused)) void
tftl_set_ts_from_sns(tfraw_tl_t l, const struct time_sns_s sns)
{
	scom_thdr_set_stmp_from_sns(AS_SCOM_THDR_T(l), sns);
	return;
}

static inline __attribute__((pure)) uint16_t
tftl_si(const struct tfraw_tl_s *l)
{
	return scom_thdr_tblidx(AS_SCOM(l));
}

static inline __attribute__((unused)) void
tftl_set_si(tfraw_tl_t l, uint16_t si)
{
	return scom_thdr_set_tblidx(AS_SCOM_THDR_T(l), si);
}

/* read buffer goodies */
static inline bool
fetch_lines(mux_ctx_t ctx)
{
	return !(prchunk_fill(ctx->rdr) < 0);
}

static inline bool
moar_ticks_p(mux_ctx_t ctx)
{
	return prchunk_haslinep(ctx->rdr);
}

/* printers */
static inline bool
enrich_batps(tfraw_tl_t t)
{
/* this will either cache the last seen price and size
 * or if only the size is seen and is different to the one before
 * this will set the last seen price, and if the size is identical
 * to what it was before this will erase the tick */
	int sl = tftl_si(t);

	if (t->p.v == 0 && t->P.v != 0) {
		/* last price is unknown but there was a last price tick?! */
#if 1
		char buf[32];
		pr_ts(buf, t->t->sec, ' ');
		fprintf(stderr, "spurious price tick: "
			"tra_p == 0, tra_q != 0, %s %s.000\n", t->cid, buf);
#else
		/* check if we repeat the tick */
		if (t->P.v == SYMTBL_TSZ[sl].v) {
			return false;
		}
		/* use the cached trade price */
		t->p = SYMTBL_TRA[sl];
		SYMTBL_TSZ[sl] = t->P;
#endif
	} else if (t->p.v != 0 && t->P.v == 0) {
		/* use the cached trade size?! */
		t->P = SYMTBL_TSZ[sl];
		SYMTBL_TRA[sl] = t->p;
	} else if (t->p.v != 0) {
		/* cache the trade price and size */
		SYMTBL_TRA[sl] = t->p;
		SYMTBL_TSZ[sl] = t->P;
	}

	if (t->b.v == 0 && t->B.v != 0) {
		/* check if we repeat the tick */
		if (t->B.v == SYMTBL_BSZ[sl].v) {
			return false;
		}
		/* use the cached bid price */
		t->b = SYMTBL_BID[sl];
		SYMTBL_BSZ[sl] = t->B;
	} else if (t->b.v != 0 && t->B.v == 0) {
		/* use the cached bid size?! */
		t->B = SYMTBL_BSZ[sl];
		SYMTBL_BID[sl] = t->b;
	} else if (t->b.v != 0) {
		/* cache the bid price */
		SYMTBL_BID[sl] = t->b;
		SYMTBL_BSZ[sl] = t->B;
	}

	if (t->a.v == 0 && t->A.v != 0) {
		/* check if we repeat the tick */
		if (t->A.v == SYMTBL_ASZ[sl].v) {
			return false;
		}
		/* otherwise use the cached ask price */
		t->a = SYMTBL_ASK[sl];
		SYMTBL_ASZ[sl] = t->A;
	} else if (t->a.v != 0 && t->A.v == 0) {
		/* use the cached ask size?! */
		t->A = SYMTBL_ASZ[sl];
		SYMTBL_ASK[sl] = t->a;
	} else if (t->a.v != 0) {
		/* cache the ask price */
		SYMTBL_ASK[sl] = t->a;
		SYMTBL_ASZ[sl] = t->A;
	}
	return true;
}

static inline bool
check_tfraw_tl(tfraw_tl_t t)
{
	int sl = tftl_si(t);

	/* look if this looks good */
	if (t->flags & 1) {
		return false;
	}
	if (!tftl_ts_sec(t)) {
		return false;
	}
	if (t->flags & 2) {
		/* HALTED */
		return true;
	}
	if (t->p.mant == 0 && t->P.mant == 0 &&
	    t->b.mant == 0 && t->B.mant == 0 &&
	    t->a.mant == 0 && t->A.mant == 0) {
		/* set HALTED */
		t->flags |= 2;
		/* reset our caches */
		SYMTBL_BID[sl].v = SYMTBL_ASK[sl].v = SYMTBL_TRA[sl].v = 0;
		SYMTBL_BSZ[sl].v = SYMTBL_ASZ[sl].v = SYMTBL_TSZ[sl].v = 0;
		return true;
	}
	/* bother our caches */
	return enrich_batps(t);
}


static zif_t z = NULL;

static char*
parse_symbol(tfraw_tl_t tgt, char *cursor)
{
	char *tmp = memchr(cursor, '|', 16);
	size_t tl = tmp - cursor;

	assert(tl > 0);
	memcpy(tgt->cid, cursor, tl);
	tgt->cid[tl] = '\0';
	return tmp + 1;
}

/**
 * tim kent's data tick line parser. */
static inline char*
tfraw_next_field(char *p, size_t l)
{
	char *tmp;
	if (LIKELY(p && (tmp = memchr(p, '|', l)) != NULL)) {
		return tmp + 1;
	}
	return NULL;
}

static inline time_t
parse_tfraw_time(const char *buf)
{
/* tim kent's format is %d/%m/%Y|%H:%M:%S:000 */
	struct tm tm[1] = {{0}};
	time_t ts;

	/* use our sped-up version, we expect one sep between date and time */
	ffff_strptime_dmY(buf, tm);
	ffff_strptime_HMS(buf + 11, tm);

	/* transform using the zone info in z */
	if (UNLIKELY(z == NULL)) {
		ts = ffff_timegm(tm);
	} else {
		ts = ffff_timelocal(tm, z);
	}
	return ts;
}

static bool
parse_prsz(tfraw_tl_t tgt, char *cursor)
{
	if (UNLIKELY(cursor[2] != '=')) {
		return false;
	}
	cursor += 3;
	switch (cursor[-2]) {
	default:
		break;
	case '2':
		/* last price */
		tgt->p = ffff_m30_get_s(&cursor);
		break;
	case '3':
		/* last size */
		tgt->P = ffff_m30_23_get_s(&cursor);
		break;
	case '4':
		/* bid price */
		tgt->b = ffff_m30_get_s(&cursor);
		break;
	case '5':
		/* bid size */
		tgt->B = ffff_m30_23_get_s(&cursor);
		break;
	case '6':
		/* ask price */
		tgt->a = ffff_m30_get_s(&cursor);
		break;
	case '7':
		/* ask size */
		tgt->A = ffff_m30_23_get_s(&cursor);
		break;
	}
	return true;
}

static int
parse_aux(tfraw_tl_t tgt, char *cursor)
{
	/* check for f16 (vol), f21 (oi), f33 (est. vol) */
	if (UNLIKELY(cursor[3] != '=')) {
		return false;
	}
	cursor += 4;
	if (cursor[-3] == '1' && cursor[-2] == '6') {
		/* volume tick */
		tgt->v = ffff_m62_get_s(&cursor);
	} else if (cursor[-3] == '2' && cursor[-2] == '1') {
		/* open interest tick */
		tgt->oi = ffff_m62_get_s(&cursor);
	} else if (cursor[-3] == '3' && cursor[-2] == '3') {
		/* volume tick */
		tgt->v = ffff_m62_get_s(&cursor);
	} else if (cursor[-3] == '3' && cursor[-2] == '4') {
		/* settlement */
		;
	}
	return true;
}

static bool
parse_tline(tfraw_tl_t tgt, char *line, size_t llen)
{
/* just assumes there is enough space */
	char *cursor = line;

#define NEXT_FIELD(_x)					\
	(_x = tfraw_next_field((_x), (line + llen) - (_x)))

	/* first off, check if it's a quote, 3rd field should be Q
	 * or T if we need trade data as well */
	if (LIKELY(line[24] == 'Q' || line[24] == 'T')) {
		tgt->tt = TT_BIDASKTRA;
	} else {
		/* make sure this line won't be counted */
		tgt->tt = TT_UNKNOWN;
		return false;
	}

	/* parse datetime field */
	tgt->t->sec = parse_tfraw_time(cursor);
	tgt->t->msec = 0;
	/* zap to the symbol field */
	cursor += 24;

	/* frobnicate the contract id */
	cursor += 2;
	/* get the symbol */
	cursor = parse_symbol(tgt, cursor);

	/* now comes a 201 and a 2 */
	NEXT_FIELD(cursor);

	while (NEXT_FIELD(cursor)) {
		/* we should now be right at the fix fields */
		if (UNLIKELY(cursor[0] != 'f')) {
			break;
		}

		/* we're only interested in f2,f3,f4,f5,f6,f7, so check
		 * if the field is just a single digit,
		 * then in a next step check if we see: f16 (volume)
		 * or f21 (open interest) */
		if (parse_prsz(tgt, cursor)) {
			;
		} else if (parse_aux(tgt, cursor)) {
			;
		}
	}
	return true;
}


static bool
read_line(mux_ctx_t ctx, tfraw_tl_t tl)
{
	char *line;
	size_t llen;
	bool res;

	llen = prchunk_getline(ctx->rdr, &line);
	lno++;
	if (!parse_tline(tl, line, llen)) {
		return false;
	}

	/* look up the symbol */
	tl->t->tblidx = ute_sym2idx(ctx->wrr, tl->cid);

	/* check if it's good or bad line */
	if (!(res = check_tfraw_tl(tl))) {
		/* bad */
		tl->flags |= 1;
		/* we should make this optional */
		//pr_tl(ctx->badfd, tl, line, llen);
	}
	return res;
}

static void
write_tick(mux_ctx_t ctx, tfraw_tl_t tl)
{
/* create one or more sparse ticks, sl1t_t objects */
	struct sl1t_s t[5];
	sl1t_t tp = t;

	if (tl->flags & 2 /*HALTED*/) {
		sl1t_copy_hdr(tp, (void*)tl);
		sl1t_mark_halted(tp);
		tp->tra = 0;
		tp->tsz = 0;
		tp++;
		goto nomore;
	}

	if (tl->p.mant) {
		sl1t_copy_hdr(tp, (void*)tl);
		sl1t_set_ttf(tp, SL1T_TTF_TRA);
		tp->tra = tl->p.v;
		tp->tsz = tl->P.v;
		tp++;
	}

	if (tl->b.mant) {
		sl1t_copy_hdr(tp, (void*)tl);
		sl1t_set_ttf(tp, SL1T_TTF_BID);
		tp->bid = tl->b.v;
		tp->bsz = tl->B.v;
		tp++;
	}

	if (tl->a.mant) {
		sl1t_copy_hdr(tp, (void*)tl);
		sl1t_set_ttf(tp, SL1T_TTF_ASK);
		tp->ask = tl->a.v;
		tp->asz = tl->A.v;
		tp++;
	}

	if (tl->v.mant) {
		sl1t_copy_hdr(tp, (void*)tl);
		sl1t_set_ttf(tp, SL1T_TTF_VOL);
		tp->w[0] = tl->v.v;
		tp++;
	}
	if (tl->oi.mant) {
		sl1t_copy_hdr(tp, (void*)tl);
		sl1t_set_ttf(tp, SL1T_TTF_OI);
		tp->w[0] = tl->oi.v;
		tp++;
	}

nomore:
	for (int i = 0; i < (tp - t); i++) {
		ute_add_tick(ctx->wrr, AS_SCOM(t + i));
	}
	return;
}

static void
read_lines(mux_ctx_t ctx)
{
	while (moar_ticks_p(ctx)) {
		struct tfraw_tl_s tl[1];
		memset(tl, 0, sizeof(*tl));
		if (read_line(ctx, tl)) {
			write_tick(ctx, tl);
		}
	}
	return;
}

static const char tfraw_zone[] = "Europe/Berlin";

void
tfraw_slab(mux_ctx_t ctx)
{
	/* open our timezone definition */
	if (ctx->opts->zone != NULL) {
		z = zif_read_inst(ctx->opts->zone);
	} else {
		z = zif_read_inst(tfraw_zone);
	}
	/* init reader, we use prchunk here */
	ctx->rdr = init_prchunk(ctx->infd);
	/* main loop */
	lno = 0;
	while (fetch_lines(ctx)) {
		read_lines(ctx);
	}
	/* expobuf is the reader of our choice */
	free_prchunk(ctx->rdr);
	if (z != NULL) {
		zif_free(z);
	}
	return;
}

/* tfraw.c ends here */
