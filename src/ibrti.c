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
#include "sumux.h"

#if defined USE_DEBUGGING_ASSERTIONS
# include <assert.h>
#endif	/* USE_DEBUGGING_ASSERTIONS */
#define DEFINE_GORY_STUFF
#include "m30.h"
#include "m62.h"

#define MAX_LINE_LEN	512
#define countof(x)	(sizeof(x) / sizeof(*x))

typedef uint8_t symidx_t;
typedef struct symtbl_s *symtbl_t;
/* ariva tick lines */
typedef struct ibrti_tl_s *ibrti_tl_t;

/* key is p for price, P for trade size, a for ask, A for ask size
 * b for bid, B for bid size and t for price stamp and T for BA stamp.
 * Made for type punning, ibrti_tl_s structs are also su_sl1t_t's */
struct ibrti_tl_s {
	struct scom_thdr_s t;
	/* not so standard fields now */
	struct time_sns_s rcv_stmp;
	uint32_t offs;
	uint32_t stmp2;
	uint16_t msec2;

	union {
		struct {
			uint32_t p;
			uint32_t P;
		};
		struct {
			uint32_t b;
			uint32_t B;
		};
		struct {
			uint32_t a;
			uint32_t A;
		};
		uint64_t v;
	};
	uint32_t seq;
	uint16_t tt;
	/* just the lowest bit is used, means bad tick */
	uint16_t flags;
};

/* 'nother type extension */
#define NSYMS	UTEHDR_MAX_SECS
struct metrsymtbl_s {
	/* fuck ugly, this points into kernel space */
	utehdr_t st;
	/* caches */
	uint32_t tra[NSYMS];
	uint32_t tsz[NSYMS];
	uint32_t bid[NSYMS];
	uint32_t bsz[NSYMS];
	uint32_t ask[NSYMS];
	uint32_t asz[NSYMS];
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

static inline void
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

/* printers */
static size_t
sprint_tstz(char *restrict buf, size_t len, time_t ts)
{
	struct tm brktime;
	ffff_gmtime(&brktime, ts);
	ffff_strftime(buf, len, &brktime);
	return 19;
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
		if (t->p == 0 && t->P != 0) {
			/* check if we repeat the tick */
			if (t->P == SYMTBL_TSZ[sl]) {
				return false;
			}
			/* use the cached trade price */
			t->p = SYMTBL_TRA[sl];
		} else if (t->p != 0) {
			/* cache the trade price */
			SYMTBL_TRA[sl] = t->p;
			SYMTBL_TSZ[sl] = t->P;
		}
		break;
	case SL1T_TTF_BID:
		if (t->b == 0 && t->B != 0) {
			/* check if we repeat the tick */
			if (t->B == SYMTBL_BSZ[sl]) {
				return false;
			}
			/* use the cached bid price */
			t->b = SYMTBL_BID[sl];
		} else if (t->b != 0) {
			/* cache the bid price */
			SYMTBL_BID[sl] = t->b;
			SYMTBL_BSZ[sl] = t->B;
		}
		break;
	case SL1T_TTF_ASK:
		if (t->a == 0 && t->A != 0) {
			/* check if we repeat the tick */
			if (t->A == SYMTBL_ASZ[sl]) {
				return false;
			}
			/* otherwise use the cached ask price */
			t->a = SYMTBL_ASK[sl];
		} else if (t->a != 0) {
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
	if (!t->p && !t->P) {
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

static bool
parse_symbol(ibrti_tl_t tgt, const char **cursor, void *fwr)
{
#if 0
/* FIXME!!! Convert me to proper ute */
	long int at;
	uint16_t si;
	bool res;

	if ((res = (at = ffff_strtol(*cursor, cursor, 0)))) {
		su_secu_t se = su_secu(at, 0, 0);
		if ((res = (si = sl1t_fio_find_secu_crea(fwr, se)))) {
			ibtl_set_si(tgt, si);
		} else {
			tgt->flags |= 4;
		}
	}
	/* has to be a tab */
#if defined USE_DEBUGGING_ASSERTIONS
	assert(*(*cursor)++ == '\t');
#else  /* !USE_DEBUGGING_ASSERTIONS */
	(*cursor)++;
#endif	/* USE_DEBUGGING_ASSERTIONS */
	return res;
#else  /* !0 */
	return false;
#endif	/* 0 */
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


static __attribute__((unused)) size_t
reco_tl(char *buf, ibrti_tl_t t)
{
	size_t len = 0;
	if (t->p) {
		len += sprintf(buf + len, "\tp%2.4f", ffff_m30_d((m30_t)t->p));
	}
	if (t->P) {
		len += sprintf(buf + len, "\tP%2.4f", ffff_m30_d((m30_t)t->P));
	}
	if (ibtl_ts_sec(t)) {
		char ts[32];
		uint16_t msec = ibtl_ts_msec(t);
		sprint_tstz(ts, sizeof(ts), ibtl_ts_sec(t));
		len += sprintf(buf + len, "\tt%s.%03hu", ts, msec);
	}
	if (t->stmp2) {
		char ts[32];
		sprint_tstz(ts, sizeof(ts), t->stmp2);
		len += sprintf(buf + len, "\tt%s.%03hu", ts, t->msec2);
	}
	if (check_ibrti_tl(t)) {
		len += sprintf(buf + len, " GOOD");
	} else {
		len += sprintf(buf + len, " INVAL");
	}
	return len;
}

static __attribute__((unused)) void
pr_tl(int fd, ibrti_tl_t t, const char *cursor, size_t len)
{
	char b[512];
	size_t lsz;

	if (t->flags & 4) {
		static const char oob[] = "out-of-bounds";
		lsz = sizeof(oob) - 1;
		memcpy(b, oob, lsz);
	} else {
		uint16_t si = ibtl_si(t);
		su_secu_t sec = SYMTBL_SEC[si];
		uint32_t quodi = su_secu_quodi(sec);
		uint32_t quoti = su_secu_quoti(sec);
		uint16_t pot = su_secu_pot(sec);
		lsz = sprintf(b, "%u/%u@%hu", quodi, quoti, pot);
	}
	b[lsz++] = ' ';
#if 1
	memcpy(b + lsz, cursor, len);
	lsz = lsz + len;
	lsz += reco_tl(b + lsz, t);
	b[lsz++] = '\n';
#else
	lsz += sprintf(b + lsz, "%zu\n", len);
#endif
	write(fd, b, lsz);
	return;
}

static const char*
parse_tline(ibrti_tl_t tgt, const char *line, void *fwr)
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

	/* symbol comes next, or `nothing' or `C-c' */
	if (UNLIKELY(!parse_symbol(tgt, &cursor, fwr))) {
		/* mark tick as bad */
		tgt->flags |= 1;
		if (cursor - line > 60) {
			fputs("long line detected\n", stderr);
		}
		goto eol;
	}
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
		tgt->p = (uint32_t)v1;
		tgt->P = (uint32_t)v2;
		break;
	case SL1T_TTF_VOL: {
		m30_t tmp = {.v = v2};
		tgt->v = ffff_m62_get_m30(tmp).v;
		break;
	}
	default:
	case SL1T_TTF_UNK:
		break;
	}

eol:
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
	endp = parse_tline(tl, line, ctx->wrr);

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
write_tick(mux_ctx_t ctx, ibrti_tl_t tl)
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
		t->bid = tl->p;
		t->bsz = tl->P;
		break;
	case SL1T_TTF_VOL:
		sl1t_set_ttf(t, ibtl_ttf(tl));
		t->w[0] = tl->v;
		break;
	case SL1T_TTF_UNK:
	default:
		return;
	}
nomore:
#if 0
/* FIXME!!! Convert me to proper ute */
	sl1t_fio_write_ticks(ctx->wrr, t, 1);
#endif	/* 0 */
	return;
}

static void
write_trng(mux_ctx_t ctx, ibrti_tl_t tl)
{
#if 0
/* FIXME!!! Convert me to proper ute */
	utehdr_t fhdr = sl1t_fio_fhdr(ctx->wrr);
	time_range_t tr = utehdr_nth_trng(fhdr, ibtl_si(tl));
	if (UNLIKELY(tr->lo == 0)) {
		tr->lo = ibtl_ts_sec(tl);
	}
	/* always set the last seen date */
	tr->hi = ibtl_ts_sec(tl);
#endif	/* 0 */
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
			write_trng(ctx, &atl);
		}
	}
	return;
}

void
ibrti_slab(mux_ctx_t ctx)
{
	/* open our timezone definition */
	if (ctx->opts->zone != NULL) {
		z = zif_read_inst(ctx->opts->zone);
	}
	/* init reader and writer */
	ctx->rdr = make_prdbuf(ctx->infd);
	/* wipe symtbl */
	memset(&symtbl, 0, sizeof(symtbl));
#if 0
/* FIXME!!! Convert me to proper ute */
	/* i wanna stab myself */
	symtbl.st = sl1t_fio_fhdr(ctx->wrr);
#endif	/* 0 */
	/* main loop */
	lno = 0;
	while (fetch_lines(ctx)) {
		read_lines(ctx);
		unfetch_lines(ctx);
	}
	/* expobuf is the reader of our choice */
	free_prdbuf(ctx->rdr);
#if 0
/* FIXME!!! Convert me to proper ute */
	free_sl1t_writer(ctx->wrr);
#endif	/* 0 */
	if (z != NULL) {
		zif_free(z);
	}
	return;
}

/* ibrti.c ends here */
