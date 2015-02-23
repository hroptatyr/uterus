### Makefile.am -- uterus master make file
##
## Copyright (C) 2008-2015  Sebastian Freundt
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

include $(top_builddir)/version.mk

LANG = C
LC_ALL = C

ACLOCAL_AMFLAGS = -I m4
EXTRA_DIST = $(BUILT_SOURCES)
CLEANFILES =
DISTCLEANFILES =
BUILT_SOURCES =
SUBDIRS =

SUBDIRS += build-aux
if DESCEND_LIBLTDL
SUBDIRS += libltdl
endif
SUBDIRS += src info aux test

pkgconfigdir = @libdir@/pkgconfig
pkgconfig_DATA =
pkgconfig_DATA += uterus.pc
EXTRA_DIST += uterus.pc.in
DISTCLEANFILES += $(pkgconfig_DATA)

EXTRA_DIST += uterus.magic
EXTRA_DIST += README.md

DISTCLEANFILES += version.mk
EXTRA_DIST += version.mk.in
BUILT_SOURCES += .version

doc_DATA = README.md
EXTRA_DIST += $(doc_DATA)

if HAVE_FILE
pkgdata_DATA = uterus.mgc
endif  HAVE_FILE
DISTCLEANFILES += $(pkgdata_DATA)


## pkg-config rule
uterus.pc: .version uterus.pc.in
	$(AM_V_GEN) PATH="$(top_builddir)/build-aux:$${PATH}" \
		yuck$(EXEEXT) scmver -n \
			--reference .version $(srcdir)/uterus.pc.in | \
		$(SHELL) "$(builddir)/config.status" --file=- > $@

uterus.mgc: uterus.magic
	$(FILE) -C -m $(srcdir)/uterus.magic && mv $(builddir)/uterus.magic.mgc $@

.version:
	$(AM_V_GEN) echo "v$(VERSION)" > $@

## make sure .version is read-only in the dist
dist-hook:
	chmod ugo-w $(distdir)/.version

## Makefile.am ends here
