AC_INIT([gcon], [m4_esyscmd([./version.sh])], [https://github.com/streaksu/gcon/issues], [gcon])

AC_PREREQ([2.69])

AC_CONFIG_AUX_DIR([build-aux])

SRCDIR="$(cd "$srcdir" && pwd -P)"

AC_SUBST([SRCDIR])

AC_CANONICAL_HOST

# Portably convert relative paths into absolute paths.
rel2abs() {
    rel2abs_first=true
    for i in $1; do
        if test $rel2abs_first = true; then
            case "$i" in
                /*)
                    printf "%s" "$i"
                    ;;
                */*)
                    if test -d "$(dirname "$i")"; then
                        printf "%s" "$(cd "$(dirname "$i")" && pwd -P)/$(basename "$i")"
                    else
                        printf "false"
                    fi
                    ;;
                *)
                    printf "%s" "$i"
                    ;;
            esac
            rel2abs_first=false
        else
            printf " %s" "$i"
        fi
    done
    printf "\n"
}

test "x${CFLAGS+set}" = "x" && CFLAGS='-g -O2 -pipe'

AC_LANG([C])
AC_PROG_CC
CC="$(rel2abs "$CC")"

werror_state="no"
AC_ARG_ENABLE([werror],
    [AS_HELP_STRING([--enable-werror], [treat warnings as errors])],
    [werror_state="$enableval"])
if test "$werror_state" = "yes"; then
    AC_SUBST([WERROR_FLAG], [-Werror])
else
    AC_SUBST([WERROR_FLAG], [-Wno-error])
fi

AC_PROG_MKDIR_P
MKDIR_P="$(rel2abs "$MKDIR_P")"
AC_PROG_INSTALL
INSTALL="$(rel2abs "$INSTALL")"

AC_CHECK_PROG([FIND_FOUND], [find], [yes])
if ! test "x$FIND_FOUND" = "xyes"; then
    AC_MSG_ERROR([find not found, please install find before configuring])
fi

# $1 - UPPERCASEVAR, $2 - default program
AC_DEFUN([GET_PROG_FROM_TOOLCHAIN], [
    AC_ARG_VAR([$1], [$2 command @<:@default: $2@:>@])

    if ! test -z "$$1"; then
        first_elem="$(echo "$$1" | cut -f 1 -d " ")"
        case "$first_elem" in
            */*)
                if test -f "$first_elem" && test -x "$first_elem"; then
                    $1_USER_FOUND=yes
                fi
                ;;
            *)
                AC_CHECK_PROG([$1_USER_FOUND], [$$1], [yes])
                ;;
        esac

        if ! test "x$$1_USER_FOUND" = "xyes"; then
            AC_MSG_ERROR([$2 not found, please install $2 before configuring])
        fi
    else
        AC_CHECK_TOOL([$1], [$2], [:])
        if test "x$$1" = "x:"; then
            AC_MSG_ERROR([$2 not found, please install $2 or set $1 to a valid command])
        fi
    fi

    $1="$(rel2abs "$$1")"
])

GET_PROG_FROM_TOOLCHAIN([STRIP], [strip])

PKGCONF_LIBS_LIST=""

PKG_PROG_PKG_CONFIG
PKG_CONFIG="$(rel2abs "$PKG_CONFIG")"

for lib in $PKGCONF_LIBS_LIST; do
    set -e
    $PKG_CONFIG --exists --print-errors "$lib"
    set +e
done

PKGCONF_CFLAGS="$($PKG_CONFIG --cflags-only-other $PKGCONF_LIBS_LIST 2>/dev/null)"
PKGCONF_CPPFLAGS="$($PKG_CONFIG --cflags-only-I $PKGCONF_LIBS_LIST 2>/dev/null)"
PKGCONF_LIBS="$($PKG_CONFIG --libs $PKGCONF_LIBS_LIST 2>/dev/null)"

AC_SUBST([PKGCONF_CFLAGS])
AC_SUBST([PKGCONF_CPPFLAGS])
AC_SUBST([PKGCONF_LIBS])

OLD_CFLAGS="$CFLAGS"
OLD_CPPFLAGS="$CPPFLAGS"
OLD_LIBS="$LIBS"

CFLAGS="$CFLAGS $PKGCONF_CFLAGS"
CPPFLAGS="$PKGCONF_CPPFLAGS $CPPFLAGS"
LIBS="$LIBS $PKGCONF_LIBS"

AC_CHECK_HEADERS([stdio.h unistd.h pthread.h fcntl.h linux/fb.h sys/ttydefaults.h sys/syscall.h stdlib.h sys/mman.h sys/wait.h sys/ioctl.h termios.h ctype.h stdnoreturn.h pty.h],
    [], [AC_MSG_ERROR([required header not found])])

CFLAGS="$OLD_CFLAGS"
CPPFLAGS="$OLD_CPPFLAGS"
LIBS="$OLD_LIBS"

AC_PREFIX_DEFAULT([/usr/local])

AC_CONFIG_FILES([GNUmakefile])
AC_OUTPUT
