#!/bin/sh

usage()
{
	cat <<EOF 
$(basename ${0}) [OPTION] TEST_FILE

--builddir DIR  specify where tools can be found
--srcdir DIR    specify where the source tree resides
--hash PROG     use hasher PROG instead of md5sum
--husk PROG     use husk around tool, e.g. 'valgrind -v'

-h, --help      print a short help screen
EOF
}

CLINE=$(getopt -o h \
	--long help,builddir:,srcdir:,hash:,husk: -n "${0}" -- "${@}")
eval set -- "${CLINE}"
while true; do
	case "${1}" in
	"-h"|"--help")
		usage
		exit 0
		;;
	"--builddir")
		builddir="${2}"
		shift 2
		;;
	"--srcdir")
		srcdir="${2}"
		shift 2
		;;
	"--hash")
		hash="${2}"
		shift 2
		;;
	"--husk")
		HUSK="${2}"
		shift 2
		;;
	--)
		shift
		break
		;;
	*)
		echo "could not parse options" >&2
		exit 1
		;;
	esac
done

## now in ${1} should be the test file
testfile="${1}"

## some helper funs
xrealpath()
{
	readlink -f "${1}" 2>/dev/null || \
		realpath "${1}" 2>/dev/null
}

## setup
fail=0
tool_stdout=$(mktemp "/tmp/tmp.XXXXXXXXXX")
tool_stderr=$(mktemp "/tmp/tmp.XXXXXXXXXX")

## also set srcdir in case the testfile needs it
if test -z "${srcdir}"; then
	srcdir=$(xrealpath $(dirname "${0}"))
else
	srcdir=$(xrealpath "${srcdir}")
fi

## source the check
. "${testfile}" || fail=1

rm_if_not_src()
{
	file="${1}"
	srcd="${2:-${srcdir}}"
	dirf=$(dirname "${file}")

	if test "${dirf}" -ef "${srcd}"; then
		## treat as precious source file
		:
	elif test "$(pwd)" -ef "${srcd}"; then
		## treat as precious source file
		:
	else
		rm -vf -- "${file}"
	fi
}

myexit()
{
	rm_if_not_src "${stdin}" "${srcdir}"
	rm_if_not_src "${stdout}" "${srcdir}"
	rm_if_not_src "${stderr}" "${srcdir}"
	rm_if_not_src "${OUTFILE}"
	rm_if_not_src "${CMDFILE}" "${srcdir}"
	rm -f -- "${tool_stdout}" "${tool_stderr}"
	exit ${1:-1}
}

find_file()
{
	file="${1}"

	if test -z "${file}"; then
		:
	elif test -r "${file}"; then
		echo "${file}"
	elif test -r "${builddir}/${file}"; then
		xrealpath "${builddir}/${file}"
	elif test -r "${srcdir}/${file}"; then
		xrealpath "${srcdir}/${file}"
	fi
}

eval_echo()
{
	local ret
	local tmpf

	echo -n ${@} >&3
	if test "/dev/stdin" -ef "/dev/null"; then
		echo >&3
	else
		echo "<<EOF" >&3
		tmpf=$(mktemp "/tmp/tmp.XXXXXXXXXX")
		tee "${tmpf}" >&3
		echo "EOF" >&3
	fi

	eval ${@} < "${tmpf:-/dev/null}"
	ret=${?}
	rm -f -- "${tmpf}"
	return ${ret}
}

exec_echo()
{
	local ret
	local HUSK="${1}"
	shift

	## repeat stdin to &3
	echo "TOOL=${TOOL}" | cat - "${@}" >&3
	echo "TOOL=${TOOL}" | cat - "${@}" | "/bin/sh"
	ret=${?}
	return ${ret}
}

hexdiff()
{
	local file1="${1}"
	local file2="${2}"
	local tmp1=$(mktemp)
	local tmp2=$(mktemp)

	hextool()
	{
		local file="${1}"
		command -p "hexdump" -C "${file}" || \
			command -p "xxd" "${file}" || \
			command -p "od" -A x -v -t x2 "${file}"
	}

	hextool "${file1}" > "${tmp1}"
	hextool "${file2}" > "${tmp2}"
	diff -u "${tmp1}" "${tmp2}"

	rm -f "${tmp1}" "${tmp2}"
}

## check if everything's set
if test -z "${TOOL}"; then
	echo "variable \${TOOL} not set" >&2
	myexit 1
fi

## set finals
if test -x "${builddir}/${TOOL}"; then
	TOOL=$(xrealpath "${builddir}/${TOOL}")
fi

stdin=$(find_file "${stdin}")
stdout=$(find_file "${stdout}")
stderr=$(find_file "${stderr}")

## for binary reference files
if test -n "${REFFILE}"; then
	REFFILE=$(find_file "${REFFILE}")
fi

## check if we used a CMDFILE instead of CMDLINE
if test -n "${CMDFILE}"; then
	CMDFILE=$(find_file "${CMDFILE}")
	exec_echo "${HUSK}" "${CMDFILE}" 3>&2 \
		> "${tool_stdout}" 2> "${tool_stderr}" || fail=${?}
else
	eval_echo "${HUSK}" "${TOOL}" "${CMDLINE}" \
		< "${stdin:-/dev/null}" \
		3>&2 \
		> "${tool_stdout}" 2> "${tool_stderr}" || fail=${?}
fi

echo
if test "${EXPECT_EXIT_CODE}" = "${fail}"; then
	fail=0
fi

if test -r "${stdout}"; then
	diff -u "${stdout}" "${tool_stdout}" || fail=1
elif test -s "${tool_stdout}"; then
	echo
	echo "test stdout was:"
	cat "${tool_stdout}" >&2
	echo
fi
if test -r "${stderr}"; then
	diff -u "${stderr}" "${tool_stderr}" || fail=1
elif test -s "${tool_stderr}"; then
	echo
	echo "test stderr was:"
	cat "${tool_stderr}" >&2
	echo
fi

## check if we need to hash stuff
if test -r "${OUTFILE}" -a -r "${REFFILE}"; then
	## check for differing files
	if ! diff -q "${REFFILE}" "${OUTFILE}"; then
		## failed due to diff -q returning non-nil
		fail=1
		hexdiff "${REFFILE}" "${OUTFILE}"
	fi

elif test -r "${OUTFILE}" -a -n "${OUTFILE_SHA1}"; then
	sha1sum "${OUTFILE}" |
	while read sum rest; do
		if test "${sum}" != "${OUTFILE_SHA1}"; then
			cat <<EOF >&2
outfile (${OUTFILE}) hashes do not match:
SHOULD BE: ${OUTFILE_SHA1}
ACTUAL:    ${sum}
EOF
			exit 1
		fi
	done || fail=1

else
	## everything's fine innit?
	:
fi

myexit ${fail}

## ut-test.sh ends here
