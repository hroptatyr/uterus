### Makefile.am -- uterus src/ make file
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

AM_CPPFLAGS = -D_GNU_SOURCE -D_POSIX_C_SOURCE=201001L
AM_CPPFLAGS += -DDEBUG -DTIMESTAMPS
AM_LDFLAGS = -static

## headerdir and utedir are defined in c.ac

M4 = m4

bin_PROGRAMS =
noinst_PROGRAMS =
ute_PROGRAMS =
ute_LTLIBRARIES =
lib_LTLIBRARIES =
header_HEADERS =
noinst_LIBRARIES =
EXTRA_DIST = $(BUILT_SOURCES) $(GGO_HELPERS)

BUILT_SOURCES = version.h
GGO_HELPERS = common.ggo.in

DSO_LDFLAGS = -shared

## datrie stuff
noinst_LIBRARIES += libdatrie.a
libdatrie_a_SOURCES =
libdatrie_a_CPPFLAGS = $(AM_CPPFLAGS) -DSTATIC_TRIE_GUTS
libdatrie_a_SOURCES += trie.c trie.h

## buffered writer
lib_LTLIBRARIES += libuterus.la
header_HEADERS += uterus.h
header_HEADERS += utefile.h
header_HEADERS += utehdr.h
header_HEADERS += date.h
header_HEADERS += tzraw.h
header_HEADERS += scommon.h
header_HEADERS += sl1t.h
header_HEADERS += scdl.h
header_HEADERS += m30.h m62.h
libuterus_la_SOURCES =
EXTRA_libuterus_la_SOURCES =
libuterus_la_CPPFLAGS = $(AM_CPPFLAGS)
libuterus_la_SOURCES += mem.h
libuterus_la_SOURCES += strptime.c
libuterus_la_SOURCES += strftime.c
libuterus_la_SOURCES += timegm.c
libuterus_la_SOURCES += mktime.c
libuterus_la_SOURCES += tzraw.c tzraw.h
libuterus_la_SOURCES += utehdr.c utehdr.h
libuterus_la_SOURCES += utefile.c utefile.h utefile-private.h
libuterus_la_SOURCES += utesort.c
libuterus_la_SOURCES += utetpc.c utetpc.h
libuterus_la_SOURCES += uteslut.c uteslut.h
libuterus_la_SOURCES += prchunk.c prchunk.h
## libdatrie again
libuterus_la_CPPFLAGS += -DSTATIC_TRIE_GUTS
libuterus_la_SOURCES += uteslut-trie-glue.h
libuterus_la_SOURCES += trie.c trie.h
libuterus_la_CPPFLAGS += -DLIBMODE -fPIC
libuterus_la_CPPFLAGS += -DUSE_DATRIE -DUSE_UTE_SORT
libuterus_la_LDFLAGS = $(AM_LDFLAGS)
EXTRA_libuterus_la_SOURCES += triedefs.h
EXTRA_libuterus_la_SOURCES += fileutils.c fileutils.h
EXTRA_libuterus_la_SOURCES += darray.c darray.h
EXTRA_libuterus_la_SOURCES += tail.c tail.h
EXTRA_libuterus_la_SOURCES += fmcmb.h
EXTRA_libuterus_la_SOURCES += intvtree.c intvtree.h

bin_PROGRAMS += ute
ute_SOURCES =
EXTRA_ute_SOURCES =
ute_SOURCES += ute.c
ute_LDADD = libuterus.la
ute_CPPFLAGS = $(AM_CPPFLAGS) -DUTEDIR=\"$(utedir)\"
ute_LDFLAGS = $(AM_LDFLAGS)


UTE_CMD_CPPFLAGS = -DSTANDALONE

ute_PROGRAMS += ute-shnot
ute_shnot_SOURCES = ute-shnot.c ute-shnot.h ute-shnot-clo.ggo
ute_shnot_CPPFLAGS = $(AM_CPPFLAGS) $(UTE_CMD_CPPFLAGS)
ute_shnot_LDFLAGS = $(AM_LDFLAGS)
ute_shnot_LDADD = libuterus.la
BUILT_SOURCES += ute-shnot-clo.c ute-shnot-clo.h

ute_PROGRAMS += ute-print
ute_print_SOURCES = ute-print.c ute-print.h ute-print-clo.ggo
ute_print_SOURCES += module.c module.h
ute_print_CPPFLAGS = $(AM_CPPFLAGS) $(UTE_CMD_CPPFLAGS)
ute_print_LDFLAGS = $(AM_LDFLAGS) $(LD_EXPORT_DYNAMIC)
ute_print_LDADD = libuterus.la
ute_print_LDADD += $(LIBLTDL)
BUILT_SOURCES += ute-print-clo.c ute-print-clo.h

ute_PROGRAMS += ute-mux
ute_mux_SOURCES = ute-mux.c ute-mux.h ute-mux-clo.ggo
ute_mux_SOURCES += module.c module.h
ute_mux_CPPFLAGS = $(AM_CPPFLAGS) $(UTE_CMD_CPPFLAGS)
ute_mux_LDFLAGS = $(AM_LDFLAGS) $(LD_EXPORT_DYNAMIC)
ute_mux_LDADD = libuterus.la
ute_mux_LDADD += $(LIBLTDL)
BUILT_SOURCES += ute-mux-clo.c ute-mux-clo.h

ute_PROGRAMS += ute-fsck
ute_fsck_SOURCES = ute-fsck.c ute-fsck-clo.ggo
ute_fsck_CPPFLAGS = $(AM_CPPFLAGS) $(UTE_CMD_CPPFLAGS)
ute_fsck_LDFLAGS = $(AM_LDFLAGS)
ute_fsck_LDADD = libuterus.la
ute_fsck_LDADD += $(LIBLTDL)
BUILT_SOURCES += ute-fsck-clo.c ute-fsck-clo.h

## muxer/demuxer DSOs
ute_LTLIBRARIES += uta.la
uta_la_SOURCES = uta.c
uta_la_LDFLAGS = -module $(DSO_LDFLAGS) $(XCCLDFLAGS)
uta_la_LIBADD = libuterus.la

ute_LTLIBRARIES += dukas.la
dukas_la_SOURCES = dukas.c
dukas_la_LDFLAGS = -module $(DSO_LDFLAGS) $(XCCLDFLAGS)

ute_LTLIBRARIES += mat.la
mat_la_SOURCES = mat.c
mat_la_LDFLAGS = -module $(DSO_LDFLAGS) $(XCCLDFLAGS)

ute_LTLIBRARIES += ariva.la
ariva_la_SOURCES = ariva.c
ariva_la_LDFLAGS = -module $(DSO_LDFLAGS) $(XCCLDFLAGS)
ariva_la_LIBADD = libuterus.la

if HAVE_EXPAT
ute_LTLIBRARIES += ibhist.la
ibhist_la_SOURCES = ibhist.c
ibhist_la_SOURCES += expobuf.h
ibhist_la_CPPFLAGS = $(AM_CPPFLAGS) $(expat_CFLAGS)
ibhist_la_LDFLAGS = -module $(DSO_LDFLAGS) $(XCCLDFLAGS)
ibhist_la_LIBADD = libuterus.la
ibhist_la_LIBADD += $(expat_LIBS)
endif

version.h: $(top_builddir)/.version
	sed 's/^/#define UTE_VERSION	"/; s/$$/"/' $< > $@

## ggo rule
%.c %.h: %.ggo $(GGO_HELPERS)
	$(M4) -I $(srcdir) common.ggo.in $< | gengetopt --no-help -l -F $*

# 
# Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(cflags) $<

