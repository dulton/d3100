#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([arm_teacher_detect], [0.1], [BUG-REPORT-ADDRESS])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX

# Checks for libraries.

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AC_ARG_ENABLE([debug],
		[ --enable-debug  enable debug],
		[case "${enableval}" in
			yes) debug=true ;;
			no) debug=false ;;
		 esac],
		[debug=false])

if test x$debug = xtrue; then
	CXXFLAGS="-O0 -g"
	CFLAGS="-O0 -g"
	LDFLAGS="-g -O0"
else
	AM_CXXFLAGS="-O2"
fi

# dummy mode check
AC_ARG_ENABLE([dummy],
		[ --enable-dummy  enable DUMMY mode],
		[case "${enableval}" in
			yes) dummy=true ;;
			no) dummy=false ;;
		 esac],
		[dummy=false])
AM_CONDITIONAL([DUMMY], [test x$dummy = xtrue])


# vars
AC_SUBST([AM_CXXFLAGS])
AC_SUBST([AM_CFLAGS])

AC_CONFIG_FILES([Makefile])

AC_OUTPUT
