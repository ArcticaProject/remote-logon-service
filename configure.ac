AC_INIT(remote-logon-service, 1.0.1.0)

AC_PREREQ(2.53)
AM_INIT_AUTOMAKE([])

AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AM_PROG_CC_C_O
AM_SILENT_RULES([yes])
AC_CONFIG_MACRO_DIR([m4])

# Initialize libtool
LT_PREREQ([2.2])
LT_INIT([disable-static])

AC_CONFIG_HEADER(config.h)
AC_CONFIG_FILES([
    Makefile
	src/Makefile
	data/Makefile
	po/Makefile.in
	tests/Makefile
])

PKG_CHECK_MODULES(SERVICE, glib-2.0 gio-2.0 gio-unix-2.0 json-glib-1.0 libnm-glib libsoup-2.4)
PKG_CHECK_MODULES(TEST, dbustest-1)

AC_CHECK_LIB(gcrypt, gcry_cipher_get_algo_keylen, GCRYPT_LIBS="-lgcrypt")

if test x"$GCRYPT_LIBS" = x ; then
	echo "You need libgcrypt to compile remote-logon-service";
	exit
fi

###########################
# Local Install
###########################

with_localinstall="no"
AC_ARG_ENABLE(localinstall, AS_HELP_STRING([--enable-localinstall],
                                           [install all files locally (for distcheck)]),
              with_localinstall=$enableval,
              with_localinstall=no)

##############################
# Custom Junk
##############################

AC_DEFUN([AC_DEFINE_PATH], [
    test "x$prefix" = xNONE && prefix="$ac_default_prefix"
    test "x$exec_prefix" = xNONE && exec_prefix='${prefix}'
    ac_define_path=`eval echo [$]$2`
    ac_define_path=`eval echo [$]ac_define_path`
    $1="$ac_define_path"
    AC_SUBST($1)
    ifelse($3, ,
        AC_DEFINE_UNQUOTED($1, "$ac_define_path"),
        AC_DEFINE_UNQUOTED($1, "$ac_define_path", $3))
])

###########################
# DBus Service Info
###########################

if test "x$with_localinstall" = "xyes"; then
	DBUSSERVICEDIR="${datadir}/dbus-1/services/"
else
	DBUSSERVICEDIR=`$PKG_CONFIG --variable=session_bus_services_dir dbus-1`
fi
AC_SUBST(DBUSSERVICEDIR)

###########################
# gcov coverage reporting
###########################

m4_include([m4/gcov.m4])
AC_TDD_GCOV
AM_CONDITIONAL([HAVE_GCOV], [test "x$ac_cv_check_gcov" = xyes])
AM_CONDITIONAL([HAVE_LCOV], [test "x$ac_cv_check_lcov" = xyes])
AM_CONDITIONAL([HAVE_GCOVR], [test "x$ac_cv_check_gcovr" = xyes])
AC_SUBST(COVERAGE_CFLAGS)
AC_SUBST(COVERAGE_LDFLAGS)
AC_SUBST(GCRYPT_LIBS)

###########################
# Internationalization
###########################

IT_PROG_INTLTOOL([0.35.0])
GETTEXT_PACKAGE=remote-logon-service
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [gettext package])
AC_DEFINE_PATH(LOCALEDIR, "${datadir}/locale", [locale directory])
AM_GLIB_GNU_GETTEXT

###########################
# Flags
###########################

AC_SUBST(AM_CFLAGS, "-Wall -Werror")
AC_SUBST(AM_CPPFLAGS, $AM_CFLAGS)

AC_OUTPUT
