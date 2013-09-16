include $(top_builddir)/version.mk

# Help the Developers and yourself.  Just use the C locale and settings
# for the compilation. They can still be overriden by make LANG=<whatever>
# but that is general a not very good idea
LANG=C
LC_ALL=C

BUILT_SOURCES =
EXTRA_DIST = $(BUILT_SOURCES)

info_TEXINFOS = uterus.texi
uterus_TEXINFOS =
uterus_TEXINFOS += $(built_texis)
uterus_TEXINFOS += uterus.texi
uterus_TEXINFOS += uteapi.texi

man1_MANS =
man1_MANS += $(built_mans)

EXTRA_DIST += author.h2m
BUILT_SOURCES += $(built_mans) $(built_infos)

built_mans =
built_mans += ute.man
built_mans += ute-chndl.man
built_mans += ute-fsck.man
built_mans += ute-info.man
built_mans += ute-mux.man
built_mans += ute-print.man
built_mans += ute-shnot.man
built_mans += ute-slab.man
built_mans += ute-slut.man

built_infos =
built_infos += ute-chndl.texi
built_infos += ute-fsck.texi
built_infos += ute-info.texi
built_infos += ute-mux.texi
built_infos += ute-print.texi
built_infos += ute-shnot.texi
built_infos += ute-slab.texi
built_infos += ute-slut.texi

dist_noinst_SCRIPTS = gentexi.sh

## help2man helpers
my_bld_man = $(__bld_man_$(V))
__bld_man_ = $(__bld_man_$(AM_DEFAULT_VERBOSITY))
__bld_man_0 = @echo "  HELP2MAN" $*;
%.man: $(top_srcdir)/src/%.ggo $(top_srcdir)/configure $(top_builddir)/version.mk
	$(my_bld_man) $(HELP2MAN) -S "$(PACKAGE_STRING)" -p "($(PACKAGE))$*" \
		-I "$(srcdir)/author.h2m" \
		-o $@ $(top_builddir)/src/$*

ute.man: $(top_srcdir)/src/ute.c $(top_srcdir)/configure $(top_builddir)/version.mk
	$(my_bld_man) $(HELP2MAN) -S "$(PACKAGE_STRING)" -p "($(PACKAGE))$*" \
		-I "$(srcdir)/author.h2m" \
		-o $@ $(top_builddir)/src/ute

my_bld_texi = $(__bld_texi_$(V))
__bld_texi_ = $(__bld_texi_$(AM_DEFAULT_VERBOSITY))
__bld_texi_0 = @echo "  GENTEXI " $*;
%.texi: $(top_srcdir)/src/%.ggo $(top_builddir)/version.mk
	$(my_bld_texi) ${SHELL} \
		$(srcdir)/gentexi.sh $(top_builddir)/src/$* \
		$($*_EXAMPLES) > $@ || rm -f -- $@

