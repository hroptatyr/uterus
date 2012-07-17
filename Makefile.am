### Makefile.am -- uterus master make file
##
## Copyright (C) 2008 - 2010  Sebastian Freundt
##
## Author:  Sebastian Freundt <hroptatyr@fresse.org>
##
## This file is part of uterus.
## 
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions
## are met:
##
## 1. Redistributions of source code must retain the above copyright
##    notice, this list of conditions and the following disclaimer.
##
## 2. Redistributions in binary form must reproduce the above copyright
##    notice, this list of conditions and the following disclaimer in the
##    documentation and/or other materials provided with the distribution.
##
## 3. Neither the name of the author nor the names of any contributors
##    may be used to endorse or promote products derived from this
##    software without specific prior written permission.
##
## THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
## IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
## WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
## DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
## FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
## CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
## SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
## BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
## WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
## OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
## IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
##
###

LANG = C
LC_ALL = C

ACLOCAL_AMFLAGS = -I m4

SUBDIRS =
if DESCEND_LIBLTDL
SUBDIRS += libltdl
endif
SUBDIRS += src info test

pkgconfigdir = @libdir@/pkgconfig
pkgconfig_DATA =
## this will disappear soon and be replaced by uterus.pc entirely
pkgconfig_DATA += libuterus.pc
pkgconfig_DATA += uterus.pc

EXTRA_DIST = .version git-version-gen

# Version string management.  There are two files to be aware of:
# .tarball-version - present only in a distribution tarball, and not in
#   a checked-out repository.  Created with contents that were learned at
#   the last time autoconf was run, and used by git-version-gen.  Must not
#   be present in either $(srcdir) or $(builddir) for git-version-gen to
#   give accurate answers during normal development with a checked out tree,
#   but must be present in a tarball when there is no version control system.
#   Therefore, it cannot be used in any dependencies.  GNUmakefile has
#   hooks to force a reconfigure at distribution time to get the value
#   correct, without penalizing normal development with extra reconfigures.
# .version - present in a checked-out repository and in a distribution
#   tarball.  At least as current as the most recent .tarball-version
#   creation.  Usable in dependencies, particularly for files that don't
#   want to depend on config.h but do want to track version changes.
BUILT_SOURCES = .version
.PHONY: .version
.version: git-version-gen
	$(srcdir)/git-version-gen $@ >/dev/null
