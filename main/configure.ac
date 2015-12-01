#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([arm_teacher_detect], [0.1], [BUG-REPORT-ADDRESS])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AM_PROG_AR
AC_PROG_RANLIB

# Checks for libraries.

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AH_TEMPLATE([DEBUG_HIMAT], [to debug hiMat in x86])
AH_TEMPLATE([ZDEBUG], [to show more info ...])
AH_TEMPLATE([ZDEBUG_LOCK], [to enable thread death lock debug])


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
	AC_DEFINE([ZDEBUG])
	AC_DEFINE([DEBUG_HIMAT])
else
	AM_CXXFLAGS="-O2"
fi

LDFLAGS="-lpthread"

# dummy mode check
AC_ARG_ENABLE([dummy],
		[ --enable-dummy  enable DUMMY mode],
		[case "${enableval}" in
			yes) dummy=true ;;
			no) dummy=false ;;
		 esac],
		[dummy=false])
AM_CONDITIONAL([DUMMY], [test x$dummy = xtrue])


# ptz srv
ac_ptzsrv="false"
PKG_CHECK_MODULES([CCGNU2], [libccgnu2, libccext2],
		[ac_ptzsrv=true],
		["Not found libccgnu2, not to build ptzsrv"])

# opencv
ac_opencv="false"
PKG_CHECK_MODULES([OPENCV], [opencv],
		[ac_opencv=true],
		["Not found opencv, ???????"])

AM_CONDITIONAL([PTZSRV], [test x$ac_ptzsrv = xtrue])
AM_CONDITIONAL([OPENCV], [test x$ac_opencv = xtrue])

case "$host" in
arm*)
	CXXFLAGS="$CXXFLAGS -march=armv7-a -mcpu=cortex-a9 -mfloat-abi=softfp -mfpu=vfpv3-d16 -I/opt/arm/include"
	CFLAGS="$CFLAGS -march=armv7-a -mcpu=cortex-a9 -mfloat-abi=softfp -mfpu=vfpv3-d16"
	;;
esac

# vars
AC_SUBST([AM_CXXFLAGS])
AC_SUBST([AM_CFLAGS])

AC_CONFIG_FILES([Makefile])

AC_OUTPUT
