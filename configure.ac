AC_INIT(parallel,1.3.0)
AC_COPYRIGHT([Copyright E. Rosten 2006, 2007, 2008])

AC_PROG_CC
AC_PROG_CXX

targets=parallel
all_options="parallel ssh_parallel"

AC_LANG(C++)

AC_CHECK_LIB(c, printf, [cc=1], [cc=0])

if test $cc = 0
then
	AC_MSG_NOTICE(Something is wrong with the C++ compiler)
else
	targets="$targets ssh_parallel"
fi


AC_SUBST(targets, $targets)
AC_CONFIG_HEADERS(config.h)

AC_OUTPUT(Makefile)

echo
echo Programs:
echo $targets
echo
echo Missing programs:
echo $all_options $targets | tr ' ' '\n' | sort | uniq -u | tr '\n' ' '
echo
echo

