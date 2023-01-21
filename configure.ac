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

AC_INIT([gcon], [./version.sh], [https://github.com/streaksu/gcon/issues], [gcon])

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
