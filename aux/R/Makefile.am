### Makefile.am
include $(top_builddir)/version.mk

AM_CPPFLAGS = -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=600 -D_BSD_SOURCE
AM_LDFLAGS =
EXTRA_DIST =

R_LDFLAGS = -module -shared -avoid-version

Rdir = $(HOME)/.R
R_LTLIBRARIES =
R_DATA =

R_LTLIBRARIES += uteRus.la
uteRus_la_SOURCES = uteRus.c
uteRus_la_CPPFLAGS = $(AM_CPPFLAGS)
uteRus_la_CPPFLAGS += $(R_CFLAGS)
uteRus_la_CPPFLAGS += -I$(top_srcdir)/src
uteRus_la_CPPFLAGS += -I$(top_builddir)/src
uteRus_la_LDFLAGS = $(AM_LDFLAGS)
uteRus_la_LDFLAGS += $(R_LDFLAGS)
uteRus_la_LDFLAGS += $(XCCLDFLAGS)
uteRus_la_LIBADD = $(top_builddir)/src/libuterus.la

#
# Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<
