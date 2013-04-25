dnl compiler.m4 --- compiler magic
dnl
dnl Copyright (C) 2005-2013 Sebastian Freundt
dnl Copyright (c) 2005 Steven G. Johnson
dnl Copyright (c) 2005 Matteo Frigo
dnl
dnl Author: Sebastian Freundt <hroptatyr@sxemacs.org>
dnl
dnl Redistribution and use in source and binary forms, with or without
dnl modification, are permitted provided that the following conditions
dnl are met:
dnl
dnl 1. Redistributions of source code must retain the above copyright
dnl    notice, this list of conditions and the following disclaimer.
dnl
dnl 2. Redistributions in binary form must reproduce the above copyright
dnl    notice, this list of conditions and the following disclaimer in the
dnl    documentation and/or other materials provided with the distribution.
dnl
dnl 3. Neither the name of the author nor the names of any contributors
dnl    may be used to endorse or promote products derived from this
dnl    software without specific prior written permission.
dnl
dnl THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
dnl IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
dnl WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
dnl DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
dnl FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
dnl CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
dnl SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
dnl BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
dnl WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
dnl OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
dnl IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
dnl
dnl This file is part of SXEmacs.

##### http://autoconf-archive.cryp.to/ax_check_compiler_flags.html
## renamed the prefix to SXE_
AC_DEFUN([SXE_CHECK_COMPILER_FLAG], [dnl
dnl SXE_CHECK_COMPILER_FLAG([flag], [action-if-accepted], [action-if-not-accepted])
	AC_MSG_CHECKING([whether _AC_LANG compiler accepts $1])

	## store werror status, then turn -Werror on
	save_ac_[]_AC_LANG_ABBREV[]_werror_flag="${ac_[]_AC_LANG_ABBREV[]_werror_flag}"
	AC_LANG_WERROR

	AC_CACHE_VAL(AS_TR_SH(sxe_cv_[]_AC_LANG_ABBREV[]_flag_$1), [dnl
		sxe_save_FLAGS="${[]_AC_LANG_PREFIX[]FLAGS}"
		_AC_LANG_PREFIX[]FLAGS="$1"
		AC_COMPILE_IFELSE([AC_LANG_PROGRAM()],
			eval AS_TR_SH(sxe_cv_[]_AC_LANG_ABBREV[]_flag_$1)="yes",
			eval AS_TR_SH(sxe_cv_[]_AC_LANG_ABBREV[]_flag_$1)="no")
		_AC_LANG_PREFIX[]FLAGS="${sxe_save_FLAGS}"
	])
	eval sxe_check_flag=$AS_TR_SH(sxe_cv_[]_AC_LANG_ABBREV[]_flag_$1)
	ac_[]_AC_LANG_ABBREV[]_werror_flag="${save_ac_[]_AC_LANG_ABBREV[]_werror_flag}"

	AC_MSG_RESULT([${sxe_check_flag}])
	if test "${sxe_check_flag}" = "yes"; then
		:
		$2
	else
		:
		$3
	fi
])dnl SXE_CHECK_COMPILER_FLAG

AC_DEFUN([SXE_CHECK_PREPROC_FLAG], [dnl
dnl SXE_CHECK_CPP_FLAG([flag], [action-if-found], [action-if-not-found])
	AC_MSG_CHECKING([whether _AC_LANG preprocessor accepts $1])

	## store werror status, then turn -Werror on
	save_ac_[]_AC_LANG_ABBREV[]_werror_flag="${ac_[]_AC_LANG_ABBREV[]_werror_flag}"
	AC_LANG_WERROR

	AC_CACHE_VAL(AS_TR_SH(sxe_cv_[]_AC_LANG_ABBREV[]pp_flag_$1), [dnl
		sxe_save_FLAGS="${[]_AC_LANG_PREFIX[]PPFLAGS}"
		_AC_LANG_PREFIX[]PPFLAGS="$1"
		AC_PREPROC_IFELSE([AC_LANG_PROGRAM()],
			eval AS_TR_SH(sxe_cv_[]_AC_LANG_ABBREV[]pp_flag_$1)="yes",
			eval AS_TR_SH(sxe_cv_[]_AC_LANG_ABBREV[]pp_flag_$1)="no")
		_AC_LANG_PREFIX[]PPFLAGS=$sxe_save_FLAGS
	])
	eval sxe_check_flag=$AS_TR_SH(sxe_cv_[]_AC_LANG_ABBREV[]pp_flag_$1)
	ac_[]_AC_LANG_ABBREV[]_werror_flag="${save_ac_[]_AC_LANG_ABBREV[]_werror_flag}"

	AC_MSG_RESULT([${sxe_check_flag}])
	if test "${sxe_check_flag}" = "yes"; then
		:
		$2
	else
		:
		$3
	fi
])dnl SXE_CHECK_PREPROC_FLAG


AC_DEFUN([SXE_DEBUGFLAGS], [dnl
	## distinguish between different compilers, no?
	SXE_CHECK_COMPILER_FLAG([-g])
	SXE_CHECK_COMPILER_FLAG([-g3])

	AC_PATH_PROG([DBX], [dbx])
	if test -n "$ac_cv_path_DBX"; then
		SXE_CHECK_COMPILER_FLAG([-gstabs])
		SXE_CHECK_COMPILER_FLAG([-gstabs3])
		SXE_CHECK_COMPILER_FLAG([-gxcoff])
		SXE_CHECK_COMPILER_FLAG([-gxcoff3])
	fi

	AC_PATH_PROG([GDB], [gdb])
	if test -n "$ac_cv_path_GDB"; then
		SXE_CHECK_COMPILER_FLAG([-ggdb])
		SXE_CHECK_COMPILER_FLAG([-ggdb3])
	fi

	AC_PATH_PROG([SDB], [sdb])
	if test -n "$ac_cv_path_SDB"; then
		SXE_CHECK_COMPILER_FLAG([-gcoff])
		SXE_CHECK_COMPILER_FLAG([-gcoff3])
	fi

	## final evaluation
	debugflags=""
	## gdb
	if test "$sxe_cv_c_flag__ggdb3" = "yes"; then
		debugflags="$debugflags -ggdb3"
	elif test "$sxe_cv_c_flag__ggdb" = "yes"; then
		debugflags="$debugflags -ggdb"
	fi
	## stabs
	if test "$sxe_cv_c_flag__gstabs3" = "yes"; then
		debugflags="$debugflags -gstabs3"
	elif test "$sxe_cv_c_flag__gstabs" = "yes"; then
		debugflags="$debugflags -gstabs"
	fi
	## coff
	if test "$sxe_cv_c_flag__gcoff3" = "yes"; then
		debugflags="$debugflags -gcoff3"
	elif test "$sxe_cv_c_flag__gcoff" = "yes"; then
		debugflags="$debugflags -gcoff"
	fi
	## xcoff
	if test "$sxe_cv_c_flag__gxcoff3" = "yes"; then
		debugflags="$debugflags -gxcoff3"
	elif test "$sxe_cv_c_flag__gxcoff" = "yes"; then
		debugflags="$debugflags -gxcoff"
	fi

	if test -z "debugflags" -a \
		"$sxe_cv_c_flag__g" = "yes"; then
		debugflags="$debugflags -g"
	fi

	SXE_CHECK_COMPILER_FLAG([-ftime-report])
	SXE_CHECK_COMPILER_FLAG([-fmem-report])
	SXE_CHECK_COMPILER_FLAG([-fvar-tracking])
	SXE_CHECK_COMPILER_FLAG([-save-temps])

	#if test "$sxe_cv_c_flag__ggdb3" = "yes" -a \
	#	"$sxe_cv_c_flag__fvar_tracking" = "yes"; then
	#	debugflags="$debugflags -fvar-tracking"
	#fi

	AC_MSG_CHECKING([for preferred debugging flags])
	AC_MSG_RESULT([${debugflags}])
])dnl SXE_DEBUGFLAGS

AC_DEFUN([SXE_WARNFLAGS], [dnl
	## Calculate warning flags.  We separate the flags for warnings from
	## the other flags because we want to force the warnings to be seen
	## by everyone who doesn't specifically override them.

	## by default we want the -Wall level
	SXE_CHECK_COMPILER_FLAG([-Wall], [warnflags="-Wall"])

	SXE_CHECK_COMPILER_FLAG([-qinfo], [
		warnflags="${warnflags} -qinfo"])

	SXE_CHECK_COMPILER_FLAG([-Wextra], [
		warnflags="${warnflags} -Wextra"])

	## Yuck, bad compares have been worth at
	## least 3 crashes!
	## Warnings about char subscripts are pretty
	## pointless, though,
	## and we use them in various places.
	SXE_CHECK_COMPILER_FLAG([-Wsign-compare], [
		warnflags="$warnflags -Wsign-compare"])
	SXE_CHECK_COMPILER_FLAG([-Wno-char-subscripts], [
		warnflags="$warnflags -Wno-char-subscripts"])
	SXE_CHECK_COMPILER_FLAG([-Wundef], [
		warnflags="$warnflags -Wundef"])

	## too much at the moment, we rarely define protos
	#warnflags="$warnflags -Wmissing-prototypes -Wstrict-prototypes"
	SXE_CHECK_COMPILER_FLAG([-Wpacked], [
		warnflags="$warnflags -Wpacked"])

	## glibc is intentionally not `-Wpointer-arith'-clean.
	## Ulrich Drepper has rejected patches to fix
	## the glibc header files.
	## we dont care
	SXE_CHECK_COMPILER_FLAG([-Wpointer-arith], [
		warnflags="$warnflags -Wpointer-arith"])

	SXE_CHECK_COMPILER_FLAG([-Wshadow], [
		warnflags="$warnflags -Wshadow"])

	## our code lacks declarations almost all the time
	SXE_CHECK_COMPILER_FLAG([-Wmissing-declarations], [
		warnflags="$warnflags -Wmissing-declarations"])
	SXE_CHECK_COMPILER_FLAG([-Wmissing-prototypes], [
		warnflags="$warnflags -Wmissing-prototypes"])

	## gcc can't practically inline anything, so exclude this
	case "${CC}" in
	dnl (
	*"gcc"*)
		;;
	dnl (
	*)
		SXE_CHECK_COMPILER_FLAG([-Winline], [
			warnflags="$warnflags -Winline"])
		;;
	esac

	SXE_CHECK_COMPILER_FLAG([-Wbad-function-cast], [
		warnflags="$warnflags -Wbad-function-cast"])
	SXE_CHECK_COMPILER_FLAG([-Wcast-qual], [
		warnflags="$warnflags -Wcast-qual"])
	SXE_CHECK_COMPILER_FLAG([-Wcast-align], [
		warnflags="$warnflags -Wcast-align"])

	## warn about incomplete switches
	## for gcc, see http://gcc.gnu.org/bugzilla/show_bug.cgi?id=50422
	## we used to have -Wswitch-default and -Wswitch-enum but that
	## set gcc off quite badly in the nested switch case
	SXE_CHECK_COMPILER_FLAG([-Wswitch], [
		warnflags="$warnflags -Wswitch"])

	SXE_CHECK_COMPILER_FLAG([-Wunused-function], [
		warnflags="$warnflags -Wunused-function"])
	SXE_CHECK_COMPILER_FLAG([-Wunused-variable], [
		warnflags="$warnflags -Wunused-variable"])
	SXE_CHECK_COMPILER_FLAG([-Wunused-parameter], [
		warnflags="$warnflags -Wunused-parameter"])
	SXE_CHECK_COMPILER_FLAG([-Wunused-value], [
		warnflags="$warnflags -Wunused-value"])
	SXE_CHECK_COMPILER_FLAG([-Wunused], [
		warnflags="$warnflags -Wunused"])
	SXE_CHECK_COMPILER_FLAG([-Wmaybe-uninitialized], [
		warnflags="${warnflags} -Wmaybe-uninitialized"])

	SXE_CHECK_COMPILER_FLAG([-Wnopragma], [
		warnflags="$warnflags -Wnopragma"])

	SXE_CHECK_COMPILER_FLAG([-fdiagnostics-show-option], [
		warnflags="${warnflags} -fdiagnostics-show-option"])

	SXE_CHECK_COMPILER_FLAG([-Wunknown-pragmas], [
		warnflags="$warnflags -Wunknown-pragmas"])
	SXE_CHECK_COMPILER_FLAG([-Wuninitialized], [
		warnflags="$warnflags -Wuninitialized"])
	SXE_CHECK_COMPILER_FLAG([-Wreorder], [
		warnflags="$warnflags -Wreorder"])
	SXE_CHECK_COMPILER_FLAG([-Wdeprecated], [
		warnflags="$warnflags -Wdeprecated"])

	SXE_CHECK_COMPILER_FLAG([-Wparentheses], [
		warnflags="${warnflags} -Wparentheses"])

	## icc specific
	SXE_CHECK_COMPILER_FLAG([-Wcheck], [
		warnflags="$warnflags -Wcheck"])

	dnl SXE_CHECK_COMPILER_FLAG([-Wp64], [
	dnl 	warnflags="$warnflags -Wp64"])

	SXE_CHECK_COMPILER_FLAG([-Wstrict-aliasing], [
		warnflags="$warnflags -Wstrict-aliasing"])

	SXE_CHECK_COMPILER_FLAG([-w3], [
		warnflags="$warnflags -w3"])

	SXE_CHECK_COMPILER_FLAG([-diag-disable 10237], [dnl
		warnflags="${warnflags} -diag-disable 10237"], [
		SXE_CHECK_COMPILER_FLAG([-wd 10237], [dnl
			warnflags="${warnflags} -wd 10237"])])

	SXE_CHECK_COMPILER_FLAG([-debug inline-debug-info], [
		warnflags="${warnflags} -debug inline-debug-info"])

	SXE_CHECK_COMPILER_FLAG([-diag-enable remark,vec,par], [
		warnflags="${warnflags} -diag-enable remark,vec,par"])

	AC_MSG_CHECKING([for preferred warning flags])
	AC_MSG_RESULT([${warnflags}])
])dnl SXE_WARNFLAGS

AC_DEFUN([SXE_OPTIFLAGS], [dnl
	optiflags="-O3"

	SXE_CHECK_COMPILER_FLAG([-ipo256], [
		special_optiflags="${special_optiflags} -ipo256"])

	SXE_CHECK_COMPILER_FLAG([-ipo-jobs256], [
		special_optiflags="${special_optiflags} -ipo-jobs256"])
])dnl SXE_OPTIFLAGS

AC_DEFUN([SXE_FEATFLAGS], [dnl
	## default flags for needed features
	AC_REQUIRE([SXE_CHECK_COMPILER_XFLAG])
	XCCFLAG="${XFLAG}"

	## recent gentoos went ballistic again, they compile PIE gcc's
	## but there's no way to turn that misconduct off ...
	## however I've got one report about a working PIE build
	## we'll just check for -nopie here, if it works, we turn it on
	## (and hence PIE off) and hope bug 16 remains fixed
	SXE_CHECK_COMPILER_FLAG([-nopie],
		[featflags="$featflags -nopie"])

	## it's utterly helpful to get the sse2 unit up
	SXE_CHECK_COMPILER_FLAG([-msse2], [dnl
		## sse2 is the cure
		featflags="$featflags -msse2"], [dnl
		## oh bugger
		AC_DEFINE([FPMATH_NO_SSE], [1], [no sse2 support for floats])])

	## icc and gcc related
	## check if some stuff can be staticalised
	## actually requires SXE_WARNFLAGS so warnings would be disabled
	## that affect the outcome of the following tests
	SXE_CHECK_COMPILER_FLAG([-static-intel], [
		featflags="${featflags} -static-intel"
		XCCLDFLAGS="${XCCLDFLAGS} \${XCCFLAG} -static-intel"], [:],
		[${SXE_CFLAGS}])
	SXE_CHECK_COMPILER_FLAG([-static-libgcc], [
		featflags="${featflags} -static-libgcc"
		XCCLDFLAGS="${XCCLDFLAGS} \${XCCFLAG} -static-libgcc"], [:],
		[${SXE_CFLAGS}])

	AC_SUBST([XCCLDFLAGS])
	AC_SUBST([XCCFLAG])
])dnl SXE_FEATFLAGS

AC_DEFUN([SXE_CHECK_COMPILER_XFLAG], [dnl
	if test "${XFLAG}" = ""; then
		SXE_CHECK_COMPILER_FLAG([-XCClinker -foo], [XFLAG="-XCClinker"])
	fi
	if test "${XFLAG}" = ""; then
		SXE_CHECK_COMPILER_FLAG([-Xlinker -foo], [XFLAG="-Xlinker"])
	fi

	AC_SUBST([XFLAG])
])dnl SXE_CHECK_COMPILER_XFLAG


AC_DEFUN([SXE_CHECK_CFLAGS], [dnl
	dnl #### This may need to be overhauled so that all of SXEMACS_CC's flags
	dnl are handled separately, not just the xe_cflags_warning stuff.

	## Use either command line flag, environment var, or autodetection
	CFLAGS=""
	SXE_DEBUGFLAGS
	SXE_WARNFLAGS
	SXE_OPTIFLAGS
	SXE_CFLAGS="$SXE_CFLAGS $debugflags $optiflags $warnflags"

	SXE_FEATFLAGS
	SXE_CFLAGS="$SXE_CFLAGS $featflags"

	save_ac_c_werror_flag="${ac_c_werror_flag}"

	CFLAGS="${SXE_CFLAGS} ${ac_cv_env_CFLAGS_value}"
	AC_MSG_CHECKING([for preferred CFLAGS])
	AC_MSG_RESULT([${CFLAGS}])

	EXTRA_CFLAGS="${special_optiflags}"
	AC_SUBST([EXTRA_CFLAGS])
	AC_MSG_CHECKING([for EXTRA_CFLAGS])
	AC_MSG_RESULT([${EXTRA_CFLAGS}])

	AC_MSG_NOTICE([
If you wish to ADD your own flags you want to stop here and rerun the
configure script like so:
  configure CFLAGS=<to-be-added-flags>

You can always override the determined CFLAGS, partially or totally,
using
  make -C <directory> CFLAGS=<your-own-flags> [target]
or
  make CFLAGS=<your-own-flags> [target]
respectively
		])

	ac_c_werror_flag="${save_ac_c_werror_flag}"
])dnl SXE_CHECK_CFLAGS

AC_DEFUN([SXE_CHECK_CC], [dnl
dnl SXE_CHECK_CC([STANDARDS])
dnl standards are flavours supported by the compiler chosen with AC_PROG_CC
	pushdef([stds], m4_default([$1], [gnu11 c11 gnu99 c99]))

	AC_REQUIRE([AC_CANONICAL_HOST])
	AC_REQUIRE([AC_CANONICAL_BUILD])
	AC_REQUIRE([AC_PROG_CPP])
	AC_REQUIRE([AC_PROG_CC])

	AC_HEADER_STDC

	case "${CC}" in dnl (
	*"-std="*)
		## user specified a std value already
		;;
		dnl (
	*)
		for i in []stds[]; do
			SXE_CHECK_COMPILER_FLAG([-std="${i}"], [
				std="-std=${i}"
				save_CC="${CC}"
				CC="${CC} ${std}"
				SXE_CHECK_ANON_STRUCTS_DECL
				CC="${save_CC}"
				if test "${sxe_cv_have_anon_structs_decl}" \
					= "yes"; then
					break
				fi
			])
		done

		AC_MSG_CHECKING([for preferred CC std])
		AC_MSG_RESULT([${std}])
		CC="${CC} ${std}"

		## while we're at it, check for anon initialising too
		SXE_CHECK_ANON_STRUCTS_INIT
		## oh and sloppy sloppy init
		SXE_CHECK_SLOPPY_STRUCTS_INIT
		;;
	esac

	popdef([stds])
])dnl SXE_CHECK_CC

AC_DEFUN([SXE_CHECK_ANON_STRUCTS_INIT], [
	AC_MSG_CHECKING([dnl
whether C compiler can initialise anonymous structs and unions])
	AC_LANG_PUSH([C])

	## backup our CFLAGS and unset it
	save_CFLAGS="${CFLAGS}"
	CFLAGS=""

	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
union __test_u {
	int i;
	struct {
		char c;
		char padc;
		short int pads;
	};
};
	]], [[
	union __test_u tmp = {.c = '4'};
	]])], [
		sxe_cv_have_anon_structs_init="yes"
	], [
		sxe_cv_have_anon_structs_init="no"
	])
	AC_MSG_RESULT([${sxe_cv_have_anon_structs_init}])

	## restore CFLAGS
	CFLAGS="${save_CFLAGS}"

	if test "${sxe_cv_have_anon_structs_init}" = "yes"; then
		AC_DEFINE([HAVE_ANON_STRUCTS_INIT], [1], [dnl
Whether c11 anon struct initialising works])
		$1
		:
	else
		$2
		:
	fi
	AC_LANG_POP()
])dnl SXE_CHECK_ANON_STRUCTS_INIT

AC_DEFUN([SXE_CHECK_ANON_STRUCTS_DECL], [
	AC_MSG_CHECKING([dnl
whether C compiler can understand anonymous structs and unions])
	AC_LANG_PUSH([C])

	## backup our CFLAGS and unset it
	save_CFLAGS="${CFLAGS}"
	CFLAGS=""

	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
union __test_u {
	int i;
	struct {
		char c;
		char padc;
		short int pads;
	};
};
	]], [[
	/* nothing to do really*/
	union __test_u foo;
	foo.c = 0;
	]])], [
		sxe_cv_have_anon_structs_decl="yes"
	], [
		sxe_cv_have_anon_structs_decl="no"
	])
	AC_MSG_RESULT([${sxe_cv_have_anon_structs_decl}])

	## restore CFLAGS
	CFLAGS="${save_CFLAGS}"

	if test "${sxe_cv_have_anon_structs_decl}" = "yes"; then
		AC_DEFINE([HAVE_ANON_STRUCTS_DECL], [1], [dnl
Whether c11 anon structs declaring works])
		$1
		:
	else
		$2
		:
	fi
	AC_LANG_POP()
])dnl SXE_CHECK_ANON_STRUCTS_DECL

AC_DEFUN([SXE_CHECK_SLOPPY_STRUCTS_INIT], [
	AC_LANG_PUSH([C])

	## backup our CFLAGS and unset it
	save_CFLAGS="${CFLAGS}"
	CFLAGS="-Werror"

	SXE_CHECK_COMPILER_FLAG([-Wmissing-field-initializers], [
		CFLAGS="${CFLAGS} -Wmissing-field-initializers"])

	AC_MSG_CHECKING([dnl
whether C compiler can initialise structs and unions in a sloppy way])

	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
struct __test_s {
	int i;
	int j;
};
	]], [[
	struct __test_s tmp = {};
	]])], [
		sxe_cv_have_sloppy_structs_init="yes"
	], [
		sxe_cv_have_sloppy_structs_init="no"
	])
	AC_MSG_RESULT([${sxe_cv_have_sloppy_structs_init}])

	## restore CFLAGS
	CFLAGS="${save_CFLAGS}"

	if test "${sxe_cv_have_sloppy_structs_init}" = "yes"; then
		AC_DEFINE([HAVE_SLOPPY_STRUCTS_INIT], [1], [dnl
Whether sloppy struct initialising works])
		$1
		:
	else
		$2
		:
	fi
	AC_LANG_POP()
])dnl SXE_CHECK_SLOPPY_STRUCTS_INIT

dnl sxe-compiler.m4 ends here
