# Help the Developers and yourself.  Just use the C locale and settings
# for the compilation. They can still be overriden by make LANG=<whatever>
# but that is general a not very good idea
LANG=C
LC_ALL=C

BUILT_SOURCES =
EXTRA_DIST = $(BUILT_SOURCES)

man1_MANS =
man1_MANS += $(built_mans)

EXTRA_DIST += author.h2m
BUILT_SOURCES += $(built_mans)

built_mans =
built_mans += ute.man
built_mans += ute-mux.man
built_mans += ute-print.man
built_mans += ute-shnot.man
built_mans += ute-fsck.man

## help2man helpers
my_bld_man = $(__bld_man_$(V))
__bld_man_ = $(__bld_man_$(AM_DEFAULT_VERBOSITY))
__bld_man_0 = @echo "  HELP2MAN" $*;
%.man: $(top_srcdir)/src/%-clo.ggo $(top_srcdir)/configure
	$(my_bld_man) $(HELP2MAN) -S "$(PACKAGE_STRING)" -p "($(PACKAGE))$*" \
		-I "$(srcdir)/author.h2m" \
		-o $@ $(top_builddir)/src/$*

ute.man: $(top_srcdir)/src/ute.c $(top_srcdir)/configure
	$(my_bld_man) $(HELP2MAN) -S "$(PACKAGE_STRING)" -p "($(PACKAGE))$*" \
		-I "$(srcdir)/author.h2m" \
		-o $@ $(top_builddir)/src/ute
