/*** module.c -- the module loader
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

#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <limits.h>
#include <ltdl.h>
/* the public module stuff */
#include "module.h"

#if !defined P_libexecdir
# define P_libexecdir	"/libexec/ute"
#endif	/* P_libexecdir */

static bool
__dirp(const char *file)
{
	struct stat st;
	return (stat(file, &st) == 0) && S_ISDIR(st.st_mode);
}

static void
add_myself(void)
{
	const char myself[] = "/proc/self/exe";
	const char libdir[] = P_libexecdir;
	char wd[PATH_MAX], *dp;
	size_t sz;

	sz = readlink(myself, wd, sizeof(wd));
	wd[sz] = '\0';
	/* strip the /ute* bit */
	if ((dp = strrchr(wd, '/')) == NULL) {
		return;
	}
	/* add the path where the binary resides */
	*dp = '\0';
	lt_dladdsearchdir(wd);

	/* check if libdir/P_libexecdir exists in its own right */
	if (__dirp(libdir)) {
		/* add libdir in its own right */
		lt_dladdsearchdir(libdir);
		return;
	}

	/* go back to /bin */
	if ((dp = strrchr(wd, '/')) == NULL) {
		return;
	}
	/* add the path where the binary resides + ../lib/unserding/ */
	strncpy(dp, libdir, sizeof(libdir));
	if (__dirp(libdir)) {
		/* add this constructed libdir then */
		lt_dladdsearchdir(wd);
	}
	return;
}


/* public funs */
ute_dso_t
open_aux(const char *fname)
{
	lt_dlhandle handle = 0;
	lt_dladvise advice[1];

	if (!lt_dladvise_init(advice) &&
	    !lt_dladvise_ext(advice) &&
	    !lt_dladvise_global(advice)) {
		handle = lt_dlopenadvise(fname, advice[0]);
	}
	lt_dladvise_destroy(advice);
	return handle;
}

void
close_aux(ute_dso_t handle)
{
	lt_dlclose(handle);
	return;
}

ute_dso_sym_t
find_sym(ute_dso_t handle, const char *sym_name)
{
	return lt_dlsym(handle, sym_name);
}


void
ute_module_init(void)
{
	/* initialise the dl system */
	lt_dlinit();

	/* add current exec path to search path */
	add_myself();
	return;
}

void
ute_module_fini(void)
{
	return;
}

/* module.c ends here */
