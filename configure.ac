AC_INIT([gcon], [0.69], [https://github.com/streaksu/gcon/issues], [gcon])

AC_PREREQ(2.69)

AC_CONFIG_AUX_DIR([build-aux])

SRCDIR="$(cd "$srcdir" && pwd -P)"
BUILDDIR="$(pwd -P)"

AC_SUBST(SRCDIR)
AC_SUBST(BUILDDIR)

test "x$CFLAGS" = "x" && CFLAGS='-g -O2 -pipe -Wall -Wextra'
AC_PROG_CC

werror_state="no"
AC_ARG_ENABLE([werror],
    AS_HELP_STRING([--enable-werror], [treat warnings as errors]),
    werror_state="$enableval")

if test "$werror_state" = "yes"; then
    AC_SUBST(WERROR, [-Werror])
fi

AC_PROG_MKDIR_P
AC_PROG_INSTALL

AC_CHECK_PROG([FIND_FOUND], [find], [yes])
if ! test "x$FIND_FOUND" = "xyes"; then
    AC_MSG_ERROR([find not found, please install find before configuring])
fi

AC_PREFIX_DEFAULT([/usr/local])
AC_CONFIG_FILES(GNUmakefile)
AC_OUTPUT
