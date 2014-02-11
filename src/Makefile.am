### Makefile.am -- uterus src/ make file
##
## Copyright (C) 2008-2010  Sebastian Freundt
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

AM_CPPFLAGS = -D_GNU_SOURCE -D_XOPEN_SOURCE=600 -D_POSIX_C_SOURCE=201001L
AM_CPPFLAGS += -DDEBUG -DTIMESTAMPS
AM_LDFLAGS = $(XCCLDFLAGS)

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
CLEANFILES =

BUILT_SOURCES =
GGO_HELPERS = common.ggo.in

DSO_LDFLAGS = -shared -avoid-version -module

## version stuff
noinst_LIBRARIES += libversion.a
libversion_a_SOURCES =
libversion_a_SOURCES += version.h
nodist_libversion_a_SOURCES = version.c
CLEANFILES += version.c
EXTRA_DIST += version.c.in

## the main thing
lib_LTLIBRARIES += libuterus.la
header_HEADERS += uterus.h
header_HEADERS += utefile.h
header_HEADERS += utehdr.h
header_HEADERS += date.h
header_HEADERS += tzraw.h
header_HEADERS += scommon.h
header_HEADERS += sl1t.h
header_HEADERS += scdl.h
header_HEADERS += ssnp.h
header_HEADERS += boobs.h
header_HEADERS += m30.h
header_HEADERS += m62.h
header_HEADERS += m30-rela.h
header_HEADERS += m62-rela.h
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
## monetary_t reader and writer
libuterus_la_SOURCES += m30.c
libuterus_la_SOURCES += m62.c
## libdatrie again
libuterus_la_CPPFLAGS += -DSTATIC_TRIE_GUTS
libuterus_la_SOURCES += uteslut-trie-glue.h
libuterus_la_SOURCES += trie.c trie.h
libuterus_la_CPPFLAGS += -DLIBMODE -fPIC
libuterus_la_CPPFLAGS += -DUSE_DATRIE -DUSE_UTE_SORT
libuterus_la_CPPFLAGS += $(lzma_CFLAGS)
libuterus_la_LDFLAGS = $(AM_LDFLAGS)
libuterus_la_LDFLAGS += $(lzma_LIBS)
libuterus_la_LDFLAGS += -version-info 0:3:0
EXTRA_libuterus_la_SOURCES += triedefs.h
EXTRA_libuterus_la_SOURCES += fileutils.c fileutils.h
EXTRA_libuterus_la_SOURCES += darray.c darray.h
EXTRA_libuterus_la_SOURCES += tail.c tail.h
EXTRA_libuterus_la_SOURCES += fmcmb.h
EXTRA_libuterus_la_SOURCES += intvtree.c intvtree.h
EXTRA_libuterus_la_SOURCES += nifty.h
EXTRA_libuterus_la_SOURCES += cmd-aux.c

uterus_LIBS = libuterus.la
uterus_LIBS += libversion.a

bin_PROGRAMS += ute
ute_SOURCES = ute.c ute.yuck
ute_LDADD = $(uterus_LIBS)
ute_CPPFLAGS = $(AM_CPPFLAGS) -DUTEDIR=\"$(utedir)\"
ute_LDFLAGS = $(AM_LDFLAGS) -static
BUILT_SOURCES += ute.yucc

UTE_CMD_CPPFLAGS = -DSTANDALONE

ute_PROGRAMS += ute-shnot
ute_shnot_SOURCES = ute-shnot.c ute-shnot.h ute-shnot.ggo
ute_shnot_CPPFLAGS = $(AM_CPPFLAGS) $(UTE_CMD_CPPFLAGS)
ute_shnot_LDFLAGS = $(AM_LDFLAGS) -static
ute_shnot_LDADD = $(uterus_LIBS)
BUILT_SOURCES += ute-shnot.x ute-shnot.xh

ute_PROGRAMS += ute-chndl
ute_chndl_SOURCES = ute-chndl.c ute-chndl.h ute-chndl.ggo
ute_chndl_CPPFLAGS = $(AM_CPPFLAGS) $(UTE_CMD_CPPFLAGS)
ute_chndl_LDFLAGS = $(AM_LDFLAGS) -static
ute_chndl_LDADD = $(uterus_LIBS)
BUILT_SOURCES += ute-chndl.x ute-chndl.xh

ute_PROGRAMS += ute-print
ute_print_SOURCES = ute-print.c ute-print.h ute-print.ggo
ute_print_SOURCES += module.c module.h
ute_print_CPPFLAGS = $(AM_CPPFLAGS) $(UTE_CMD_CPPFLAGS)
ute_print_CPPFLAGS += $(LTDLINCL)
ute_print_LDFLAGS = $(AM_LDFLAGS) $(LD_EXPORT_DYNAMIC)
ute_print_LDADD = $(uterus_LIBS)
ute_print_LDADD += $(LIBLTDL)
EXTRA_ute_print_DEPENDENCIES = $(LTDLDEPS)
BUILT_SOURCES += ute-print.x ute-print.xh

ute_PROGRAMS += ute-mux
ute_mux_SOURCES = ute-mux.c ute-mux.h ute-mux.ggo
ute_mux_SOURCES += module.c module.h
ute_mux_CPPFLAGS = $(AM_CPPFLAGS) $(UTE_CMD_CPPFLAGS)
ute_mux_CPPFLAGS += -DUTEDIR=\"$(utedir)\"
ute_mux_CPPFLAGS += $(LTDLINCL)
ute_mux_LDFLAGS = $(AM_LDFLAGS) $(LD_EXPORT_DYNAMIC)
ute_mux_LDADD = $(uterus_LIBS)
ute_mux_LDADD += $(LIBLTDL)
EXTRA_ute_mux_DEPENDENCIES = $(LTDLDEPS)
BUILT_SOURCES += ute-mux.x ute-mux.xh

ute_PROGRAMS += ute-fsck
ute_fsck_SOURCES = ute-fsck.c ute-fsck.ggo
ute_fsck_CPPFLAGS = $(AM_CPPFLAGS) $(UTE_CMD_CPPFLAGS)
ute_fsck_LDFLAGS = $(AM_LDFLAGS) -static
ute_fsck_LDADD = $(uterus_LIBS)
BUILT_SOURCES += ute-fsck.x ute-fsck.xh

ute_PROGRAMS += ute-slut
ute_slut_SOURCES = ute-slut.c ute-slut.ggo
ute_slut_CPPFLAGS = $(AM_CPPFLAGS) $(UTE_CMD_CPPFLAGS)
ute_slut_LDFLAGS = $(AM_LDFLAGS) -static
ute_slut_LDADD = $(uterus_LIBS)
BUILT_SOURCES += ute-slut.x ute-slut.xh

ute_PROGRAMS += ute-slab
ute_slab_SOURCES = ute-slab.c ute-slab.ggo
ute_slab_CPPFLAGS = $(AM_CPPFLAGS) $(UTE_CMD_CPPFLAGS)
ute_slab_LDFLAGS = $(AM_LDFLAGS) -static
ute_slab_LDADD = $(uterus_LIBS)
BUILT_SOURCES += ute-slab.x ute-slab.xh

ute_PROGRAMS += ute-info
ute_info_SOURCES = ute-info.c ute-info.ggo
ute_info_CPPFLAGS = $(AM_CPPFLAGS) $(UTE_CMD_CPPFLAGS)
ute_info_LDFLAGS = $(AM_LDFLAGS) -static
ute_info_LDADD = $(uterus_LIBS)
EXTRA_ute_info_SOURCES = gbs.c gbs.h
BUILT_SOURCES += ute-info.x ute-info.xh

ute_PROGRAMS += ute-anal
ute_anal_SOURCES = ute-anal.c ute-anal.ggo
ute_anal_CPPFLAGS = $(AM_CPPFLAGS) $(UTE_CMD_CPPFLAGS)
ute_anal_LDFLAGS = $(AM_LDFLAGS) -static
if HAVE_LIBPNG
ute_anal_LDFLAGS += -lpng
endif  HAVE_LIBPNG
ute_anal_LDADD = $(uterus_LIBS)
BUILT_SOURCES += ute-anal.x ute-anal.xh

## muxer/demuxer DSOs
ute_LTLIBRARIES += uta.la
uta_la_SOURCES = uta.c
uta_la_LDFLAGS = $(DSO_LDFLAGS) $(XCCLDFLAGS)

ute_LTLIBRARIES += dukas.la
dukas_la_SOURCES = dukas.c
dukas_la_LIBADD = libversion.a
dukas_la_LDFLAGS = $(DSO_LDFLAGS) $(XCCLDFLAGS)
## dukas has a binary too
noinst_PROGRAMS += ute-mux-dukas
ute_mux_dukas_SOURCES = dukas.c dukas.ggo
ute_mux_dukas_CPPFLAGS = $(AM_CPPFLAGS) $(UTE_CMD_CPPFLAGS)
ute_mux_dukas_LDFLAGS = $(AM_LDFLAGS)
ute_mux_dukas_LDADD = $(uterus_LIBS)
BUILT_SOURCES += dukas.x dukas.xh

ute_LTLIBRARIES += mat.la
mat_la_SOURCES = mat.c
mat_la_LDFLAGS = $(DSO_LDFLAGS) $(XCCLDFLAGS)

if HAVE_HDF5
ute_LTLIBRARIES += hdf5.la
hdf5_la_SOURCES = hdf5.c hdf5.ggo
hdf5_la_CPPFLAGS = $(AM_CPPFLAGS) $(HDF5_CPPFLAGS)
hdf5_la_LIBADD = libversion.a
hdf5_la_LDFLAGS = $(DSO_LDFLAGS) $(XCCLDFLAGS) $(HDF5_LDFLAGS)
endif  ## HAVE_HDF5
BUILT_SOURCES += hdf5.x hdf5.xh

ute_LTLIBRARIES += ariva.la
ariva_la_SOURCES = ariva.c
ariva_la_LDFLAGS = $(DSO_LDFLAGS) $(XCCLDFLAGS)

ute_LTLIBRARIES += truefx.la
truefx_la_SOURCES = truefx.c
truefx_la_LDFLAGS = $(DSO_LDFLAGS) $(XCCLDFLAGS)

if HAVE_EXPAT
ute_LTLIBRARIES += ibhist.la
ibhist_la_SOURCES = ibhist.c
ibhist_la_SOURCES += expobuf.h
ibhist_la_CPPFLAGS = $(AM_CPPFLAGS) $(expat_CFLAGS)
ibhist_la_LDFLAGS = $(DSO_LDFLAGS) $(XCCLDFLAGS)
ibhist_la_LDFLAGS += $(expat_LIBS)
endif

ute_LTLIBRARIES += forexfactory.la
forexfactory_la_SOURCES = forexfactory.c
forexfactory_la_LDFLAGS = $(DSO_LDFLAGS) $(XCCLDFLAGS)

ute_LTLIBRARIES += fxstreet.la
fxstreet_la_SOURCES = fxstreet.c
fxstreet_la_LDFLAGS = $(DSO_LDFLAGS) $(XCCLDFLAGS)

version.c: version.c.in $(top_builddir)/version.mk
	sed -e 's,[@]VERSION[@],$(VERSION),g' <$(srcdir)/$@.in >$@

## gengetopt rule with sed hack to avoid using VERSION and config.h
## depend on Makefile.am to survive a git pull
%.x %.xh: %.ggo $(GGO_HELPERS) Makefile.am
	$(M4) -I $(srcdir) common.ggo.in $< | \
	gengetopt --no-help -c x -H xh -l -F $* \
	&& sed \
	  -e 's,config.h,version.h,g' \
	  -e 's,\(.*\)_PARSER_VERSION VERSION,\1_PARSER_VERSION $(PACKAGE)_version_string,g' \
	  $*.xh >$*.xh-t \
	&& mv $*.xh-t $*.xh

## yuck suffix recipe
SUFFIXES = .yuck .yucc
.yuck.yucc:
	$(AM_V_GEN) PATH=$(top_builddir)/build-aux:"$${PATH}" \
		yuck$(EXEEXT) gen -o $@ $< || { rm -f $@; false; }

## for dist or dist check we need binaries, so help2man can work
dist-hook: $(bin_PROGRAMS) $(ute_PROGRAMS)

## hack to build libtdl (at least initially) if dist-hook binaries need it
if DESCEND_LIBLTDL
$(LTDLDEPS):
	cd ../libltdl && $(MAKE)
endif

# 
# Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(cflags) $<

