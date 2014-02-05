include $(top_builddir)/version.mk

# Help the Developers and yourself. Just use the C locale and settings
# for the compilation. They can still be overriden by make LANG=<whatever>
# but that is general a not very good idea
LANG = C
LC_ALL = C

AM_CPPFLAGS = -D_GNU_SOURCE -D_XOPEN_SOURCE=600 -D_POSIX_C_SOURCE=201001L
AM_CPPFLAGS += -I$(abs_top_srcdir)/src
AM_LDFLAGS = -L$(abs_top_builddir)/src

EXTRA_DIST = $(ut_tests) $(BUILT_SOURCES)
TESTS = $(ut_tests) $(bin_tests) $(XFAIL_TESTS)
XFAIL_TESTS = $(ut_xtests) $(bin_xtests)
BUILT_SOURCES =
check_PROGRAMS =
ut_tests =
ut_xtests =
bin_tests =
bin_xtests =

## we used to provide a separate libm30 but that turned out to be bollocks
m30_LIBS = $(uterus_LIBS)
uterus_LIBS = $(abs_top_builddir)/src/libuterus.la

TEST_EXTENSIONS = .clit
AM_LOG_COMPILER = false

CLIT_LOG_COMPILER = $(builddir)/clitoris
AM_CLIT_LOG_FLAGS = -v -k --builddir $(top_builddir)/src

ut_tests += mux.01.clit
ut_tests += mux.02.clit
ut_tests += mux.03.clit
ut_tests += mux.04.clit
ut_tests += mux.05.clit
ut_tests += mux.06.clit
if HAVE_EXPAT
ut_tests += mux.07.clit
ut_tests += mux.08.clit
ut_tests += mux.09.clit
ut_tests += mux.10.clit
else  ## !HAVE_EXPAT
EXTRA_DIST += mux.07.clit
EXTRA_DIST += mux.08.clit
EXTRA_DIST += mux.09.clit
EXTRA_DIST += mux.10.clit
endif  ## HAVE_EXPAT
ut_tests += mux.11.clit
ut_tests += mux.12.clit
ut_tests += mux.13.clit
ut_tests += mux.14.clit
ut_tests += mux.15.clit
ut_tests += mux.16.clit
ut_tests += mux.17.clit
ut_tests += mux.18.clit
ut_tests += mux.19.clit
ut_tests += mux.20.clit
ut_tests += mux.21.clit
EXTRA_DIST += mux.a.dukasq
EXTRA_DIST += mux.b.ariva
EXTRA_DIST += mux.c.dukas.bi5
EXTRA_DIST += mux.d.ariva
EXTRA_DIST += mux.e.ariva
EXTRA_DIST += mux.f.ibhist.xml
EXTRA_DIST += mux.g.ibhist.xml
EXTRA_DIST += mux.h.dukas.bi5
EXTRA_DIST += mux.i.uta
EXTRA_DIST += mux.j.uta
EXTRA_DIST += mux.k.uta
EXTRA_DIST += mux.l.uta
EXTRA_DIST += mux.m.uta
ut_tests += mux.22.clit

EXTRA_DIST += mux.4.ref.ute
EXTRA_DIST += mux.4.ref.beute
EXTRA_DIST += mux.5.ref.ute
EXTRA_DIST += mux.6.ref.ute
EXTRA_DIST += mux.7.ref.ute
EXTRA_DIST += mux.7.ref.beute
EXTRA_DIST += mux.8.ref.ute
EXTRA_DIST += mux.9.ref.ute
EXTRA_DIST += mux.10.ref.ute
EXTRA_DIST += mux.11.ref.ute
EXTRA_DIST += mux.12.ref.ute
EXTRA_DIST += mux.13.ref.ute

if WORDS_BIGENDIAN
else
ut_tests += print.01.clit
ut_tests += print.02.clit
endif
ut_tests += print.03.clit
ut_tests += print.04.clit
ut_tests += print.05.clit
ut_tests += print.06.clit
ut_tests += print.07.clit
ut_tests += print.08.clit
ut_tests += print.09.clit
ut_tests += print.10.clit
EXTRA_DIST += print.a.ute
EXTRA_DIST += print.b.ute
EXTRA_DIST += print.c.ute
EXTRA_DIST += print.d.ute
EXTRA_DIST += print.e.ute
EXTRA_DIST += print.e.beute
EXTRA_DIST += print.f.ute
EXTRA_DIST += print.f.beute

ut_tests += shnot.01.clit
ut_tests += shnot.02.clit
EXTRA_DIST += shnot.1.ref.ute
EXTRA_DIST += shnot.2.ref.ute

ut_tests += chndl.01.clit
ut_tests += chndl.02.clit
EXTRA_DIST += chndl.2.ref.ute

ut_tests += fsck.01.clit
ut_tests += fsck.02.clit
ut_tests += fsck.03.clit
ut_tests += fsck.04.clit
ut_tests += fsck.05.clit
ut_tests += fsck.06.clit
ut_tests += fsck.07.clit
ut_tests += fsck.08.clit
ut_tests += fsck.09.clit
ut_tests += fsck.10.clit
ut_tests += fsck.11.clit
ut_tests += fsck.12.clit
ut_tests += fsck.13.clit
ut_tests += fsck.14.clit
ut_tests += fsck.15.clit
ut_tests += fsck.16.clit
if HAVE_LZMA
ut_tests += fsck.17.clit
ut_tests += fsck.18.clit
ut_tests += fsck.19.clit
ut_tests += fsck.20.clit
ut_tests += fsck.21.clit
ut_tests += fsck.22.clit
ut_tests += fsck.23.clit
ut_tests += fsck.24.clit
endif  HAVE_LZMA
ut_tests += fsck.25.clit
ut_tests += fsck.26.clit
ut_tests += fsck.27.clit
ut_tests += fsck.28.clit
EXTRA_DIST += fsck.a.ute
EXTRA_DIST += fsck.b.ute
EXTRA_DIST += fsck.17.ref.leute
EXTRA_DIST += fsck.17.ref.beute

ut_tests += slut.01.clit
ut_tests += slut.02.clit
ut_tests += slut.03.clit
ut_tests += slut.04.clit

ut_tests += info.01.clit
ut_tests += info.02.clit
ut_tests += info.03.clit
ut_tests += info.04.clit
ut_tests += info.05.clit
ut_tests += info.06.clit
ut_tests += info.07.clit
ut_tests += info.08.clit
EXTRA_DIST += info.5.ref.ute
EXTRA_DIST += info.5.ref.beute
EXTRA_DIST += info.7.ref.ute
ut_tests += info.09.clit
ut_tests += info.10.clit

## test the core api
check_PROGRAMS += core-file-1
check_PROGRAMS += core-file-2
check_PROGRAMS += core-file-3
check_PROGRAMS += core-file-4

core_file_1_LDFLAGS = $(AM_LDFLAGS) -static
core_file_1_LDADD = $(uterus_LIBS)
core_file_2_LDFLAGS = $(AM_LDFLAGS) -static
core_file_2_LDADD = $(uterus_LIBS)
core_file_3_LDFLAGS = $(AM_LDFLAGS) -static
core_file_3_LDADD = $(uterus_LIBS)
core_file_4_LDFLAGS = $(AM_LDFLAGS) -static
core_file_4_LDADD = $(uterus_LIBS)
bin_tests += core-file-1
bin_tests += core-file-2
bin_tests += core-file-3
bin_tests += core-file-4

## testing headers and stuff
check_PROGRAMS += m30-1
m30_1_LDFLAGS = $(AM_LDFLAGS) -static
m30_1_LDADD = $(m30_LIBS)
bin_tests += m30-1

check_PROGRAMS += m30-2
m30_2_LDFLAGS = $(AM_LDFLAGS) -static
m30_2_LDADD = $(m30_LIBS)
bin_tests += m30-2

check_PROGRAMS += m30-3
m30_3_LDFLAGS = $(AM_LDFLAGS) -static
m30_3_LDADD = $(m30_LIBS)
bin_tests += m30-3

check_PROGRAMS += m30-4
m30_4_LDFLAGS = $(AM_LDFLAGS) -static
m30_4_LDADD = $(m30_LIBS)
bin_tests += m30-4

check_PROGRAMS += m30-5
m30_5_LDFLAGS = $(AM_LDFLAGS) -static
m30_5_LDADD = $(m30_LIBS)
bin_tests += m30-5

check_PROGRAMS += m30-6
m30_6_LDFLAGS = $(AM_LDFLAGS) -static
m30_6_LDADD = $(m30_LIBS)
bin_tests += m30-6

check_PROGRAMS += m30-7
m30_7_LDFLAGS = $(AM_LDFLAGS) -static
m30_7_LDADD = $(m30_LIBS)
bin_tests += m30-7

check_PROGRAMS += m30-8
m30_8_LDFLAGS = $(AM_LDFLAGS) -static
m30_8_LDADD = $(m30_LIBS)
bin_tests += m30-8

check_PROGRAMS += m30-9
m30_9_LDFLAGS = $(AM_LDFLAGS) -static
m30_9_LDADD = $(m30_LIBS)
bin_tests += m30-9

check_PROGRAMS += m30-10
m30_10_LDFLAGS = $(AM_LDFLAGS) -static
m30_10_LDADD = $(m30_LIBS)
bin_tests += m30-10

check_PROGRAMS += m30-11
m30_11_LDFLAGS = $(AM_LDFLAGS) -static
m30_11_LDADD = $(m30_LIBS)
bin_tests += m30-11

check_PROGRAMS += m30-12
m30_12_LDFLAGS = $(AM_LDFLAGS) -static
m30_12_LDADD = $(m30_LIBS)
bin_tests += m30-12

check_PROGRAMS += m30-13
m30_13_LDFLAGS = $(AM_LDFLAGS) -static
m30_13_LDADD = $(m30_LIBS)
bin_tests += m30-13

check_PROGRAMS += m30-14
m30_14_LDFLAGS = $(AM_LDFLAGS) -static
m30_14_LDADD = $(m30_LIBS)
bin_tests += m30-14

check_PROGRAMS += m30-15
m30_15_LDFLAGS = $(AM_LDFLAGS) -static
m30_15_LDADD = $(m30_LIBS)
bin_tests += m30-15

check_PROGRAMS += m30-16
m30_16_LDFLAGS = $(AM_LDFLAGS) -static
m30_16_LDADD = $(m30_LIBS)
bin_tests += m30-16

check_PROGRAMS += m62-1
m62_1_LDFLAGS = $(AM_LDFLAGS) -static
m62_1_LDADD = $(m30_LIBS)
bin_tests += m62-1

check_PROGRAMS += m62-2
m62_2_LDFLAGS = $(AM_LDFLAGS) -static
m62_2_LDADD = $(m30_LIBS)
bin_tests += m62-2

check_PROGRAMS += m62-3
m62_3_LDFLAGS = $(AM_LDFLAGS) -static
m62_3_LDADD = $(m30_LIBS)
bin_tests += m62-3

check_PROGRAMS += m62-4
m62_4_LDFLAGS = $(AM_LDFLAGS) -static
m62_4_LDADD = $(m30_LIBS)
bin_tests += m62-4

check_PROGRAMS += m62-5
m62_5_LDFLAGS = $(AM_LDFLAGS) -static
m62_5_LDADD = $(m30_LIBS)
bin_tests += m62-5

check_PROGRAMS += m62-6
m62_6_LDFLAGS = $(AM_LDFLAGS) -static
m62_6_LDADD = $(m30_LIBS)
bin_tests += m62-6

check_PROGRAMS += m62-7
m62_7_LDFLAGS = $(AM_LDFLAGS) -static
m62_7_LDADD = $(m30_LIBS)
bin_tests += m62-7

check_PROGRAMS += m62-8
m62_8_LDFLAGS = $(AM_LDFLAGS) -static
m62_8_LDADD = $(m30_LIBS)
bin_tests += m62-8

check_PROGRAMS += m62-9
m62_9_LDFLAGS = $(AM_LDFLAGS) -static
m62_9_LDADD = $(m30_LIBS)
bin_tests += m62-9

check_PROGRAMS += m62-10
m62_10_LDFLAGS = $(AM_LDFLAGS) -static
m62_10_LDADD = $(m30_LIBS)
bin_tests += m62-10


## our friendly helpers
check_PROGRAMS += clitoris
clitoris_CPPFLAGS = $(AM_CPPFLAGS)
clitoris_CPPFLAGS += -D_GNU_SOURCE
clitoris_LDFLAGS = $(AM_LDFLAGS) -static
clitoris_LDADD = -lutil
BUILT_SOURCES += clitoris.x clitoris.xh

check_PROGRAMS += shack
BUILT_SOURCES += shack.x shack.xh

check_PROGRAMS += hxdiff
BUILT_SOURCES += hxdiff.x hxdiff.xh

## gengetopt rule with sed hack to avoid using VERSION and config.h
## depend on Makefile.am to survive a git pull
%.x %.xh: %.ggo
	$(AM_V_GEN) gengetopt -F $* -c x -H xh -i $<
	@sed -e 's,_PARSER_VERSION VERSION,_PARSER_VERSION "$(VERSION)",g' \
	     $*.xh > $*.xh-t \
	&& mv $*.xh-t $*.xh

clean-local:
	-rm -rf *.tmpd

## Makefile.am ends here
