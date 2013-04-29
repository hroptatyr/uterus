# Help the Developers and yourself. Just use the C locale and settings
# for the compilation. They can still be overriden by make LANG=<whatever>
# but that is general a not very good idea
LANG = C
LC_ALL = C

AM_CPPFLAGS = -D_GNU_SOURCE -D_XOPEN_SOURCE=600 -D_POSIX_C_SOURCE=201001L
AM_CPPFLAGS += -I$(abs_top_srcdir)/src
AM_LDFLAGS = -L$(abs_top_builddir)/src

EXTRA_DIST = $(ut_tests) $(BUILT_SOURCES)
TESTS = $(ut_tests) $(bin_tests)
BUILT_SOURCES =
check_PROGRAMS =
ut_tests =
bin_tests =

m30_LIBS = $(abs_top_builddir)/src/libm30.la

uterus_LIBS =
uterus_LIBS += $(abs_top_builddir)/src/libuterus.la
uterus_LIBS += $(m30_LIBS)

TEST_EXTENSIONS = .ut
UT_LOG_COMPILER = $(builddir)/ut-test
AM_UT_LOG_FLAGS = --builddir $(top_builddir)/src

ut_tests += mux.1.ut
ut_tests += mux.2.ut
ut_tests += mux.3.ut
ut_tests += mux.4.ut
ut_tests += mux.5.ut
ut_tests += mux.6.ut
if HAVE_EXPAT
ut_tests += mux.7.ut
ut_tests += mux.8.ut
ut_tests += mux.9.ut
ut_tests += mux.10.ut
else  ## !HAVE_EXPAT
EXTRA_DIST += mux.7.ut
EXTRA_DIST += mux.8.ut
EXTRA_DIST += mux.9.ut
EXTRA_DIST += mux.10.ut
endif  ## HAVE_EXPAT
ut_tests += mux.11.ut
ut_tests += mux.12.ut
ut_tests += mux.13.ut
ut_tests += mux.14.ut
ut_tests += mux.15.ut
ut_tests += mux.16.ut
ut_tests += mux.17.ut
ut_tests += mux.18.ut
ut_tests += mux.19.ut
ut_tests += mux.20.ut
ut_tests += mux.21.ut
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
ut_tests += print.1.ut
ut_tests += print.2.ut
endif
ut_tests += print.3.ut
ut_tests += print.4.ut
ut_tests += print.5.ut
ut_tests += print.6.ut
ut_tests += print.7.ut
ut_tests += print.8.ut
EXTRA_DIST += print.a.ute
EXTRA_DIST += print.b.ute
EXTRA_DIST += print.c.ute
EXTRA_DIST += print.d.ute
EXTRA_DIST += print.e.ute
EXTRA_DIST += print.e.beute
EXTRA_DIST += print.f.ute
EXTRA_DIST += print.f.beute

ut_tests += shnot.1.ut
ut_tests += shnot.2.ut
EXTRA_DIST += shnot.1.ref.ute
EXTRA_DIST += shnot.2.ref.ute

ut_tests += chndl.1.ut
ut_tests += chndl.2.ut
EXTRA_DIST += chndl.2.ref.ute

ut_tests += fsck.1.ut
ut_tests += fsck.2.ut
ut_tests += fsck.3.ut
ut_tests += fsck.4.ut
ut_tests += fsck.5.ut
ut_tests += fsck.6.ut
ut_tests += fsck.7.ut
ut_tests += fsck.8.ut
ut_tests += fsck.9.ut
ut_tests += fsck.10.ut
ut_tests += fsck.11.ut
ut_tests += fsck.12.ut
ut_tests += fsck.13.ut
ut_tests += fsck.14.ut
ut_tests += fsck.15.ut
ut_tests += fsck.16.ut
if HAVE_LZMA
ut_tests += fsck.17.ut
ut_tests += fsck.18.ut
ut_tests += fsck.19.ut
ut_tests += fsck.20.ut
ut_tests += fsck.21.ut
ut_tests += fsck.22.ut
ut_tests += fsck.23.ut
ut_tests += fsck.24.ut
endif  HAVE_LZMA
ut_tests += fsck.25.ut
ut_tests += fsck.26.ut
ut_tests += fsck.27.ut
ut_tests += fsck.28.ut
EXTRA_DIST += fsck.a.ute
EXTRA_DIST += fsck.b.ute
EXTRA_DIST += fsck.17.ref.leute
EXTRA_DIST += fsck.17.ref.beute

ut_tests += slut.1.ut
ut_tests += slut.2.ut
ut_tests += slut.3.ut
ut_tests += slut.4.ut

ut_tests += info.1.ut
ut_tests += info.2.ut
ut_tests += info.3.ut
ut_tests += info.4.ut
ut_tests += info.5.ut
ut_tests += info.6.ut
EXTRA_DIST += info.5.ref.ute
EXTRA_DIST += info.5.ref.beute

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


## the test suite code
check_PROGRAMS += ut-test
BUILT_SOURCES += ut-test-clo.c ut-test-clo.h
EXTRA_DIST += ut-test.sh


## ggo rule
%.c %.h: %.ggo
	$(AM_V_GEN) gengetopt -F $* -i $<

clean-local:
	-rm -rf *.tmpd

## Makefile.am ends here
