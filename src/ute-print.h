#if !defined INCLUDED_ute_print_h_
#define INCLUDED_ute_print_h_

#define MAX_LINE_LEN		512

#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif	/* !LIKELY */
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif	/* !UNLIKELY */
#if !defined UNUSED
# define UNUSED(_x)	__attribute__((unused)) _x
#endif	/* !UNUSED */

static inline size_t
pr_ts(char *restrict buf, uint32_t sec)
{
	struct tm tm;
	ffff_gmtime(&tm, sec);
	ffff_strftime(buf, 32, &tm);
	return 19;
}

static inline size_t
pr_tsmstz(char *restrict buf, uint32_t sec, uint32_t msec, zif_t z)
{
	struct tm tm;
	int h, m, off;
	ffff_localtime(&tm, sec, z);
	ffff_strftime(buf, 32, &tm);
	buf[19] = '.';
	buf[20] = (char)(((msec / 100) % 10) + '0');
	buf[21] = (char)(((msec / 10) % 10) + '0');
	buf[22] = (char)(((msec / 1) % 10) + '0');
	buf[23] = ' ';
	/* compute offset as HHMM */
	if ((off = tm.tm_gmtoff) == 0) {;
		buf[24] = '+';
		buf[25] = '0';
		buf[26] = '0';
		buf[27] = '0';
		buf[28] = '0';
		goto done;
	} else if (off > 0) {
		h = off / 3600;
		m = (off % 3600) / 60;
		buf[24] = '+';
	} else /* (off < 0) */ {
		off = -off;
		h = off / 3600;
		m = (off % 3600) / 60;
		buf[24] = '-';
	}
	buf[25] = (char)(((h / 10) % 10) + '0');
	buf[26] = (char)(((h / 1) % 10) + '0');
	buf[27] = (char)(((m / 10) % 10) + '0');
	buf[28] = (char)(((m / 1) % 10) + '0');
done:
	return 29;
}

static inline size_t
pr_sym(utectx_t ctx, char *restrict buf, uint16_t idx)
{
	const char *p;
	size_t pl = 0;

	if (LIKELY((p = ute_idx2sym(ctx, idx)) != NULL)) {
		pl = strlen(p);
		memcpy(buf, p, pl);
	}
	return pl;
}

#endif	/* INCLUDED_ute_print_h_ */
