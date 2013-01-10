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

TEST_EXTENSIONS = .ut
UT_LOG_COMPILER = $(builddir)/ut-test
AM_UT_LOG_FLAGS = --builddir $(top_builddir)/src --hash sha1sum

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

## test the core api
LIBUTERUS = $(abs_top_builddir)/src/libuterus.la
check_PROGRAMS += core-file-1
check_PROGRAMS += core-file-2
check_PROGRAMS += core-file-3
check_PROGRAMS += core-file-4

core_file_1_LDFLAGS = $(AM_LDFLAGS) -static
core_file_1_LDADD = $(LIBUTERUS)
core_file_2_LDFLAGS = $(AM_LDFLAGS) -static
core_file_2_LDADD = $(LIBUTERUS)
core_file_3_LDFLAGS = $(AM_LDFLAGS) -static
core_file_3_LDADD = $(LIBUTERUS)
core_file_4_LDFLAGS = $(AM_LDFLAGS) -static
core_file_4_LDADD = $(LIBUTERUS)
bin_tests += core-file-1
bin_tests += core-file-2
bin_tests += core-file-3
bin_tests += core-file-4

check_PROGRAMS += ut-test
BUILT_SOURCES += ut-test-clo.c ut-test-clo.h
EXTRA_DIST += ut-test.sh

## ggo rule
%.c %.h: %.ggo
	$(AM_V_GEN) gengetopt -F $* -i $<

.NOTPARALLEL:

## Makefile.am ends here
