/*** module.h -- libtool based DSO system
 *
 * Copyright (C) 2005 - 2012 Sebastian Freundt
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

#if !defined INCLUDED_module_h_
#define INCLUDED_module_h_

typedef void *ute_dso_t;
typedef void *ute_dso_sym_t;

/**
 * Open FILE as dso and return a handle.
 * FILE should be a muxer or demuxer and provide the symbols:
 * ... */
extern ute_dso_t open_aux(const char *file);

/**
 * Close (read unload) the dso HANDLE. */
extern void close_aux(ute_dso_t handle);

/**
 * Find symbol SYM_NAME in dso HANDLE. */
extern ute_dso_sym_t find_sym(ute_dso_t handle, const char *sym_name);

/**
 * Find all modules and call CB. */
extern int trav_dso(int(*cb)(const char *fname, void *clo), void *clo);

/**
 * Initialise the module system. */
extern void ute_module_init(void);

/**
 * Deinitialise the module system. */
extern void ute_module_fini(void);

#endif	/* INCLUDED_module_h_ */
