# configure.ac: Project's autoconf script.
# Copyright (C) 2023 streaksu
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

AC_INIT([gcon], m4_esyscmd([./version.sh]), [https://github.com/streaksu/gcon/issues], [gcon])

AC_PREREQ(2.69)
AC_CONFIG_AUX_DIR([build-aux])

SRCDIR="$(cd "$srcdir" && pwd -P)"
BUILDDIR="$(pwd -P)"

AC_SUBST(SRCDIR)
AC_SUBST(BUILDDIR)

test "x$CFLAGS" = "x" && CFLAGS='-g -O2 -pipe -Wall -Wextra'

AC_LANG(C)
AC_PROG_CC

AC_CHECK_HEADERS([stdio.h unistd.h fcntl.h linux/fb.h stdlib.h sys/mman.h sys/wait.h sys/ioctl.h termios.h])

werror_state="no"
AC_ARG_ENABLE([werror],
    AS_HELP_STRING([--enable-werror], [treat warnings as errors]),
    werror_state="$enableval")
if test "$werror_state" = "yes"; then
    CFLAGS="$CFLAGS -Werror"
fi

AC_PROG_MKDIR_P
case "$MKDIR_P" in
    .*) MKDIR_P="$(cd "$(dirname "$MKDIR_P")" && pwd -P)/$(basename "$MKDIR_P")" ;;
esac
AC_PROG_INSTALL
case "$INSTALL" in
    .*) INSTALL="$(cd "$(dirname "$INSTALL")" && pwd -P)/$(basename "$INSTALL")" ;;
esac

AC_CHECK_PROG([FIND_FOUND], [find], [yes])
if ! test "x$FIND_FOUND" = "xyes"; then
    AC_MSG_ERROR([find not found, please install find before configuring])
fi

NATIVE_STRIP="$($CC -dumpmachine)"-strip
AC_CHECK_PROG([NATIVE_STRIP_FOUND], $NATIVE_STRIP, [yes])
if ! test "x$NATIVE_STRIP_FOUND" = "xyes"; then
    NATIVE_STRIP=strip
fi
AC_SUBST(NATIVE_STRIP)

AC_PREFIX_DEFAULT([/usr/local])
AC_CONFIG_FILES(GNUmakefile)
AC_OUTPUT
