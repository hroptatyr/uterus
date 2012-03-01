# Help the Developers and yourself. Just use the C locale and settings
# for the compilation. They can still be overriden by make LANG=<whatever>
# but that is general a not very good idea
LANG = C
LC_ALL = C

EXTRA_DIST = $(TESTS)
EXTRA_DIST += $(UT_LOG_COMPILER)
TESTS =

TEST_EXTENSIONS = .ut
UT_LOG_COMPILER = $(srcdir)/ut-test.sh
AM_UT_LOG_FLAGS = --builddir $(top_builddir)/src --hash sha1sum
LOG_COMPILER = echo

TESTS += mux.1.ut
TESTS += mux.2.ut
TESTS += mux.3.ut
TESTS += mux.4.ut
TESTS += mux.5.ut
EXTRA_DIST += mux.a.dukasq
EXTRA_DIST += mux.b.ariva
EXTRA_DIST += mux.c.dukas.bi5
EXTRA_DIST += mux.d.ariva

TESTS += print.1.ut
TESTS += print.2.ut
EXTRA_DIST += print.a.ute
EXTRA_DIST += print.b.ute
EXTRA_DIST += print.c.ute
EXTRA_DIST += print.d.ute

TESTS += shnot.1.ut
TESTS += shnot.2.ut

TESTS += fsck.1.ut
TESTS += fsck.2.ut
TESTS += fsck.3.ut
EXTRA_DIST += fsck.a.ute

## Makefile.am ends here
