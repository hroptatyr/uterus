include $(top_builddir)/version.mk

# Help the Developers and yourself.  Just use the C locale and settings
# for the compilation. They can still be overriden by make LANG=<whatever>
# but that is general a not very good idea
LANG=C
LC_ALL=C

BUILT_SOURCES =
EXTRA_DIST = $(BUILT_SOURCES)
SUFFIXES =

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

## man gen helpers
SUFFIXES += .yuck
SUFFIXES += .man

VPATH += @top_srcdir@/src

.yuck.man:
	$(AM_V_GEN) PATH="$(top_builddir)/build-aux:$${PATH}" \
		yuck$(EXEEXT) genman \
			--package "$(PACKAGE_NAME)" \
			-i "$(srcdir)/author.h2m" \
			-o $@ --version-string "$(VERSION)" $<

my_bld_texi = $(__bld_texi_$(V))
__bld_texi_ = $(__bld_texi_$(AM_DEFAULT_VERBOSITY))
__bld_texi_0 = @echo "  GENTEXI " $*;
SUFFIXES += .texi
.yuck.texi:
	$(my_bld_texi) ${SHELL} \
		$(srcdir)/gentexi.sh $(top_builddir)/src/$* \
		$($*_EXAMPLES) > $@ || { rm -f -- $@; false; }

