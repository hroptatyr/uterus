/* ib rtickitems as defined by Rudi
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
#include "pageio.h"
#include "secu.h"
#include "date.h"
#include "sl1t.h"
#include "utefile.h"
#include "sumux.h"

#if defined USE_DEBUGGING_ASSERTIONS
# include <assert.h>
#endif	/* USE_DEBUGGING_ASSERTIONS */
#define DEFINE_GORY_STUFF
#include "m30.h"
#include "m62.h"

#define MAX_LINE_LEN	512
#define countof(x)	(sizeof(x) / sizeof(*x))

/* should coincide with slut_sym_t */
typedef char cid_t[32];

typedef uint8_t symidx_t;
typedef struct symtbl_s *symtbl_t;
/* ariva tick lines */
typedef struct ibrti_tl_s *ibrti_tl_t;

/* key is p for price, P for trade size, a for ask, A for ask size
 * b for bid, B for bid size and t for price stamp and T for BA stamp.
 * Made for type punning, ibrti_tl_s structs are also su_sl1t_t's */
struct ibrti_tl_s {
	struct scom_thdr_s t[1];
	/* not so standard fields now */
	uint32_t offs;
	uint32_t stmp2;
	uint16_t msec2;

	union {
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
ibtl_ts_sec(const struct ibrti_tl_s *l)
{
	return scom_thdr_sec(AS_SCOM(l));
}

static inline __attribute__((unused)) void
ibtl_set_ts_sec(ibrti_tl_t l, uint32_t s)
{
	return scom_thdr_set_sec(AS_SCOM_THDR_T(l), s);
}

static inline __attribute__((unused, pure)) uint16_t
ibtl_ts_msec(const struct ibrti_tl_s *l)
{
	return scom_thdr_msec(AS_SCOM(l));
}

static inline __attribute__((unused)) void
ibtl_set_ts_msec(ibrti_tl_t l, uint16_t s)
{
	return scom_thdr_set_msec(AS_SCOM_THDR_T(l), s);
}

static inline __attribute__((unused)) void
ibtl_set_ts_from_sns(ibrti_tl_t l, const struct time_sns_s sns)
{
	scom_thdr_set_stmp_from_sns(AS_SCOM_THDR_T(l), sns);
	return;
}

static inline __attribute__((pure)) uint16_t
ibtl_si(const struct ibrti_tl_s *l)
{
	return scom_thdr_tblidx(AS_SCOM(l));
}

static inline __attribute__((unused)) void
ibtl_set_si(ibrti_tl_t l, uint16_t si)
{
	return scom_thdr_set_tblidx(AS_SCOM_THDR_T(l), si);
}

static inline __attribute__((pure)) uint16_t
ibtl_ttf(const struct ibrti_tl_s *l)
{
	return l->tt;
}

static inline __attribute__((unused)) void
ibtl_set_ttf(ibrti_tl_t l, uint16_t tt)
{
	l->tt = tt;
	return;
}

/* read buffer goodies */
static inline bool
fetch_lines(mux_ctx_t ctx)
{
	return prdb_fetch_lines(ctx->rdr);
}

static inline void
unfetch_lines(mux_ctx_t ctx)
{
	prdb_unfetch_lines(ctx->rdr);
	return;
}

static inline bool
moar_ticks_p(mux_ctx_t ctx)
{
	return prdb_one_more_line_p(ctx->rdr);
}

static inline void
check_tic_stmp(ibrti_tl_t tic)
{
	ibtl_set_ts_sec(tic, tic->stmp2);
	ibtl_set_ts_msec(tic, tic->msec2);
	return;
}

static inline void
check_tic_offs(ibrti_tl_t UNUSED(t))
{
	return;
}

static inline bool
enrich_batps(ibrti_tl_t t)
{
/* this will either cache the last seen price and size
 * or if only the size is seen and is different to the one before
 * this will set the last seen price, and if the size is identical
 * to what it was before this will erase the tick */
	int sl = ibtl_si(t);
	switch (ibtl_ttf(t)) {
	case SL1T_TTF_TRA:
		if (t->p.v == 0 && t->P.v != 0) {
			/* check if we repeat the tick */
			if (t->P.v == SYMTBL_TSZ[sl].v) {
				return false;
			}
			/* use the cached trade price */
			t->p = SYMTBL_TRA[sl];
		} else if (t->p.v != 0) {
			/* cache the trade price */
			SYMTBL_TRA[sl] = t->p;
			SYMTBL_TSZ[sl] = t->P;
		}
		break;
	case SL1T_TTF_BID:
		if (t->b.v == 0 && t->B.v != 0) {
			/* check if we repeat the tick */
			if (t->B.v == SYMTBL_BSZ[sl].v) {
				return false;
			}
			/* use the cached bid price */
			t->b = SYMTBL_BID[sl];
		} else if (t->b.v != 0) {
			/* cache the bid price */
			SYMTBL_BID[sl] = t->b;
			SYMTBL_BSZ[sl] = t->B;
		}
		break;
	case SL1T_TTF_ASK:
		if (t->a.v == 0 && t->A.v != 0) {
			/* check if we repeat the tick */
			if (t->A.v == SYMTBL_ASZ[sl].v) {
				return false;
			}
			/* otherwise use the cached ask price */
			t->a = SYMTBL_ASK[sl];
		} else if (t->a.v != 0) {
			/* cache the ask price */
			SYMTBL_ASK[sl] = t->a;
			SYMTBL_ASZ[sl] = t->A;
		}
		break;
	default:
		/* do nothing */
		break;
	}
	return true;
}

static inline bool
check_ibrti_tl(ibrti_tl_t t)
{
	/* look if this looks good */
	if (t->flags & 1) {
		return false;
	}
	if (!ibtl_ts_sec(t)) {
		return false;
	}
	if (t->flags & 2) {
		/* HALTED */
		return true;
	}
	if (t->p.v == 0 && t->P.v == 0) {
		return false;
	}
	if (ibtl_ttf(t) == SL1T_TTF_UNK) {
		return false;
	}
	/* bother our caches */
	return enrich_batps(t);
}


static zif_t z = NULL;

static inline time_t
parse_time(const char *buf)
{
	struct tm tm;
	time_t ts;

	/* use our sped-up version */
	ffff_strptime(buf, &tm);
	if (UNLIKELY(z == NULL)) {
		ts = ffff_timegm(&tm);
	} else {
		ts = ffff_timelocal(&tm, z);
	}
	return ts;
}

#if defined USE_DEBUGGING_ASSERTIONS
static __attribute__((unused)) void
fputn(FILE *whither, const char *p, size_t n)
{
	for (const char *endp = p + n; p < endp; p++) {
		fputc(*p, whither);
	}
	return;
}
#endif

static const char*
parse_symbol(ibrti_tl_t tgt, const char *cursor)
{
	char *tmp = memchr(cursor, '\t', 32);
	size_t tl = tmp - cursor;

#if defined USE_DEBUGGING_ASSERTIONS
	assert(tl > 0);
	assert(*tmp == '\t');
#endif	/* USE_DEBUGGING_ASSERTIONS */

	memcpy(tgt->cid, cursor, tl);
	tgt->cid[tl] = '\0';
	/* put cursor after tab char */
	return tmp + 1;
}

static inline uint16_t
parse_rTI_tick_type(char buf)
{
	switch (buf) {
	case '1':
		return SL1T_TTF_BID;
	case '2':
		return SL1T_TTF_ASK;
	case '3':
		return SL1T_TTF_TRA;
	case '8':
		return SL1T_TTF_VOL;
	default:
		return SL1T_TTF_UNK;
	}
}

static const char*
parse_tline(ibrti_tl_t tgt, const char *line  )
{
/* just assumes there is enough space */
	const char *cursor = line;
	uint32_t v1, v2;

#define NEXT_TAB(_x)	while (*_x++ != '\t')
#define PARSE_TIME(_tgt, _cursor, _line)	\
	_tgt = parse_time(_cursor);		\
	NEXT_TAB(_cursor)
#if defined USE_ASSERTIONS
#define PARSE_SEQ(_tgt, _cursor, _line)			\
	/* read seq, as it is used to assure order */	\
	_tgt = ffff_strtol(_cursor, &_cursor, 0);	\
	_cursor++
#else  /* !USE_ASSERTIONS */
#define PARSE_SEQ(_tgt, _cursor, _line)		\
	/* skip seq */				\
	NEXT_TAB(_cursor)
#endif	/* USE_ASSERTIONS */
#define PARSE_MSEC(_tgt, _cursor, _line)			\
	/* read msec, as it is used to assure order */		\
	_tgt = (uint16_t)ffff_strtol(_cursor, &_cursor, 0);	\
	_cursor++
#define PARSE_TT(_tgt, _cursor, _line)				\
	if (LIKELY(_cursor[1] == '\n')) {			\
		_tgt = parse_rTI_tick_type(_cursor[0]);		\
		_cursor++;					\
	} else if (LIKELY(_cursor[2] == '\n')) {		\
		_tgt = SL1T_TTF_UNK;				\
		_cursor += 2;					\
	} else {						\
		_tgt = SL1T_TTF_UNK;				\
	}
#define RDPRI(_into, _else)						\
	/* care about \N */						\
	if (cursor[0] != '\\' /* || cursor[1] == 'N' */) {		\
		m30_t tmp = ffff_m30_get_s((char**)&cursor);		\
		_into = tmp.v;						\
	} else {							\
		_else;							\
	}								\
	while (*cursor++ != '\t')
#define RDQTY(_into, _else)						\
	/* care about \N */						\
	if (cursor[0] != '\\' /* || cursor[1] == 'N' */) {		\
		const char *cend;					\
		m30_t tmp;						\
		for (cend = cursor; *cend >= '0' && *cend <= '9'; cend++); \
		tmp.expo = 2;						\
		tmp.mant = __30_23_get_s(cursor, (cend - cursor));	\
		_into = tmp.v;						\
		cursor = cend;						\
	} else {							\
		_else;							\
	}								\
	while (*cursor++ != '\t')

	/* symbol first */
	cursor = parse_symbol(tgt, cursor);

	PARSE_TIME(tgt->stmp2, cursor, cursor);
	PARSE_MSEC(tgt->msec2, cursor, cursor);
	/* read seq (maybe) */
	PARSE_SEQ(tgt->seq, cursor, line);

	/* read price slot */
	RDPRI(v1, v1 = 0);
	/* read quantity slot */
	RDQTY(v2, v2 = 0);
	/* just the tick type, we switch on that one directly */
	PARSE_TT(tgt->tt, cursor, cursor);

	switch (ibtl_ttf(tgt)) {
	case SL1T_TTF_BID:
	case SL1T_TTF_ASK:
	case SL1T_TTF_TRA:
		tgt->p.v = v1;
		tgt->P.v = v2;
		break;
	case SL1T_TTF_VOL: {
		m30_t tmp;
		tmp.v = v2;
		tgt->v = ffff_m62_get_m30(tmp);
		break;
	}
	default:
	case SL1T_TTF_UNK:
		break;
	}

	/* this definitely crashes if the file doesnt end in a new line */
	while (*cursor++ != '\n');
#undef PARSE_TT
#undef PARSE_SEQ
#undef RDPRI
#undef RDQTY
	return cursor;
}


static bool
read_line(mux_ctx_t ctx, ibrti_tl_t tl)
{
	const char *line, *endp;
	bool res;

	line = prdb_current_line(ctx->rdr);
	endp = parse_tline(tl, line);

	/* ffw the read buffer */
	prdb_set_current_line_by_ptr(ctx->rdr, endp);

	lno++;
#if defined USE_DEBUGGING_ASSERTIONS
	assert(endp[-1] == '\n');
#elif defined USE_ASSERTIONS
	if (endp[-1] != '\n') {
		char buf[MAX_LINE_LEN];
		memcpy(buf, line, endp - line);
		SUSHI_DEBUG("misparsed line: \"%s\"\n", buf);
	}
#endif	/* USE_DEBUGGING_ASSERTIONS */
	/* assess tick quality */
	check_tic_stmp(tl);
	check_tic_offs(tl);

	/* look up the symbol */
	tl->t->tblidx = ute_sym2idx(ctx->wrr, tl->cid);
	/* check if it's good or bad line */
	if (!(res = check_ibrti_tl(tl))) {
		/* bad */
		tl->flags |= 1;
		/* we should make this optional */
		//pr_tl(ctx->badfd, tl, line, endp - line - 1);
	}
	return res;
}

static void
write_tick(mux_ctx_t UNUSED(ctx), ibrti_tl_t tl)
{
/* create one or more sparse ticks, sl1t_t objects */
	struct sl1t_s t[1];

	/* seems to be common */
	sl1t_copy_hdr(t, CONST_SL1T_T(tl));

	if (tl->flags & 2 /*HALTED*/) {
		sl1t_mark_halted(t);
		goto nomore;
	}

	switch (ibtl_ttf(tl)) {
	case SL1T_TTF_BID:
	case SL1T_TTF_ASK:
	case SL1T_TTF_TRA:
		sl1t_set_ttf(t, ibtl_ttf(tl));
		t->bid = tl->p.v;
		t->bsz = tl->P.v;
		break;
	case SL1T_TTF_VOL:
		sl1t_set_ttf(t, ibtl_ttf(tl));
		t->w[0] = tl->v.v;
		break;
	case SL1T_TTF_UNK:
	default:
		return;
	}
nomore:
	/* just one tick to worry about */
	ute_add_tick(ctx->wrr, AS_SCOM(t));
	return;
}

static void
read_lines(mux_ctx_t ctx)
{
	while (moar_ticks_p(ctx)) {
		struct ibrti_tl_s atl;
		memset(&atl, 0, sizeof(atl));
		if (read_line(ctx, &atl)) {
			write_tick(ctx, &atl);
		}
	}
	return;
}

/* is this the right zone? */
static const char tfraw_zone[] = "Europe/Berlin";

void
ibrti_slab(mux_ctx_t ctx)
{
	/* open our timezone definition */
	if (ctx->opts->zone != NULL) {
		z = zif_read_inst(ctx->opts->zone);
	} else {
		z = zif_read_inst(tfraw_zone);
	}
	/* init reader and writer */
	ctx->rdr = make_prdbuf(ctx->infd);
	/* wipe symtbl */
	memset(&symtbl, 0, sizeof(symtbl));

	/* main loop */
	lno = 0;
	while (fetch_lines(ctx)) {
		read_lines(ctx);
		unfetch_lines(ctx);
	}
	/* expobuf is the reader of our choice */
	free_prdbuf(ctx->rdr);
	if (z != NULL) {
		zif_free(z);
	}
	return;
}

/* ibrti.c ends here */
