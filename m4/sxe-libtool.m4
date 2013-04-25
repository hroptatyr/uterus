dnl sxe-libtool.m4 -- just a quick libtoolish macros
dnl
dnl Copyright (C) 2007, 2008 Sebastian Freundt.
dnl
dnl This file is part of SXEmacs

AC_DEFUN([SXE_CHECK_LIBTOOL], [dnl
	AC_MSG_RESULT([starting libtool investigation...])

	## turn off -Werror'ing
	ac_[]_AC_LANG_ABBREV[]_werror_flag=

	LT_PREREQ([2.1])
	LT_INIT([dlopen])

	LT_LIB_DLLOAD
	LT_LIB_M
	LT_SYS_DLOPEN_DEPLIBS
	LT_SYS_SYMBOL_USCORE
	LT_FUNC_DLSYM_USCORE

	## cope with libtool's convenience lib/bin concept
	if test -n "$lt_cv_objdir"; then
		## this variable is a #define, too
		LT_OBJDIR="$lt_cv_objdir"
	else
		## hm, probably not the best idea but let's try
		LT_OBJDIR="."
	fi
	## definitely subst that though
	AC_SUBST([LT_OBJDIR])

	## currently there's no official variable for that, but `lt-'
	## seems to be a consistent choice throughout all libtools
	LT_CONVENIENCE_PREFIX="lt-"
	AC_SUBST([LT_CONVENIENCE_PREFIX])

	## reset XCCFLAG, we KNOW it's XCClinker in libtool
	XCCFLAG="-XCClinker"
])dnl SXE_CHECK_LIBTOOL

AC_DEFUN([SXE_CHECK_LIBLTDL], [dnl
	## make sure the libtool stuff has been run before
	AC_REQUIRE([SXE_CHECK_LIBTOOL])

	## This can't be here because older versions of libtool
	## seem to overlook the argument to LTDL_INIT
	## put the next two lines into c.ac instead and then call this macro
	dnl LT_CONFIG_LTDL_DIR([libltdl])
	dnl LTDL_INIT([recursive])

	# workaround libtool LT_CONFIG_H bug #12262
	AC_CONFIG_COMMANDS_PRE([LT_CONFIG_H=`expr "$LT_CONFIG_H" : '.*/\(.*\)'`])
	AM_CONDITIONAL([DESCEND_LIBLTDL], [test "${with_included_ltdl}" = "yes"])
])dnl SXE_CHECK_LIBLTDL

dnl sxe-libtool.m4 ends here
