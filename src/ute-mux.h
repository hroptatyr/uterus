/*** ute-mux.h -- muxing external sources
 *
 * Copyright (C) 2009 - 2012 Sebastian Freundt
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
#if !defined INCLUDED_ute_mux_h_
#define INCLUDED_ute_mux_h_

typedef struct mux_ctx_s *mux_ctx_t;
typedef struct sumux_opt_s *sumux_opt_t;

struct mux_ctx_s {
	/* input file index */
	int infd;
	/* err file desc */
	int badfd;

	/* can be used by the muxer */
	void *rdr;
	/* will be used by ute-mux */
	void *wrr;

	/* special service */
	const char *infn;

	/* our options */
	sumux_opt_t opts;
};

struct sumux_opt_s {
	const char *outfile;
	const char *sname;
	const char *zone;
	/** offset for timestamps relative to something other than epoch */
	int32_t tsoff;
	/** tick type as in sl1t.h */
	uint16_t tt;
	/** multiplier for reduced, down-scaled or point-less values */
	int32_t mul;
	/** magnifier for expanded or up-scaled values */
	int32_t mag;

	/* outfile flags */
#define OUTFILE_IS_INTO		(1)
	int flags;
};

/**
 * Public mux function.
 * Implemented through DSOs. */
extern void mux(mux_ctx_t mctx);

#endif	/* INCLUDED_ute_mux_h_ */
