AC_INIT(parallel,1.3.2)
AC_COPYRIGHT([Copyright E. Rosten 2006, 2007, 2008])


dnl APPEND(var, value)
dnl This appends vale to a shell variable var
define(APPEND, [$1="$$1 $2"])

dnl TEST_AND_SET_CXXFLAG(flag, [program], [run])
dnl
dnl This attempts to compile a and run program with a certain compiler flag.
dnl If no program is given, then the minimal C++ program is compiled, and 
dnl this tests just the validity of the compiler flag. 
define([TEST_AND_SET_CXXFLAG],[
	if test "$3" == ""
	then
		AC_MSG_CHECKING([if compiler flag $1 works])	
	else
		AC_MSG_CHECKING([$3])	
	fi
	save_CXXFLAGS="$CXXFLAGS"
	APPEND(CXXFLAGS, [$1])

	m4_if([$2],[],[pushdef(prog, [int main(){}])], [pushdef(prog, [$2])])

	m4_if([$4],[run],
		[AC_RUN_IFELSE([prog], [cvd_conf_test=1],[cvd_conf_test=0], [cvd_conf_test=0])],
		[AC_COMPILE_IFELSE([prog], [cvd_conf_test=1],[cvd_conf_test=0])]
	)
	
	
	popdef([prog])

	if test $cvd_conf_test = 1
	then
		AC_MSG_RESULT(yes)
		ts_success=yes
	else
		AC_MSG_RESULT(no)
		CXXFLAGS="$save_CXXFLAGS"
		ts_success=no
	fi
])









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

TEST_AND_SET_CXXFLAG(-Wall)
TEST_AND_SET_CXXFLAG(-Wextra)


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

