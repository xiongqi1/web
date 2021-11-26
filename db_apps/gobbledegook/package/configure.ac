AC_INIT([ggk], [1.0])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([build-aux])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CHECK_LIB([pthread], pthread_create, [], [AC_MSG_ERROR([pthread library not found])])
AC_CHECK_LIB([glib-2.0], g_variant_new, [], [AC_MSG_ERROR([glib-2.0 library not found])])
AC_CHECK_LIB([gio-2.0], g_dbus_method_invocation_return_dbus_error, [], [AC_MSG_ERROR([gio-2.0 library not found])])
AC_CHECK_LIB([gobject-2.0], g_object_unref, [], [AC_MSG_ERROR([gobject-2.0 library not found])])
AX_CXX_COMPILE_STDCXX(11)
AC_PROG_RANLIB

AC_ARG_WITH(service_plugin,
   [AC_HELP_STRING([--with-service_plugin=name],
   [select service plugin @<:@default=test@:>@])],
   service_plugin=$withval,
   service_plugin=test)
   AM_CONDITIONAL(SERVICE_PLUGIN_SATURN, [test "$service_plugin" = "saturn" ])
   AM_CONDITIONAL(SERVICE_PLUGIN_MARS, [test "$service_plugin" = "mars" ])
   AM_CONDITIONAL(SERVICE_PLUGIN_MRSWIVEL, [test "$service_plugin" = "mrswivel" ])
   AM_CONDITIONAL(SERVICE_PLUGIN_TEST, [test "$service_plugin" = "test" ])

AC_ARG_ENABLE(casa_build, AC_HELP_STRING([--enable-casa_build],
   [enable casa db_apps buildsystem]), [enable_casa_build=${enableval}])
AM_CONDITIONAL(CASA_BUILD, test "${enable_casa_build}" != "yes")

AC_PROG_CXX
AM_PROG_AR

AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GIO_CFLAGS)
AC_SUBST(GOBJECT_CFLAGS)

LT_INIT

if test -z "${CASA_BUILD_TRUE}"; then
if pkg-config --atleast-version=2.00 glib-2.0; then
   GLIB_CFLAGS=`pkg-config --cflags glib-2.0`
else
   AC_MSG_ERROR(glib-2.0 not found)
fi

if pkg-config --atleast-version=2.00 gio-2.0; then
   GIO_CFLAGS=`pkg-config --cflags gio-2.0`
else
   AC_MSG_ERROR(gio-2.0 not found)
fi

if pkg-config --atleast-version=2.00 gobject-2.0; then
   GOBJECT_CFLAGS=`pkg-config --cflags gobject-2.0`
else
   AC_MSG_ERROR(gobject-2.0 not found)
fi
fi

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
 Makefile
 src/Makefile
 src/plugins/Makefile
 src/plugins/test/Makefile
 src/plugins/saturn/Makefile
 src/plugins/mars/Makefile
 src/plugins/mrswivel/Makefile
])

if test -z "${CASA_BUILD_TRUE}"; then
PKG_CHECK_MODULES([XML2], [libxml-2.0])
PKG_CHECK_MODULES([DBUS_1], [dbus-1])
fi

AC_SUBST([XML2_CFLAGS])
AC_SUBST([XML2_LIBS])
AC_SUBST([DBUS_1_CFLAGS])
AC_SUBST([DBUS_1_LIBS])

AC_OUTPUT
