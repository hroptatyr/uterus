/*** clitoris.c -- command-line-interface tester or is it?
 *
 * Copyright (C) 2013-2014 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@ga-group.nl>
 *
 * This file is part of clitoris.
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
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <limits.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#if defined HAVE_PTY_H
# include <pty.h>
#endif	/* HAVE_PTY_H */
/* check for me */
#include <wordexp.h>

#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif	/* !LIKELY */
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif	/* UNLIKELY */

#if !defined countof
# define countof(x)	(sizeof(x) / sizeof(*x))
#endif	/* !countof */

#if !defined with
# define with(args...)	for (args, *__ep__ = (void*)1; __ep__; __ep__ = 0)
#endif	/* !with */

#if !defined PATH_MAX
# define PATH_MAX	256U
#endif	/* !PATH_MAX */

typedef struct clitf_s clitf_t;
typedef struct clit_buf_s clit_buf_t;
typedef struct clit_bit_s clit_bit_t;
typedef struct clit_tst_s *clit_tst_t;

struct clitf_s {
	size_t z;
	void *d;
};

struct clit_buf_s {
	size_t z;
	const char *d;
};

/**
 * A clit bit can be an ordinary memory buffer (z > 0 && d),
 * a file descriptor (fd != 0 && d == NULL), or a file name (z == -1UL && fn) */
struct clit_bit_s {
	size_t z;
	const char *d;
};

struct clit_chld_s {
	int pin;
	int pou;
	int per;

	pid_t chld;
	pid_t diff;
	pid_t feed;

	unsigned int test_id;

	unsigned int verbosep:1;
	unsigned int ptyp:1;
	unsigned int keep_going_p:1;

	unsigned int timeo;
};

/* a test is the command (inlcuding stdin), stdout result, and stderr result */
struct clit_tst_s {
	clit_bit_t cmd;
	clit_bit_t out;
	clit_bit_t err;
	clit_bit_t rest;

	/* specific per-test flags */
	/** don't fail when the actual output differs from th expected output */
	unsigned int ign_out:1;
	/** don't fail when the command returns non-SUCCESS */
	unsigned int ign_ret:1;

	/** don't pass the output on to external differ */
	unsigned int supp_diff:1;
	/** expand the proto-output as though it was a shell here-document */
	unsigned int xpnd_proto:1;

	/* padding */
	unsigned int:5;

	/** expect this return code, or any but 0 if all bits are set */
	unsigned int exp_ret:8;
};


static sigset_t fatal_signal_set[1];
static sigset_t empty_signal_set[1];

static const char dflt_diff[] = "diff";
static const char *cmd_diff = dflt_diff;


static void
__attribute__((format(printf, 1, 2)))
error(const char *fmt, ...)
{
	va_list vap;
	va_start(vap, fmt);
	vfprintf(stderr, fmt, vap);
	va_end(vap);
	if (errno) {
		fputs(": ", stderr);
		fputs(strerror(errno), stderr);
	}
	fputc('\n', stderr);
	return;
}

static inline __attribute__((const, pure, always_inline)) char*
deconst(const char *s)
{
	union {
		const char *c;
		char *p;
	} x = {s};
	return x.p;
}


/* imported code */
/*** fast_strstr.c
 *
 * This algorithm is licensed under the open-source BSD3 license
 *
 * Copyright (c) 2014, Raphael Javaux
 * All rights reserved.
 *
 * Licence text, see above.
 *
 * The code has been modified to mimic memmem().
 *
 **/
/**
* Finds the first occurrence of the sub-string needle in the string haystack.
* Returns NULL if needle was not found.
*/
static char*
xmemmem(const char *hay, const size_t hayz, const char *ndl, const size_t ndlz)
{
	const char *const eoh = hay + hayz;
	const char *const eon = ndl + ndlz;
	const char *hp;
	const char *np;
	const char *cand;
	unsigned int hsum;
	unsigned int nsum;
	unsigned int eqp;

	/* trivial checks first
         * a 0-sized needle is defined to be found anywhere in haystack
         * then run strchr() to find a candidate in HAYSTACK (i.e. a portion
         * that happens to begin with *NEEDLE) */
	if (ndlz == 0UL) {
		return deconst(hay);
	} else if ((hay = memchr(hay, *ndl, hayz)) == NULL) {
		/* trivial */
		return NULL;
	}

	/* First characters of haystack and needle are the same now. Both are
	 * guaranteed to be at least one character long.  Now computes the sum
	 * of characters values of needle together with the sum of the first
	 * needle_len characters of haystack. */
	for (hp = hay + 1U, np = ndl + 1U, hsum = *hay, nsum = *hay, eqp = 1U;
	     hp < eoh && np < eon;
	     hsum ^= *hp, nsum ^= *np, eqp &= *hp == *np, hp++, np++);

	/* HP now references the (NZ + 1)-th character. */
	if (np < eon) {
		/* haystack is smaller than needle, :O */
		return NULL;
	} else if (eqp) {
		/* found a match */
		return deconst(hay);
	}

	/* now loop through the rest of haystack,
	 * updating the sum iteratively */
	for (cand = hay; hp < eoh; hp++) {
		hsum ^= *cand++;
		hsum ^= *hp;

		/* Since the sum of the characters is already known to be
		 * equal at that point, it is enough to check just NZ - 1
		 * characters for equality,
		 * also CAND is by design < HP, so no need for range checks */
		if (hsum == nsum && memcmp(cand, ndl, ndlz - 1U) == 0) {
			return deconst(cand);
		}
	}
	return NULL;
}


/* clit bit handling */
#define CLIT_BIT_FD(x)	(clit_bit_fd_p(x) ? (int)(x).z : -1)

static inline __attribute__((const, pure)) bool
clit_bit_buf_p(clit_bit_t x)
{
	return x.z != -1UL && x.d != NULL;
}

static inline __attribute__((const, pure)) bool
clit_bit_fd_p(clit_bit_t x)
{
	return x.d == NULL;
}

static inline __attribute__((const, pure)) bool
clit_bit_fn_p(clit_bit_t x)
{
	return x.z == -1UL && x.d != NULL;
}

/* ctors */
static inline __attribute__((unused)) clit_bit_t
clit_make_fd(int fd)
{
	return (clit_bit_t){.z = fd};
}

static inline clit_bit_t
clit_make_fn(const char *fn)
{
	return (clit_bit_t){.z = -1UL, .d = fn};
}

static const char*
bufexp(const char src[static 1], size_t ssz)
{
	static char *buf;
	static size_t bsz;
	wordexp_t xp[1];

	if (UNLIKELY(ssz == 0)) {
		return NULL;
	}

#define CHKBSZ(x)						   \
	if ((x) >= bsz) {					   \
		bsz = ((x) / 256U + 1U) * 256U;			   \
		if (UNLIKELY((buf = realloc(buf, bsz)) == NULL)) { \
			/* well we'll leak XP here */		   \
			return NULL;				   \
		}						   \
	}

	/* get our own copy for deep vein massages */
	CHKBSZ(ssz);
	memcpy(buf, src, ssz);
	buf[ssz] = '\0';

	switch (wordexp(buf, xp, WRDE_UNDEF)) {
	case 0:
		if (xp->we_wordc > 0) {
			/* everything's fine */
			break;
		}
	case WRDE_NOSPACE:
		wordfree(xp);
	default:
		return NULL;
	}

	/* copy the first `argument', back into BUF,
	 * which is hopefully big enough */
	with (size_t wz = strlen(xp->we_wordv[0])) {
		CHKBSZ(wz);
		memcpy(buf, xp->we_wordv[0], wz);
		buf[wz] = '\0';
	}

	wordfree(xp);
	return buf;
}


static clitf_t
mmap_fd(int fd, size_t fz)
{
	void *p;

	if ((p = mmap(NULL, fz, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED) {
		return (clitf_t){.z = 0U, .d = NULL};
	}
	return (clitf_t){.z = fz, .d = p};
}

static int
munmap_fd(clitf_t map)
{
	return munmap(map.d, map.z);
}

static void
block_sigs(void)
{
	(void)sigprocmask(SIG_BLOCK, fatal_signal_set, (sigset_t*)NULL);
	return;
}

static void
unblock_sigs(void)
{
	sigprocmask(SIG_SETMASK, empty_signal_set, (sigset_t*)NULL);
	return;
}

static void
unblock_sig(int sig)
{
	static sigset_t unblk_set[1];

	sigemptyset(unblk_set);
	sigaddset(unblk_set, sig);
	(void)sigprocmask(SIG_UNBLOCK, unblk_set, (sigset_t*)NULL);
	return;
}

#if defined HAVE_PTY_H
static pid_t
pfork(int *pty)
{
	if (UNLIKELY(pty == NULL)) {
		errno = ENOMEM;
		return -1;
	}
	return forkpty(pty, NULL, NULL, NULL);
}
#else  /* !HAVE_PTY_H */
static pid_t
pfork(int *pty)
{
	fputs("pseudo-tty not supported\n", stderr);
	return *pty = -1;
}
#endif	/* HAVE_PTY_H */


static const char *
find_shtok(const char *bp, size_t bz)
{
/* finds a (lone) occurrence of $ at the beginning of a line */
	for (const char *res;
	     (res = memchr(bp, '$', bz)) != NULL;
	     bz -= (res + 1 - bp), bp = res + 1) {
		/* we're actually after a "\n$" or
		 * a "$" at the beginning of the buffer pointer (bp)
		 * now check that either the buffer ends there or
		 * the $ is followed by a newline, or the $ is followed
		 * by a space, which is the line-to-exec indicator */
		if ((res == bp || res[-1] == '\n') &&
		    (bz <= 1U || (res[1] == '\n' || res[1] == ' '))) {
			return res;
		}
	}
	return NULL;
}

static clit_bit_t
find_cmd(const char *bp, size_t bz)
{
	clit_bit_t resbit = {0U};
	clit_bit_t tok;

	/* find the bit where it says '$ ' */
	with (const char *res) {
		if (UNLIKELY((res = find_shtok(bp, bz)) == NULL)) {
			return (clit_bit_t){0U};
		} else if (UNLIKELY(res[1] != ' ')) {
			return (clit_bit_t){0U};
		}
		/* otherwise */
		resbit.d = res += 2U;
		bz -= res - bp;
		bp = res;
	}

	/* find the new line bit */
	for (const char *res;
	     (res = memchr(bp, '\n', bz)) != NULL;
	     bz -= (res + 1U - bp), bp = res + 1U) {
		size_t lz = (res + 1U - bp);

		/* check for trailing \ or <<EOF (in that line) */
		if (UNLIKELY((tok.d = xmemmem(bp, lz, "<<", 2U)) != NULL)) {
			tok.d += 2U;
			tok.z = res - tok.d;
			/* analyse this eof token */
			bp = res + 1U;
			goto here_doc;
		} else if (res == bp || res[-1] != '\\') {
			resbit.z = res + 1 - resbit.d;
			break;
		}
	}
	return resbit;

here_doc:
	/* massage tok so that it starts on a non-space and ends on one */
	for (; tok.z && (*tok.d == ' ' || *tok.d == '\t'); tok.d++, tok.z--);
	for (;
	     tok.z && (tok.d[tok.z - 1] == ' ' || tok.d[tok.z - 1] == '\t');
	     tok.z--);
	if (tok.z &&
	    (*tok.d == '\'' || *tok.d == '"') && tok.d[tok.z - 1] == *tok.d) {
		tok.d++;
		tok.z -= 2U;
	}
	/* now find the opposite EOF token */
	for (const char *eotok;
	     (eotok = xmemmem(bp, bz, tok.d, tok.z)) != NULL;
	     bz -= eotok + 1U - bp, bp = eotok + 1U) {
		if (LIKELY(eotok[-1] == '\n' && eotok[tok.z] == '\n')) {
			resbit.z = eotok + tok.z + 1U - resbit.d;
			break;
		}
	}
	return resbit;
}

static int
find_ignore(struct clit_tst_s tst[static 1])
{
	with (const char *cmd = tst->cmd.d, *const ec = cmd + tst->cmd.z) {
		static char tok_ign[] = "ignore";
		static char tok_out[] = "output";
		static char tok_ret[] = "return";

		if (strncmp(cmd, tok_ign, sizeof(tok_ign) - 1U)) {
			/* don't bother */
			break;
		}
		/* fast-forward a little */
		cmd += sizeof(tok_ign) - 1U;

		if (isspace(*cmd)) {
			/* it's our famous ignore token it seems */
			tst->ign_out = tst->ign_ret = 1U;
		} else if (*cmd++ != '-') {
			/* unknown token then */
			break;
		} else if (!strncmp(cmd, tok_out, sizeof(tok_out) - 1U)) {
			/* ignore-output it is */
			tst->ign_out = 1U;
			cmd += sizeof(tok_out) - 1U;
		} else if (!strncmp(cmd, tok_ret, sizeof(tok_ret) - 1U)) {
			/* ignore-return it is */
			tst->ign_ret = 1U;
			cmd += sizeof(tok_ret) - 1U;
		} else {
			/* don't know what's going on */
			break;
		}

		/* now, fast-forward to the actual command, and reass */
		while (++cmd < ec && isspace(*cmd));
		tst->cmd.z -= (cmd - tst->cmd.d);
		tst->cmd.d = cmd;
		return 0;
	}
	return -1;
}

static int
find_negexp(struct clit_tst_s tst[static 1])
{
	with (const char *cmd = tst->cmd.d, *const ec = cmd + tst->cmd.z) {
		unsigned int exp = 0U;

		switch (*cmd) {
		case '!'/*NEG*/:
			exp = 255U;
			break;
		case '?'/*EXP*/:;
			char *p;
			exp = strtoul(cmd + 1U, &p, 10);
			cmd = cmd + (p - cmd);
			if (isspace(*cmd)) {
				break;
			}
		default:
			return -1;
		}

		/* now, fast-forward to the actual command, and reass */
		while (++cmd < ec && isspace(*cmd));
		tst->cmd.z -= (cmd - tst->cmd.d);
		tst->cmd.d = cmd;
		tst->exp_ret = exp;
	}
	return 0;
}

static int
find_suppdiff(struct clit_tst_s tst[static 1])
{
	with (const char *cmd = tst->cmd.d, *const ec = cmd + tst->cmd.z) {
		switch (*cmd) {
		case '@':
			break;
		default:
			return -1;
		}

		/* now, fast-forward to the actual command, and reass */
		while (++cmd < ec && isspace(*cmd));
		tst->cmd.z -= (cmd - tst->cmd.d);
		tst->cmd.d = cmd;
		tst->supp_diff = 1U;
		tst->ign_out = 1U;
	}
	return 0;
}

static int
find_xpnd_proto(struct clit_tst_s tst[static 1])
{
	with (const char *cmd = tst->cmd.d, *const ec = cmd + tst->cmd.z) {
		switch (*cmd) {
		case '$':
			break;
		default:
			return -1;
		}

		/* now, fast-forward to the actual command, and reass */
		while (++cmd < ec && isspace(*cmd));
		tst->cmd.z -= (cmd - tst->cmd.d);
		tst->cmd.d = cmd;
		tst->xpnd_proto = 1U;
	}
	return 0;
}

static int
find_tst(struct clit_tst_s tst[static 1], const char *bp, size_t bz)
{
	if (UNLIKELY(!(tst->cmd = find_cmd(bp, bz)).z)) {
		goto fail;
	}
	/* reset bp and bz */
	bz = bz - (tst->cmd.d + tst->cmd.z - bp);
	bp = tst->cmd.d + tst->cmd.z;
	if (UNLIKELY((tst->rest.d = find_shtok(bp, bz)) == NULL)) {
		goto fail;
	}
	/* otherwise set the rest bit already */
	tst->rest.z = bz - (tst->rest.d - bp);

	/* now the stdout bit must be in between (or 0) */
	with (size_t outz = tst->rest.d - bp) {
		if (outz &&
		    /* prefixed '< '? */
		    UNLIKELY(bp[0] == '<' && bp[1] == ' ')) {
			/* it's a < FILE comparison */
			const char *fn;

			if ((fn = bufexp(bp + 2, outz - 2U - 1U)) != NULL) {
				tst->out = clit_make_fn(fn);
			} else {
				error("expansion failed");
				goto fail;
			}
		} else {
			tst->out = (clit_bit_t){.z = outz, bp};
		}
	}

	while (
		/* oh let's see if we should ignore things */
		!find_ignore(tst) ||

		/* check for suppress diff */
		!find_suppdiff(tst) ||

		/* check for expect and negate operators */
		!find_negexp(tst) ||

		/* check for proto-output expander */
		!find_xpnd_proto(tst) ||

		0);

	tst->err = (clit_bit_t){0U};
	return 0;
fail:
	memset(tst, 0, sizeof(*tst));
	return -1;
}

static int
find_opt(struct clit_chld_s ctx[static 1], const char *bp, size_t bz)
{
	static const char magic[] = "setopt ";

	for (const char *mp;
	     (mp = xmemmem(bp, bz, magic, sizeof(magic) - 1U)) != NULL;
	     bz -= (mp + 1U) - bp, bp = mp + 1U) {
		unsigned int opt;

		/* check if it's setopt or unsetopt */
		if (mp == bp || LIKELY(mp[-1] == '\n')) {
			/* yay, it's a genuine setopt */
			opt = 1U;
		} else if (mp >= bp + 2U && mp[-2] == 'u' && mp[-1] == 'n' &&
			   (mp == bp + 2U || mp > bp + 2U && mp[-3] == '\n')) {
			/* it's a genuine unsetopt */
			opt = 0U;
		} else {
			/* found rubbish then */
			mp += sizeof(magic) - 1U;
			continue;
		}
#define CMP(x, lit)	(strncmp((x), (lit), sizeof(lit) - 1))
		/* parse the option value */
		if ((mp += sizeof(magic) - 1U) == NULL) {
			;
		} else if (CMP(mp, "verbose\n") == 0) {
			ctx->verbosep = opt;
		} else if (CMP(mp, "pseudo-tty\n") == 0) {
			ctx->ptyp = opt;
		} else if (CMP(mp, "timeout") == 0) {
			const char *arg = mp + sizeof("timeout");
			char *p;
			long unsigned int timeo;

			if ((timeo = strtoul(arg, &p, 0), *p == '\n')) {
				ctx->timeo = (unsigned int)timeo;
			}
		} else if (CMP(mp, "keep-going\n") == 0) {
			ctx->keep_going_p = opt;
		}
#undef CMP
	}
	return 0;
}

static int
init_chld(struct clit_chld_s ctx[static 1] __attribute__((unused)))
{
	/* set up the set of fatal signals */
	sigemptyset(fatal_signal_set);
	sigaddset(fatal_signal_set, SIGHUP);
	sigaddset(fatal_signal_set, SIGQUIT);
	sigaddset(fatal_signal_set, SIGINT);
	sigaddset(fatal_signal_set, SIGTERM);
	sigaddset(fatal_signal_set, SIGXCPU);
	sigaddset(fatal_signal_set, SIGXFSZ);
	/* also the empty set */
	sigemptyset(empty_signal_set);
	return 0;
}

static int
fini_chld(struct clit_chld_s ctx[static 1] __attribute__((unused)))
{
	return 0;
}

static void
mkfifofn(char *restrict buf, size_t bsz, const char *key, unsigned int tid)
{
	snprintf(buf, bsz, "%s output  %x", key, tid);
	return;
}

static pid_t
feeder(clit_bit_t exp, int expfd)
{
	pid_t feed;

	switch ((feed = fork())) {
	case -1:
		/* ah good then */
		break;
	case 0:;
		/* i am the child */
		ssize_t nwr;

		while (exp.z > 0 &&
		       (nwr = write(expfd, exp.d, exp.z)) > 0) {
			exp.d += nwr;
			if ((size_t)nwr <= exp.z) {
				exp.z -= nwr;
			} else {
				exp.z = 0;
			}
		}

		/* we're done */
		close(expfd);

		/* and out, always succeed */
		exit(EXIT_SUCCESS);
	default:
		/* i'm the parent */
		break;
	}
	return feed;
}

static pid_t
xpnder(clit_bit_t exp, int expfd)
{
	pid_t feed;

	switch ((feed = fork())) {
	case -1:
		/* ah good then */
		break;
	case 0:;
		/* i am the child */
		ssize_t nwr;
		int xin[2U];
		pid_t sh;

		if (UNLIKELY(pipe(xin) < 0)) {
		fail:
			/* whatever */
			exit(EXIT_FAILURE);
		}

		switch ((sh = fork())) {
			static char *const sh_args[] = {"sh", "-s", NULL};
		case -1:
			/* big fucking problem */
			goto fail;
		case 0:
			/* close write end of pipe */
			close(xin[1U]);
			/* redir xin[0U] -> stdin */
			dup2(xin[0U], STDIN_FILENO);
			/* close read end of pipe */
			close(xin[0U]);

			/* redir stdout -> expfd */
			dup2(expfd, STDOUT_FILENO);
			/* close expfd */
			close(expfd);

			/* child again */
			execv("/bin/sh", sh_args);
			exit(EXIT_SUCCESS);
		default:
			/* parent i am */
			close(xin[0U]);
			/* also forget about expfd */
			close(expfd);
			break;
		}

		if (write(xin[1U], "cat <<EOF\n", 10U) < 10) {
			goto fail;
		}
		while (exp.z > 0 &&
		       (nwr = write(xin[1U], exp.d, exp.z)) > 0) {
			exp.d += nwr;
			if ((size_t)nwr <= exp.z) {
				exp.z -= nwr;
			} else {
				exp.z = 0;
			}
		}
		if (write(xin[1U], "EOF\n", 4U) < 4) {
			goto fail;
		}

		/* we're done */
		close(xin[1U]);

		/* wait for child process */
		with (int st) {
			while (waitpid(sh, &st, 0) != sh);
		}

		/* and out, always succeed */
		exit(EXIT_SUCCESS);
	default:
		/* i'm the parent */
		break;
	}
	return feed;
}

static pid_t
differ(struct clit_chld_s ctx[static 1], clit_bit_t exp, bool xpnd_proto_p)
{
#if !defined L_tmpnam
# define L_tmpnam	(PATH_MAX)
#endif	/* !L_tmpnam */
	static char expfn[PATH_MAX];
	static char actfn[PATH_MAX];
	pid_t difftool = -1;

	assert(!clit_bit_fd_p(exp));

	if (clit_bit_fn_p(exp) &&
	    (strlen(exp.d) >= sizeof(expfn) || strcpy(expfn, exp.d) == NULL)) {
		error("cannot prepare in file `%s'", exp.d);
		goto out;
	} else if (!clit_bit_fn_p(exp) &&
		   (mkfifofn(expfn, sizeof(expfn), "expected", ctx->test_id),
		    mkfifo(expfn, 0666) < 0)) {
		error("cannot create fifo `%s'", expfn);
		goto out;
	} else if (mkfifofn(actfn, sizeof(actfn), "actual", ctx->test_id),
		   mkfifo(actfn, 0666) < 0) {
		error("cannot create fifo `%s'", actfn);
		goto out;
	}

	block_sigs();

	switch ((difftool = vfork())) {
	case -1:
		/* i am an error */
		error("vfork for diff failed");
		break;

	case 0:;
		/* i am the child */
		static char *const diff_opt[] = {
			"diff",
			"-u",
			expfn, actfn, NULL,
		};

		unblock_sigs();

		/* don't allow input at all */
		close(STDIN_FILENO);

		/* diff stdout -> stderr */
		dup2(STDERR_FILENO, STDOUT_FILENO);

		execvp(cmd_diff, diff_opt);

		/* just unlink the files the WRONLY is waiting for
		 * ACTFN is always something that we create and unlink,
		 * so delete that one now to trigger an error in the
		 * parent's open() code below */
		unlink(actfn);
		/* EXPFN is opened in the parent code below if it's
		 * a fifo created by us, unlink that one to break the hang */
		if (clit_bit_buf_p(exp)) {
			unlink(expfn);
		}
		_exit(EXIT_FAILURE);

	default:;
		/* i am the parent */
		static const int ofl = O_WRONLY;
		int expfd = -1;
		int actfd = -1;

		/* clean up descriptors */
		if (clit_bit_buf_p(exp) &&
		    (expfd = open(expfn, ofl, 0666)) < 0) {
			goto clobrk;
		} else if ((actfd = open(actfn, ofl, 0666)) < 0) {
			goto clobrk;
		}

		/* assign actfd as out descriptor */
		ctx->pou = actfd;

		/* fork out the feeder guy */
		if (clit_bit_buf_p(exp)) {
			/* check if we need the expander */
			if (LIKELY(!xpnd_proto_p)) {
				ctx->feed = feeder(exp, expfd);
			} else {
				ctx->feed = xpnder(exp, expfd);
			}
			/* forget about expfd lest we leak it */
			close(expfd);
		} else {
			/* best to let everyone know that we chose
			 * not to use a feeder */
			ctx->feed = -1;
		}
		break;
	clobrk:
		error("exec'ing %s failed", cmd_diff);
		if (expfd >= 0) {
			close(expfd);
		}
		if (actfd >= 0) {
			close(actfd);
		}
		kill(difftool, SIGTERM);
		difftool = -1;
		break;
	}

	unblock_sigs();
out:
	if (*expfn && !clit_bit_fn_p(exp)) {
		unlink(expfn);
	}
	if (*actfn) {
		unlink(actfn);
	}
	return difftool;
}

static int
init_tst(struct clit_chld_s ctx[static 1], struct clit_tst_s tst[static 1])
{
/* set up a connection with /bin/sh to pipe to and read from */
	int pty;
	int pin[2];
	int per[2];

	/* obtain a test id */
	with (struct timeval tv[1]) {
		(void)gettimeofday(tv, NULL);
		ctx->test_id = (unsigned int)(tv->tv_sec ^ tv->tv_usec);
	}

	if (!tst->supp_diff) {
		ctx->diff = differ(ctx, tst->out, tst->xpnd_proto);
	} else {
		ctx->diff = -1;
		ctx->feed = -1;
		ctx->pou = -1;
	}

	if (0) {
		;
	} else if (UNLIKELY(pipe(pin) < 0)) {
		ctx->chld = -1;
		return -1;
	} else if (UNLIKELY(ctx->ptyp && pipe(per) < 0)) {
		ctx->chld = -1;
		return -1;
	}

	block_sigs();
	switch ((ctx->chld = LIKELY(!ctx->ptyp) ? vfork() : pfork(&pty))) {
	case -1:
		/* i am an error */
		unblock_sigs();
		return -1;

	case 0:
		/* i am the child */
		unblock_sigs();
		if (UNLIKELY(ctx->ptyp)) {
			/* in pty mode connect child's stderr to parent's */
			;
		}

		/* read from pin and write to pou */
		if (LIKELY(!ctx->ptyp)) {
			/* pin[0] ->stdin */
			dup2(pin[0], STDIN_FILENO);
		} else {
			dup2(per[1], STDERR_FILENO);
			close(per[0]);
			close(per[1]);
		}
		close(pin[0]);
		close(pin[1]);

		/* stdout -> pou[1] */
		if (!tst->supp_diff) {
			dup2(ctx->pou, STDOUT_FILENO);
			close(ctx->pou);
		}

		execl("/bin/sh", "sh", NULL);
		error("exec'ing /bin/sh failed");
		_exit(EXIT_FAILURE);

	default:
		/* i am the parent, clean up descriptors */
		close(pin[0]);
		if (UNLIKELY(ctx->ptyp)) {
			close(pin[1]);
		}
		if (LIKELY(ctx->pou >= 0)) {
			close(ctx->pou);
		}
		ctx->pou = -1;

		/* assign desc, write end of pin */
		if (LIKELY(!ctx->ptyp)) {
			ctx->pin = pin[1];
		} else {
			ctx->pin = pty;
			ctx->per = per[0];
			close(per[1]);
		}
		break;
	}
	return 0;
}

static int
run_tst(struct clit_chld_s ctx[static 1], struct clit_tst_s tst[static 1])
{
	int rc = 0;
	int st;

	if (UNLIKELY(init_tst(ctx, tst) < 0)) {
		rc = -1;
		if (ctx->feed > 0) {
			kill(ctx->feed, SIGTERM);
		}
		if (ctx->diff > 0) {
			kill(ctx->diff, SIGTERM);
		}
		goto wait;
	}
	with (const char *p = tst->cmd.d, *const ep = tst->cmd.d + tst->cmd.z) {
		for (ssize_t nwr;
		     p < ep && (nwr = write(ctx->pin, p, ep - p)) > 0;
		     p += nwr);
	}
	unblock_sigs();

	if (LIKELY(!ctx->ptyp) ||
	    write(ctx->pin, "exit $?\n", 8U) < 8) {
		/* indicate we're not writing anymore on the child's stdin
		 * or in case of a pty, send exit command and keep fingers
		 * crossed the pty will close itself */
		close(ctx->pin);
	}

	/* wait for the beef child */
	while (ctx->chld > 0 && waitpid(ctx->chld, &st, 0) != ctx->chld);
	if (LIKELY(ctx->chld > 0 && WIFEXITED(st))) {
		rc = WEXITSTATUS(st);

		if (tst->exp_ret == rc) {
			rc = 0;
		} else if (tst->exp_ret == 255U && rc) {
			rc = 0;
		} else {
			rc = 1;
		}
	} else {
		rc = 1;
	}

wait:
	/* wait for the feeder */
	while (ctx->feed > 0 && waitpid(ctx->feed, &st, 0) != ctx->feed);
	if (LIKELY(ctx->feed > 0 && WIFEXITED(st))) {
		int tmp_rc = WEXITSTATUS(st);

		if (tst->ign_out) {
			/* don't worry */
			;
		} else if (tmp_rc > rc) {
			rc = tmp_rc;
		}
	}

	/* finally wait for the differ */
	while (ctx->diff > 0 && waitpid(ctx->diff, &st, 0) != ctx->diff);
	if (LIKELY(ctx->diff > 0 && WIFEXITED(st))) {
		int tmp_rc = WEXITSTATUS(st);

		if (tst->ign_out) {
			/* don't worry */
			;
		} else if (tmp_rc > rc) {
			rc = tmp_rc;
		}
	}

	/* and after all, if we ignore the rcs just reset them to zero */
	if (tst->ign_ret) {
		rc = 0;
	}

#if defined HAVE_PTY_H
	if (UNLIKELY(ctx->ptyp)) {
		/* also close child's stdin here */
		close(ctx->pin);
	}

	/* also connect per's out end with stderr */
	if (UNLIKELY(ctx->ptyp)) {
# if defined HAVE_SPLICE
#  if !defined SPLICE_F_MOVE
#   define SPLICE_F_MOVE		(0)
#  endif  /* SPLICE_F_MOVE */
		for (ssize_t nsp;
		     (nsp = splice(
			      ctx->per, NULL, STDERR_FILENO, NULL,
			      4096U, SPLICE_F_MOVE)) == 4096U;);
# endif	/* HAVE_SPLICE */
		close(ctx->per);
	}
#endif	/* HAVE_PTY_H */
	return rc;
}

static void
set_timeout(unsigned int tdiff)
{
	if (UNLIKELY(tdiff == 0U)) {
		return;
	}

	/* unblock just this one signal */
	unblock_sig(SIGALRM);
	alarm(tdiff);
	return;
}

static void
prepend_path(const char *p)
{
#define free_path()	prepend_path(NULL);
	static char *paths;
	static size_t pathz;
	static char *restrict pp;
	size_t pz;

	if (UNLIKELY(p == NULL)) {
		/* freeing */
		if (paths == NULL) {
			free(paths);
			paths = pp = NULL;
		}
		return;
	}
	/* otherwise it'd be safe to compute the strlen() methinks */
	pz = strlen(p);

	if (UNLIKELY(paths == NULL)) {
		char *envp;

		if (LIKELY((envp = getenv("PATH")) != NULL)) {
			const size_t envz = strlen(envp);

			/* get us a nice big cushion */
			pathz = ((envz + pz + 1U/*\nul*/) / 256U + 2U) * 256U;
			if (UNLIKELY((paths = malloc(pathz)) == NULL)) {
				/* don't bother then */
				return;
			}
			/* set pp for further reference */
			pp = (paths + pathz) - (envz + 1U/*\nul*/);
			/* glue the current path at the end of the array */
			memccpy(pp, envp, '\0', envz);
			/* terminate pp at least at the very end */
			pp[envz] = '\0';
		} else {
			/* just alloc space for P */
			pathz = ((pz + 1U/*\nul*/) / 256U + 2U) * 256U;
			if (UNLIKELY((paths = malloc(pathz)) == NULL)) {
				/* don't bother then */
				return;
			}
			/* set pp for further reference */
			pp = (paths + pathz) - (pz + 1U/*\nul*/);
			/* copy P and then exit */
			memcpy(pp, p, pz + 1U/*\nul*/);
			goto out;
		}
	}

	/* calc prepension pointer */
	pp -= pz + 1U/*:*/;

	if (UNLIKELY(pp < paths)) {
		/* awww, not enough space, is there */
		ptrdiff_t ppoff = pp - paths;
		size_t newsz = ((pathz + pz + 1U/*:*/) / 256U + 1U) * 256U;

		if (UNLIKELY((paths = realloc(paths, newsz)) == NULL)) {
			/* just leave things be */
			return;
		}
		/* memmove to the back */
		memmove(paths + (newsz - pathz), paths, pathz);
		/* recalc paths pointer */
		pp = paths + (newsz - pathz) + ppoff;
		pathz = newsz;
	}

	/* actually prepend now */
	memcpy(pp, p, pz);
	pp[pz] = ':';
out:
	setenv("PATH", pp, 1);
	return;
}


static int verbosep;
static int ptyp;
static int keep_going_p;
static unsigned int timeo;

static int
test_f(clitf_t tf)
{
	static struct clit_chld_s ctx[1];
	static struct clit_tst_s tst[1];
	const char *bp = tf.d;
	size_t bz = tf.z;
	int rc = 0;

	if (UNLIKELY(init_chld(ctx) < 0)) {
		return -1;
	}

	/* preset options */
	if (verbosep) {
		ctx->verbosep = 1U;
	}
	if (ptyp) {
		ctx->ptyp = 1U;
	}
	if (keep_going_p) {
		ctx->keep_going_p = 1U;
	}
	ctx->timeo = timeo;

	/* find options in the test script */
	find_opt(ctx, bp, bz);

	/* prepare */
	if (ctx->timeo > 0) {
		set_timeout(ctx->timeo);
	}
	for (; find_tst(tst, bp, bz) == 0; bp = tst->rest.d, bz = tst->rest.z) {
		if (ctx->verbosep) {
			fputs("$ ", stderr);
			fwrite(tst->cmd.d, sizeof(char), tst->cmd.z, stderr);
		}
		with (int tst_rc = run_tst(ctx, tst)) {
			if (ctx->verbosep) {
				fprintf(stderr, "$? %d\n", tst_rc);
			}
			rc = rc ?: tst_rc;
		}
		if (rc && !ctx->keep_going_p) {
			break;
		}
	}
	if (UNLIKELY(fini_chld(ctx)) < 0) {
		rc = -1;
	}
	return rc;
}

static int
test(const char *testfile)
{
	int fd;
	struct stat st;
	clitf_t tf;
	int rc = -1;

	if ((fd = open(testfile, O_RDONLY)) < 0) {
		error("Error: cannot open file `%s'", testfile);
		goto out;
	} else if (fstat(fd, &st) < 0) {
		error("Error: cannot stat file `%s'", testfile);
		goto clo;
	} else if ((tf = mmap_fd(fd, st.st_size)).d == NULL) {
		error("Error: cannot map file `%s'", testfile);
		goto clo;
	}
	/* yaay, perform the test */
	rc = test_f(tf);

	/* and out we are */
	munmap_fd(tf);
clo:
	close(fd);
out:
	return rc;
}


#include "clitoris.yucc"

int
main(int argc, char *argv[])
{
	yuck_t argi[1U];
	int rc = 99;

	if (yuck_parse(argi, argc, argv)) {
		goto out;
	} else if (argi->nargs != 1U) {
		yuck_auto_help(argi);
		goto out;
	}

	if (argi->builddir_arg) {
		setenv("builddir", argi->builddir_arg, 1);
	}
	if (argi->srcdir_arg) {
		setenv("srcdir", argi->srcdir_arg, 1);
	}
	if (argi->hash_arg) {
		setenv("hash", argi->hash_arg, 1);
	}
	if (argi->husk_arg) {
		setenv("husk", argi->husk_arg, 1);
	}
	if (argi->verbose_flag) {
		verbosep = 1U;
	}
	if (argi->pseudo_tty_flag) {
		ptyp = 1U;
	}
	if (argi->timeout_arg) {
		timeo = strtoul(argi->timeout_arg, NULL, 10);
	}
	if (argi->keep_going_flag) {
		keep_going_p = 1U;
	}
	if (argi->diff_arg) {
		cmd_diff = argi->diff_arg;
	} else if (getenv("DIFF") != NULL) {
		cmd_diff = getenv("DIFF");
	}

	/* prepend our current directory and our argv[0] directory */
	with (char *arg0 = argv[0]) {
		char *dir0;
		if ((dir0 = strrchr(arg0, '/')) != NULL) {
			*dir0 = '\0';
			prepend_path(arg0);
		}
	}
	prepend_path(".");
	/* also bang builddir to path */
	with (char *blddir = getenv("builddir")) {
		if (LIKELY(blddir != NULL)) {
			/* use at most 256U bytes for blddir */
			char _blddir[256U];

			memccpy(_blddir, blddir, '\0', sizeof(_blddir) - 1U);
			_blddir[sizeof(_blddir) - 1U] = '\0';
			prepend_path(_blddir);
		}
	}

	/* just to be clear about this */
#if defined WORDS_BIGENDIAN
	setenv("endian", "big", 1);
#else  /* !WORDS_BIGENDIAN */
	setenv("endian", "little", 1);
#endif	/* WORDS_BIGENDIAN */

	if ((rc = test(argi->args[0U])) < 0) {
		rc = 99;
	}

	/* resource freeing */
	free_path();
out:
	yuck_free(argi);
	return rc;
}

/* clitoris.c ends here */
