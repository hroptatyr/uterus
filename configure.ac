AC_PREREQ([2.64])
AC_INIT([uterus], [0.1], [http://www.ga-group.nl/])
AC_CONFIG_AUX_DIR([.])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADER([src/config.h])
AC_CONFIG_SRCDIR([src/])

AM_INIT_AUTOMAKE([foreign])

## compiler check
AC_PROG_CC
AC_PROG_CC_C99
## libtool goddess^Wgoodness
SXE_CHECK_LIBTOOL

## output
AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([src/Makefile])
AC_OUTPUT

