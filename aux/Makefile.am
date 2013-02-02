### Makefile.am

AM_CPPFLAGS = -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=600 -D_BSD_SOURCE
AM_LDFLAGS =
EXTRA_DIST =

MEX_LDFLAGS = -module -shared -avoid-version -shrext ".mexa64"

if BUILD_MATCLI
mexdir = $(HOME)/.matlab/$(shell basename "$(MATLABROOT)")/toolbox/uterus
mex_LTLIBRARIES =
mex_DATA =

mex_LTLIBRARIES += ute_open.la
mex_DATA += ute_open.m
ute_open_la_SOURCES = ute_open.c
ute_open_la_SOURCES += ute_handle.h
ute_open_la_CPPFLAGS = $(AM_CPPFLAGS) -DMATLAB_MEX_FILE
ute_open_la_CPPFLAGS += $(matlab_CFLAGS)
ute_open_la_CPPFLAGS += -I$(top_srcdir)/src
ute_open_la_CPPFLAGS += -I$(top_builddir)/src
ute_open_la_LDFLAGS = $(AM_LDFLAGS)
ute_open_la_LDFLAGS += $(MEX_LDFLAGS)
ute_open_la_LDFLAGS += $(XCCLDFLAGS)
ute_open_la_LIBADD = $(top_builddir)/src/libuterus.la

mex_LTLIBRARIES += ute_close.la
mex_DATA += ute_close.m
ute_close_la_SOURCES = ute_close.c
ute_close_la_SOURCES += ute_handle.h
ute_close_la_CPPFLAGS = $(AM_CPPFLAGS) -DMATLAB_MEX_FILE
ute_close_la_CPPFLAGS += $(matlab_CFLAGS)
ute_close_la_CPPFLAGS += -I$(top_srcdir)/src
ute_close_la_CPPFLAGS += -I$(top_builddir)/src
ute_close_la_LDFLAGS = $(AM_LDFLAGS)
ute_close_la_LDFLAGS += $(MEX_LDFLAGS)
ute_close_la_LDFLAGS += $(XCCLDFLAGS)
ute_close_la_LIBADD = $(top_builddir)/src/libuterus.la

mex_LTLIBRARIES += ute_nticks.la
mex_DATA += ute_nticks.m
ute_nticks_la_SOURCES = ute_nticks.c
ute_nticks_la_SOURCES += ute_handle.h
ute_nticks_la_CPPFLAGS = $(AM_CPPFLAGS) -DMATLAB_MEX_FILE
ute_nticks_la_CPPFLAGS += $(matlab_CFLAGS)
ute_nticks_la_CPPFLAGS += -I$(top_srcdir)/src
ute_nticks_la_CPPFLAGS += -I$(top_builddir)/src
ute_nticks_la_LDFLAGS = $(AM_LDFLAGS)
ute_nticks_la_LDFLAGS += $(MEX_LDFLAGS)
ute_nticks_la_LDFLAGS += $(XCCLDFLAGS)
ute_nticks_la_LIBADD = $(top_builddir)/src/libuterus.la

mex_LTLIBRARIES += ute_slut.la
mex_DATA += ute_slut.m
ute_slut_la_SOURCES = ute_slut.c
ute_slut_la_SOURCES += ute_handle.h
ute_slut_la_CPPFLAGS = $(AM_CPPFLAGS) -DMATLAB_MEX_FILE
ute_slut_la_CPPFLAGS += $(matlab_CFLAGS)
ute_slut_la_CPPFLAGS += -I$(top_srcdir)/src
ute_slut_la_CPPFLAGS += -I$(top_builddir)/src
ute_slut_la_LDFLAGS = $(AM_LDFLAGS)
ute_slut_la_LDFLAGS += $(MEX_LDFLAGS)
ute_slut_la_LDFLAGS += $(XCCLDFLAGS)
ute_slut_la_LIBADD = $(top_builddir)/src/libuterus.la
endif  BUILD_MATCLI
EXTRA_DIST += $(mex_DATA)


# 
# Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<
