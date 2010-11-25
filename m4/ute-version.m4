dnl ute-version.m4 -- estimate version from git infos
dnl
dnl Copyright (C) 2010  Sebastian Freundt.
dnl
dnl This file is part of uterus

dnl UTE_CHECK_VERSION( VARNAME, VERSIONFILE, ACTION-IF-FOUND, ACTION-IF-NOT )
AC_DEFUN([UTE_CHECK_VERSION], [
	pushdef([VARNAME], [$1])
	pushdef([VERFILE], [$2])
	pushdef([VERSUCC], [$3])
	pushdef([VERFAIL], [$4])

	AC_ARG_VAR([GIT], [the git command])
	AC_CHECK_PROG([GIT], [git], [git], [/bin/false])

	AC_MSG_CHECKING([for ]${PACKAGE_NAME}[ version])
	if test -n "[]VERFILE[]" -a -e "${ac_top_builddir}[]VERFILE[]"; then
		VARNAME=$(cat -- "${ac_top_builddir}[]VERFILE[]")
		VERSUCC

	elif "${GIT}" symbolic-ref HEAD >/dev/null 2>&1; then
		ute_cv_d=$("${GIT}" describe --match "v[[]0-9[]]*" HEAD 2>/dev/null)
		case "${ute_cv_d}" in
		"v"[[]0-9[]]*)
			"${GIT}" update-index -q --refresh
			ute_cv_t=$("${GIT}" diff-index --name-only HEAD --)
			if test -z "${ute_cv_t}"; then
				ute_cv_d="${ute_cv_d}-dirty"
			fi
			;;
		*)
			## no tags yet
			ute_cv_d=$("${GIT}" show-ref --head --hash=6 HEAD)
		esac
		VARNAME="${ute_cv_d}"
		VERSUCC

	else
		VARNAME="unknown"
		VERFAIL
	fi
	AC_MSG_RESULT([${]VARNAME[}])
	AC_SUBST(VARNAME)
	AC_DEFINE_UNQUOTED(VARNAME, ["${]VARNAME[}"], [grrrr])

	popdef([VERFAIL])
	popdef([VERSUCC])
	popdef([VERFILE])
	popdef([VARNAME])
])dnl UTE_CHECK_VERSION

dnl ute-version.m4 ends here
