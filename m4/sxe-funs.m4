dnl sxe-funs.m4 --- functions we intend to use
dnl
dnl This file is part of uterus

AC_DEFUN([SXE_FUNC_MKOSTEMP], [dnl
	AC_MSG_CHECKING([for mkostemp])

	## we know it's a gnu thing
	save_CPPFLAGS="${CPPFLAGS}"
	CPPFLAGS="${CPPFLAGS} -D_GNU_SOURCE"

	AC_LINK_IFELSE([AC_LANG_SOURCE([
AC_INCLUDES_DEFAULT[

int
main()
{
	char tmp[] = "XXXXXX";
	return mkostemp(tmp, 0644) >= 0 ? 0 : 1;
}
]])], [
	sxe_cv_func_mkostemp="yes"
	AC_DEFINE([HAVE_MKOSTEMP], [1], [Define if mkostemp() exists])
	], [
	sxe_cv_func_mkostemp="no"
	])

	if test "${sxe_cv_func_mkostemp}" = "yes"; then
		AC_MSG_RESULT([yes])
	else
		AC_MSG_RESULT([no])
	fi

	## reset the CPPFLAGS
	CPPFLAGS="${save_CPPFLAGS}"
])dnl SXE_FUNC_MKOSTEMP

dnl sxe-funs.m4 ends here
