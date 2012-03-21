# Help the Developers and yourself. Just use the C locale and settings
# for the compilation. They can still be overriden by make LANG=<whatever>
# but that is general a not very good idea
LANG = C
LC_ALL = C

AM_CPPFLAGS = -D_GNU_SOURCE -D_POSIX_C_SOURCE=201001L -I$(abs_top_srcdir)/src
AM_LDFLAGS = -L$(abs_top_builddir)/src

EXTRA_DIST = $(ut_tests) $(UT_LOG_COMPILER)
TESTS = $(ut_tests) $(bin_tests)
check_PROGRAMS =
ut_tests =
bin_tests =

TEST_EXTENSIONS = .ut
UT_LOG_COMPILER = $(srcdir)/ut-test.sh
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
endif
ut_tests += mux.11.ut
ut_tests += mux.12.ut
EXTRA_DIST += mux.a.dukasq
EXTRA_DIST += mux.b.ariva
EXTRA_DIST += mux.c.dukas.bi5
EXTRA_DIST += mux.d.ariva
EXTRA_DIST += mux.e.ariva
EXTRA_DIST += mux.f.ibhist.xml
EXTRA_DIST += mux.g.ibhist.xml

EXTRA_DIST += mux.4.ref.ute
EXTRA_DIST += mux.5.ref.ute
EXTRA_DIST += mux.6.ref.ute
EXTRA_DIST += mux.7.ref.ute
EXTRA_DIST += mux.8.ref.ute
EXTRA_DIST += mux.9.ref.ute
EXTRA_DIST += mux.10.ref.ute
EXTRA_DIST += mux.11.ref.ute
EXTRA_DIST += mux.12.ref.ute

ut_tests += print.1.ut
ut_tests += print.2.ut
EXTRA_DIST += print.a.ute
EXTRA_DIST += print.b.ute
EXTRA_DIST += print.c.ute
EXTRA_DIST += print.d.ute

ut_tests += shnot.1.ut
ut_tests += shnot.2.ut

EXTRA_DIST += shnot.1.ref.ute
EXTRA_DIST += shnot.2.ref.ute

ut_tests += fsck.1.ut
ut_tests += fsck.2.ut
ut_tests += fsck.3.ut
ut_tests += fsck.4.ut
ut_tests += fsck.5.ut
ut_tests += fsck.6.ut
ut_tests += fsck.7.ut
ut_tests += fsck.8.ut
EXTRA_DIST += fsck.a.ute
EXTRA_DIST += fsck.b.ute

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


.NOTPARALLEL:

## Makefile.am ends here
