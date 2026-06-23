AC_PREREQ([2.69])
AC_INIT([hw-binding-samples], [0.1.0], [noreply@example.com])
AM_INIT_AUTOMAKE([foreign subdir-objects -Wall -Werror])
AC_CONFIG_SRCDIR([kms-sidecar/src/main.c])
AC_CONFIG_HEADERS([config.h])

AC_PROG_CC
PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES([OPENSSL], [openssl])
PKG_CHECK_MODULES([FAPI], [tss2-fapi])

AC_SUBST([OPENSSL_CFLAGS])
AC_SUBST([OPENSSL_LIBS])
AC_SUBST([FAPI_CFLAGS])
AC_SUBST([FAPI_LIBS])

AC_CONFIG_FILES([
    Makefile
    kms-sidecar/Makefile
    sample-service/Makefile
])
AC_OUTPUT
