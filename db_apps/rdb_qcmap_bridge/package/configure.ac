AC_INIT([rdb_qcmap_bridge], [1.0])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_MACRO_DIRS([m4])
AC_CONFIG_HEADER([config.h])
AM_MAINTAINER_MODE

# Checks for programs.
AM_PROG_AR
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_LIBTOOL
AC_PROG_AWK
AC_PROG_CPP
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
PKG_PROG_PKG_CONFIG

# automake
AC_CONFIG_FILES([
    Makefile
    src/Makefile
])

# modules

PKG_CHECK_MODULES([DATA], [data])
AC_SUBST([DATA_CFLAGS])
AC_SUBST([DATA_LIBS])

PKG_CHECK_MODULES([DSUTILS], [dsutils])
AC_SUBST([DSUTILS_CFLAGS])
AC_SUBST([DSUTILS_LIBS])

PKG_CHECK_MODULES([QMIFRAMEWORK], [qmi-framework])
AC_SUBST([QMIFRAMEWORK_CFLAGS])
AC_SUBST([QMIFRAMEWORK_LIBS])

PKG_CHECK_MODULES([QMI], [qmi])
AC_SUBST([QMI_CFLAGS])
AC_SUBST([QMI_LIBS])


PKG_CHECK_MODULES([ESTD], [libestd])
AC_SUBST([ESTD_CFLAGS])
AC_SUBST([ESTD_LIBS])

AC_CHECK_TOOL([STRIP],[strip])

# flags

AC_ARG_ENABLE(debug,
AS_HELP_STRING([--enable-debug],
               [enable debugging, default: no]),
[case "${enableval}" in
             yes) debug=true ;;
             no)  debug=false ;;
             *)   AC_MSG_ERROR([bad value ${enableval} for --enable-debug]) ;;
esac],
[debug=false])

AM_CONDITIONAL(DEBUG, test x"$debug" = x"true")

AC_OUTPUT
