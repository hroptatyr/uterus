### Makefile.am
include $(top_builddir)/version.mk

AM_CPPFLAGS = -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=600 -D_BSD_SOURCE
AM_LDFLAGS =
EXTRA_DIST =

MEX_LDFLAGS = -module -shared -avoid-version -shrext ".mex"

EXTRA_DIST += $(mex_DATA)
mex_LTLIBRARIES =
mex_DATA =

if BUILD_MATCLI
mexdir = $(MATLABPATH)/uterus
octlab_CFLAGS = -DMATLAB_MEX_FILE $(matlab_CFLAGS)
else  !BUILD_MATCLI

## backup plan is to use octave to build the mex file
if BUILD_OCTCLI
mexdir = $(OCTAVEPATH)/uterus
octlab_CFLAGS = -DOCTAVE_MEX_FILE $(octave_CFLAGS)

else  !BUILD_OCTCLI
## no idea what we're doing here
mexdir = $(pkglibexecdir)

endif  !BUILD_OCTCLI
endif  !BUILD_MATCLI

mex_LTLIBRARIES += ute_open.la
mex_DATA += ute_open.m
ute_open_la_SOURCES = ute_open.c
ute_open_la_SOURCES += ute_handle.h
ute_open_la_CPPFLAGS = $(AM_CPPFLAGS)
ute_open_la_CPPFLAGS += $(octlab_CFLAGS)
ute_open_la_CPPFLAGS += -I$(top_srcdir)/src
ute_open_la_CPPFLAGS += -I$(top_builddir)/src
ute_open_la_LDFLAGS = $(AM_LDFLAGS)
ute_open_la_LDFLAGS += $(MEX_LDFLAGS)
ute_open_la_LDFLAGS += $(XCCLDFLAGS)
if HAVE_CURL
ute_open_la_CPPFLAGS += $(curl_CFLAGS)
ute_open_la_LDFLAGS += $(curl_LIBS)
endif  HAVE_CURL
ute_open_la_LIBADD = $(top_builddir)/src/libuterus.la

mex_LTLIBRARIES += ute_close.la
mex_DATA += ute_close.m
ute_close_la_SOURCES = ute_close.c
ute_close_la_SOURCES += ute_handle.h
ute_close_la_CPPFLAGS = $(AM_CPPFLAGS)
ute_close_la_CPPFLAGS += $(octlab_CFLAGS)
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
ute_nticks_la_CPPFLAGS = $(AM_CPPFLAGS)
ute_nticks_la_CPPFLAGS += $(octlab_CFLAGS)
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
ute_slut_la_CPPFLAGS = $(AM_CPPFLAGS)
ute_slut_la_CPPFLAGS += $(octlab_CFLAGS)
ute_slut_la_CPPFLAGS += -I$(top_srcdir)/src
ute_slut_la_CPPFLAGS += -I$(top_builddir)/src
ute_slut_la_LDFLAGS = $(AM_LDFLAGS)
ute_slut_la_LDFLAGS += $(MEX_LDFLAGS)
ute_slut_la_LDFLAGS += $(XCCLDFLAGS)
ute_slut_la_LIBADD = $(top_builddir)/src/libuterus.la

mex_LTLIBRARIES += ute_tick.la
mex_DATA += ute_tick.m
ute_tick_la_SOURCES = ute_tick.c
ute_tick_la_SOURCES += ute_handle.h
ute_tick_la_CPPFLAGS = $(AM_CPPFLAGS)
ute_tick_la_CPPFLAGS += $(octlab_CFLAGS)
ute_tick_la_CPPFLAGS += -I$(top_srcdir)/src
ute_tick_la_CPPFLAGS += -I$(top_builddir)/src
ute_tick_la_LDFLAGS = $(AM_LDFLAGS)
ute_tick_la_LDFLAGS += $(MEX_LDFLAGS)
ute_tick_la_LDFLAGS += $(XCCLDFLAGS)
ute_tick_la_LIBADD = $(top_builddir)/src/libuterus.la


## matlab needs mexa64, octave needs mex
install-data-hook:
	-for i in ute_tick ute_nticks ute_slut ute_open ute_close; do \
		cd $(DESTDIR)$(mexdir) && mv -f $${i}.mex $${i}.$(MEXEXT); \
	done

uninstall-hook:
	-for i in ute_tick ute_nticks ute_slut ute_open ute_close; do \
		$(RM) -- $(DESTDIR)$(mexdir)/$${i}.$(MEXEXT); \
	done

# 
# Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<
