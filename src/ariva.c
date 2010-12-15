/* ariva files are simple
 * TIMESTAMP.NANOSECONDS UIISYM@POTSYM KEYVALS
 * where KEYVALS is one char and a numeric value.
 * The files are in chronological order. */

/* hope it's ours, the one that defines index_t */
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "pageio.h"
#include "secu.h"
#include "date.h"
#include "utefile.h"
#include "sl1t.h"
#include "sumux.h"
#define DEFINE_GORY_STUFF
#include "m30.h"
#include "m62.h"

#define MAX_LINE_LEN	512
#define countof(x)	(sizeof(x) / sizeof(*x))

#define FLAG_INVAL	0x01
#define FLAG_HALTED	0x02

typedef uint8_t symidx_t;
typedef struct symtbl_s *symtbl_t;
/* ariva tick lines */
typedef struct ariva_tl_s *ariva_tl_t;

/* key is p for price, P for trade size, a for ask, A for ask size
 * b for bid, B for bid size and t for price stamp and T for BA stamp.
 * Made for type punning, ariva_tl_s structs are also sl1t_t's */
struct ariva_tl_s {
	struct scom_thdr_s t;
	/* not so standard fields now */
	struct time_sns_s rcv_stmp;
	uint32_t offs;
	uint32_t stmp2;

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
	struct {
		m62_t v;
		m62_t V;
	};
	struct {
		m30_t k;
	};
	/* just the lowest bit is used, means bad tick */
	uint32_t flags;
};

/* 'nother type extension */
#define NSYMS	UTEHDR_MAX_SYMS
struct metrsymtbl_s {
	/* caches */
	time_t metr[NSYMS];
	int32_t offs[NSYMS];
	m30_t tra[NSYMS];
	m30_t bid[NSYMS];
	m30_t bsz[NSYMS];
	m30_t ask[NSYMS];
	m30_t asz[NSYMS];
	bool aucp[NSYMS];
};


static struct metrsymtbl_s symtbl;
/* convenience macroes for the additional tables */
#define SYMTBL_METR	(symtbl.metr)
#define SYMTBL_OFFS	(symtbl.offs)
#define SYMTBL_TRA	(symtbl.tra)
#define SYMTBL_BID	(symtbl.bid)
#define SYMTBL_BSZ	(symtbl.bsz)
#define SYMTBL_ASK	(symtbl.ask)
#define SYMTBL_ASZ	(symtbl.asz)
#define SYMTBL_AUCP	(symtbl.aucp)

/* convenience bindings to access the sl1t_t slot in there */
static inline __attribute__((pure)) uint32_t
atl_ts_sec(const struct ariva_tl_s *l)
{
	return sl1t_stmp_sec((const_sl1t_t)l);
}

static inline __attribute__((unused)) void
atl_set_ts_sec(ariva_tl_t l, uint32_t s)
{
	return scom_thdr_set_sec(AS_SCOM_THDR_T(l), s);
}

static inline __attribute__((unused, pure)) uint16_t
atl_ts_msec(const struct ariva_tl_s *l)
{
	return scom_thdr_msec(AS_SCOM(l));
}

static inline __attribute__((unused)) void
atl_set_ts_msec(ariva_tl_t l, uint16_t s)
{
	return scom_thdr_set_msec(AS_SCOM_THDR_T(l), s);
}

static inline __attribute__((unused)) void
atl_set_ts_from_sns(ariva_tl_t l, const struct time_sns_s sns)
{
	scom_thdr_set_stmp_from_sns(AS_SCOM_THDR_T(l), sns);
	return;
}

static inline __attribute__((unused, pure)) uint16_t
atl_si(const struct ariva_tl_s *l)
{
	return scom_thdr_tblidx(AS_SCOM(l));
}

static inline void
atl_set_si(ariva_tl_t l, uint16_t si)
{
	return scom_thdr_set_tblidx(AS_SCOM_THDR_T(l), si);
}

static inline uint32_t
atl_cached_sec(const struct ariva_tl_s *l)
{
	return SYMTBL_METR[atl_si(l)];
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

static struct time_sns_s
diff_su(uint32_t s, uint16_t ms, struct time_sns_s ts)
{
	struct time_sns_s res;
	if (ts.nsec < ms * 1000000UL) {
		res.sec = ts.sec - s - 1;
		res.nsec = 1000000000 + ts.nsec - ms * 1000000;
	} else {
		res.sec = ts.sec - s;
		res.nsec = ts.nsec - ms * 1000000;
	}
	return res;
}

static inline int32_t
diff_su_ms(uint32_t s, uint16_t ms, struct time_sns_s ts)
{
/* return the difference in ms */
	struct time_sns_s d = diff_su(s, ms, ts);
	int32_t o = d.sec * 1000 + d.nsec / 1000000;
	return o;
}

static inline int32_t
diff_sl1t_ms(const_sl1t_t t, struct time_sns_s ts)
{
	uint32_t s = sl1t_stmp_sec(t);
	uint16_t ms = sl1t_stmp_msec(t);
	return diff_su_ms(s, ms, ts);
}

static inline void
check_tic_stmp(ariva_tl_t tic)
{
	if (UNLIKELY(atl_ts_sec(tic) == 0)) {
		if (tic->stmp2) {
			/* better than nothing, use him */
			atl_set_ts_sec(tic, tic->stmp2);
		} else {
			/* use the cached metronome value */
			atl_set_ts_sec(tic, atl_cached_sec(tic));
		}
	}
	return;
}

static inline void
check_tic_offs(ariva_tl_t t)
{
	int32_t d;

	if (UNLIKELY(atl_ts_sec(t) == 0)) {
		/* silently drop him? */
		return;
	}
	/* compute current diff */
	d = diff_sl1t_ms((sl1t_t)t, t->rcv_stmp);
	if (d < 0) {
		/* someone invented a time machine */
#if 0
		atl_set_ts_from_sns(t, t->rcv_stmp);
#endif
	} else {
		int32_t cac = SYMTBL_OFFS[atl_si(t)];
		/* update offset table */
		SYMTBL_OFFS[atl_si(t)] = (d + cac) / 2;
	}
	return;
}

static inline void
enrich_batps(ariva_tl_t t)
{
	if (t->P.mant != 0 && t->p.mant == 0 && t->k.mant == 0) {
		if (!SYMTBL_AUCP[atl_si(t)]) {
			/* use the cached trade price */
			t->p = SYMTBL_TRA[atl_si(t)];
			t->k.v = 0;
		} else {
			/* we use the cached TRA value, but assign it to
			 * the auction price slot */
			t->k = SYMTBL_TRA[atl_si(t)];
			t->p.v = 0;
		}
	}
	if (t->b.mant == 0 && t->B.mant != 0) {
		/* use the cached bid price */
		t->b = SYMTBL_BID[atl_si(t)];
	}
	if (t->a.mant == 0 && t->A.mant != 0) {
		/* use the cached ask price */
		t->a = SYMTBL_ASK[atl_si(t)];
	}

	if (t->a.mant != 0 && t->A.mant == 0 &&
	    SYMTBL_ASZ[atl_si(t)].mant != 0) {
		/* use the cached ask size */
		t->A = SYMTBL_ASZ[atl_si(t)];
	}
	if (t->b.mant != 0 && t->B.mant == 0 &&
	    SYMTBL_BSZ[atl_si(t)].mant != 0) {
		/* use the cached bid size */
		t->B = SYMTBL_BSZ[atl_si(t)];
	}
	return;
}

static inline bool
check_ariva_tl(ariva_tl_t t)
{
	/* look if this looks good */
	if (t->flags & 1) {
		return false;
	}
	if (!atl_ts_sec(t)) {
		return false;
	}
	if (t->b.mant == 0 && t->a.mant == 0 &&
	    t->B.mant == 0 && t->A.mant == 0 && t->flags & 2) {
		/* HALTED */
		return true;
	}
	if (t->p.mant == 0 && t->P.mant == 0 &&
	    t->a.mant == 0 && t->b.mant == 0 &&
	    t->A.mant == 0 && t->B.mant == 0 && t->V.mant == 0) {
		return false;
	}
	return true;
}


static bool
parse_rcv_stmp(ariva_tl_t tgt, const char **cursor)
{
#if 1
#define DIE_IF_CURSOR0_IS_NOT(c)		\
	if (UNLIKELY(*((*cursor)++) != (c))) {	\
		return false;			\
	}
	/* the receive time stamp */
	tgt->rcv_stmp.sec = ffff_strtol(*cursor, cursor, 0);
	DIE_IF_CURSOR0_IS_NOT('.');
	tgt->rcv_stmp.nsec = ffff_strtol(*cursor, cursor, 0);
	DIE_IF_CURSOR0_IS_NOT(' ');
#undef DIE_IF_CURSOR0_IS_NOT
	return true;

#else  /* !0 */
	/* common case first */
	if (LIKELY(cursor[0][10] == '.' && cursor[0][20] == ' ')) {
		(*cursor) += 21;
		return true;
	}
	/* otherwise */
	for (int i = 0; i < 21; i++) {
		if (cursor[0][i] == ' ') {
			(*cursor) += i + 1;
			return true;
		}
	}
	return false;
#endif
}

/* bummer, not the wisest global var name, fix me! */
static zif_t z = NULL;

static inline time_t
parse_time(const char *buf)
{
	struct tm tm;

	/* use our sped-up version */
	ffff_strptime_ISO(buf, &tm);
	return ffff_timelocal(&tm, z);
}

static bool
parse_symbol(ariva_tl_t tgt, const char **cursor, void *UNUSED(_fwr))
{
#define FIDDLE3(a, b, c)	(((a) << 16) + ((b) << 8) + (c))
	size_t UNUSED(len);
	const char *p = *cursor;
	uint32_t sum = FIDDLE3(p[0], p[1], p[2]);

	/* special symbols are nothing, C-c and restart,
	 * we use the first 3 characters and demand them to be
	 * significant */
	switch (sum) {
	case FIDDLE3('n', 'o', 't'):
	case FIDDLE3('r', 'e', 's'):
		*cursor += 7;
		return false;
	case FIDDLE3('C', '-', 'c'):
		*cursor += 3;
		return false;
	default:
		break;
	}

	/* otherwise it could be a real symbol */
	for (p += 3; p[0] != ' '; p++) {
		if (UNLIKELY(p[0] == '\n')) {
			*cursor = p;
			return false;
		}
	}
	if ((len = p - *cursor) >= UTEHDR_SYMLEN) {
		len = UTEHDR_SYMLEN;
	}
	atl_set_si(tgt, /*sl1t_fio_find_sym_crea(fwr, *cursor, len)*/1);
	*cursor = p + 1;
#undef FIDDLE3
	return true;
}

static __attribute__((unused)) size_t
reco_tl(char *buf, ariva_tl_t t)
{
	size_t len = 0;
	if (t->p.mant) {
		len += sprintf(buf + len, "\tp%2.4f", ffff_m30_d(t->p));
	}
	if (t->P.mant) {
		len += sprintf(buf + len, "\tP%2.4f", ffff_m30_d(t->P));
	}
	if (atl_ts_sec(t)) {
		char ts[32];
		sprint_tstz(ts, sizeof(ts), atl_ts_sec(t));
		len += sprintf(buf + len, "\tt%s", ts);
	}
	if (t->b.mant) {
		len += sprintf(buf + len, "\tb%2.4f", ffff_m30_d(t->b));
	}
	if (t->B.mant) {
		len += sprintf(buf + len, "\tB%2.4f", ffff_m30_d(t->B));
	}
	if (t->a.mant) {
		len += sprintf(buf + len, "\ta%2.4f", ffff_m30_d(t->a));
	}
	if (t->A.mant) {
		len += sprintf(buf + len, "\tA%2.4f", ffff_m30_d(t->A));
	}
	if (t->k.mant) {
		len += sprintf(buf + len, "\tk%2.4f", ffff_m30_d(t->k));
	}
	if (t->v.mant) {
		len += sprintf(buf + len, "\tv%2.4f", ffff_m62_d(t->v));
	}
	if (t->V.mant) {
		len += sprintf(buf + len, "\tV%2.4f", ffff_m62_d(t->V));
	}
	if (t->stmp2) {
		char ts[32];
		sprint_tstz(ts, sizeof(ts), t->stmp2);
		len += sprintf(buf + len, "\tt%s", ts);
	}
	if (check_ariva_tl(t)) {
		len += sprintf(buf + len, " GOOD");
	} else {
		len += sprintf(buf + len, " INVAL");
	}
	return len;
}

static __attribute__((unused)) void
pr_tl(mux_ctx_t ctx, ariva_tl_t t, const char *cursor, size_t len)
{
	char b[512];
	int32_t d;
	size_t lsz;
	int fd = ctx->badfd;
	//utehdr_t fhdr = sl1t_fio_fhdr(ctx->wrr);

	d = diff_sl1t_ms((sl1t_t)t, t->rcv_stmp);
	lsz = snprintf(b, sizeof(b), "%u.%09u (%d) %d ",
		       t->rcv_stmp.sec, t->rcv_stmp.nsec,
		       d, atl_si(t) /*utehdr_nth_sym(fhdr, atl_si(t))*/);
	memcpy(b + lsz, cursor, len);
	lsz = lsz + len;
	lsz += reco_tl(b + lsz, t);
	b[lsz++] = '\n';
	write(fd, b, lsz);
	return;
}

static void
parse_keyval(ariva_tl_t tgt, const char **cursor)
{
/* assumes tgt's si is set already */
	const char *p = *cursor;

	while (true) {
		switch (*p++) {
		case '\n':
			p--;
			goto out;
		case 'p':
			tgt->p = ffff_m30_get_s((char**)&p);
			p++;
			/* store in cache */
			SYMTBL_TRA[atl_si(tgt)] = tgt->p;
			/* also reset auction */
			SYMTBL_AUCP[atl_si(tgt)] = false;
			continue;
		case 'b':
			tgt->b = ffff_m30_get_s((char**)&p);
			p++;
			/* store in cache */
			SYMTBL_BID[atl_si(tgt)] = tgt->b;
			if (tgt->b.mant == 0) {
				tgt->flags |= FLAG_HALTED;
			}
			continue;
		case 'a':
			tgt->a = ffff_m30_get_s((char**)&p);
			p++;
			/* store in cache */
			SYMTBL_ASK[atl_si(tgt)] = tgt->a;
			if (tgt->a.mant == 0) {
				tgt->flags |= FLAG_HALTED;
			}
			continue;
		case 'k':
			tgt->k = ffff_m30_get_s((char**)&p);
			p++;
			/* once weve seen this, an auction is going on */
			SYMTBL_AUCP[atl_si(tgt)] = true;
			continue;
		case 'v':
			/* unlike our `v' this is the vol-pri */
			tgt->V = ffff_m62_get_s((char**)&p);
			p++;
			continue;
		case 'P':
			tgt->P = ffff_m30_23_get_s((char**)&p);
			p++;
			continue;
		case 'B':
			tgt->B = ffff_m30_23_get_s((char**)&p);
			p++;
			/* store in cache */
			SYMTBL_BSZ[atl_si(tgt)] = tgt->B;
			if (tgt->B.mant == 0) {
				tgt->flags |= FLAG_HALTED;
			}
			continue;
		case 'A':
			tgt->A = ffff_m30_23_get_s((char**)&p);
			p++;
			/* store in cache */
			SYMTBL_ASZ[atl_si(tgt)] = tgt->A;
			if (tgt->A.mant == 0) {
				tgt->flags |= FLAG_HALTED;
			}
			continue;
		case 'V':
			/* this is the volume */
			tgt->v = ffff_m62_get_s((char**)&p);
			p++;
			continue;

		case 'T':
			tgt->stmp2 = parse_time(p);
			p += 15;
			if (atl_ts_sec(tgt) > 0) {
				continue;
			}
			/* store in cache */
			SYMTBL_METR[atl_si(tgt)] = tgt->stmp2;
			continue;
		case 't':
			/* price stamp */
			atl_set_ts_sec(tgt, parse_time(p));
			p += 15;
			/* also set the instruments metronome */
			SYMTBL_METR[atl_si(tgt)] = atl_ts_sec(tgt);
			continue;
		case 'c':
			/* must be a candle tick, kick it */
			tgt->flags = FLAG_INVAL;
			goto out;
		default:
			break;
		}
		while (*p > ' ') {
			p++;
		}
		if (*p == ' ') {
			p++;
		}
	}
out:
	if (tgt->flags & FLAG_HALTED &&
	    (tgt->k.mant != 0 || tgt->P.mant != 0 ||
	     (tgt->a.mant != 0 && tgt->A.mant != 0) ||
	     (tgt->b.mant != 0 && tgt->B.mant != 0))) {
		tgt->flags &= ~FLAG_HALTED;
	}
	*cursor = p;
	return;
}

static const char*
parse_tline(ariva_tl_t tgt, const char *line, void *fwr)
{
/* just assumes there is enough space */
	const char *cursor = line;

	/* receive time stamp, always first on line */
	if (UNLIKELY(!parse_rcv_stmp(tgt, &cursor))) {
		goto eol;
	}
	/* symbol comes next, or `nothing' or `C-c' */
	if (UNLIKELY(!parse_symbol(tgt, &cursor, fwr))) {
		goto eol;
	}
	/* and now all the rest */
	parse_keyval(tgt, &cursor);
eol:
	while (*cursor++ != '\n');
	return cursor;
}


static bool
read_line(mux_ctx_t ctx, ariva_tl_t tl)
{
	const char *line = prdb_current_line(ctx->rdr);
	const char *endp = parse_tline(tl, line, ctx->wrr);
	bool res;
	/* assess tick quality */
	check_tic_stmp(tl);
	check_tic_offs(tl);
	/* check if it's good or bad line */
	if ((res = check_ariva_tl(tl))) {
		/* good */
		enrich_batps(tl);
	} else {
		/* bad */
		tl->flags = 1;
		/* we should make this optional */
		pr_tl(ctx, tl, line, endp - line - 1);
	}
	/* ffw the read buffer */
	prdb_set_current_line_by_ptr(ctx->rdr, endp);
	return res;
}

static void
write_tick(mux_ctx_t ctx, ariva_tl_t tl)
{
/* create one or more sparse ticks, sl1t_t objects */
	struct sl1t_s t[6];
	sl1t_t tp = t;

	if (tl->flags == 2 /*HALTED*/) {
		sl1t_copy_hdr(tp, (void*)tl);
		sl1t_mark_halted(tp);
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

	if (tl->k.mant) {
		sl1t_copy_hdr(tp, (void*)tl);
		sl1t_set_ttf(tp, SL1T_TTF_AUC);
		tp->tra = tl->k.v;
		tp->tsz = tl->P.v;
		tp++;
	}

	if (tl->v.mant) {
		sl1t_copy_hdr(tp, (void*)tl);
		sl1t_set_ttf(tp, SL1T_TTF_VOL);
		tp->w[0] = tl->v.v;
		tp++;
	}

	if (tl->V.mant) {
		sl1t_copy_hdr(tp, (void*)tl);
		sl1t_set_ttf(tp, SL1T_TTF_VPR);
		tp->w[0] = tl->V.v;
		tp++;
	}

nomore:
	for (int i = 0; i < (tp - t); i++) {
		ute_add_tick(ctx->wrr, AS_SCOM(t + i));
	}
	return;
}

#if 0
static void
write_trng(mux_ctx_t ctx, ariva_tl_t tl)
{
/* we cant bring this until there's clarity about what we're gonna do
 * with metadata */
	utehdr_t fhdr = sl1t_fio_fhdr(ctx->wrr);
	time_range_t tr = utehdr_nth_trng(fhdr, atl_si(tl));
	if (UNLIKELY(tr->lo == 0)) {
		tr->lo = atl_ts_sec(tl);
	}
	/* always set the last seen date */
	tr->hi = atl_ts_sec(tl);
	return;
}
#else
static void
write_trng(mux_ctx_t UNUSED(ctx), ariva_tl_t UNUSED(tl))
{
	return;
}
#endif

static void
read_lines(mux_ctx_t ctx)
{
	while (moar_ticks_p(ctx)) {
		struct ariva_tl_s atl;
		memset(&atl, 0, sizeof(atl));
		if (read_line(ctx, &atl)) {
			write_tick(ctx, &atl);
			write_trng(ctx, &atl);
		}
	}
	return;
}

static const char ariva_zone[] = "Europe/Berlin";

void
ariva_slab(mux_ctx_t ctx)
{
	/* open our timezone definition */
	if (ctx->opts->zone != NULL) {
		z = zif_read_inst(ctx->opts->zone);
	} else {
		z = zif_read_inst(ariva_zone);
	}
	/* init reader and writer */
	ctx->rdr = make_prdbuf(ctx->infd);
	/* wipe symtbl */
	memset(&symtbl, 0, sizeof(symtbl));
	/* main loop */
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

/* ariva.c ends here */
