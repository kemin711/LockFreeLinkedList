#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([lkfdatastruct], [1.0.0], [kmzhou@yahoo.com])
AC_CONFIG_SRCDIR([src/reclaimer.cpp])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([-Wall -Wportability subdir-objects])
AM_PROG_AR
LT_PREREQ([2.4])
LT_INIT([dlopen])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_INSTALL

# Checks for libraries.
AC_CHECK_LIB([pthread], [pthread_create])

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_SIZE_T
AC_LANG_PUSH([C++])
AX_CXX_COMPILE_STDCXX(17, [ext], [mandatory])

# Checks for library functions.

AC_CONFIG_FILES([Makefile src/Makefile test/Makefile])
AC_OUTPUT
