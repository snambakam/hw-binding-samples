AC_PREREQ([2.69])
AC_INIT([hw-binding-samples], [0.1.0], [noreply@example.com])
AM_INIT_AUTOMAKE([foreign subdir-objects -Wall -Werror])
AC_CONFIG_SRCDIR([kms-sidecar/src/main.c])
AC_CONFIG_HEADERS([config.h])

AC_PROG_CC
AC_PROG_CXX
AM_PROG_AR
AC_PROG_RANLIB
PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES([OPENSSL], [openssl])
PKG_CHECK_MODULES([FAPI], [tss2-fapi])
PKG_CHECK_MODULES([PROTOBUF], [protobuf])
PKG_CHECK_MODULES([GRPCPP], [grpc++])

AC_PATH_PROG([PROTOC], [protoc])
AS_IF([test -z "$PROTOC"], [AC_MSG_ERROR([protoc not found; install protobuf-compiler])])
AC_PATH_PROG([GRPC_CPP_PLUGIN], [grpc_cpp_plugin])
AS_IF([test -z "$GRPC_CPP_PLUGIN"], [AC_MSG_ERROR([grpc_cpp_plugin not found; install grpc-plugins])])

AC_SUBST([OPENSSL_CFLAGS])
AC_SUBST([OPENSSL_LIBS])
AC_SUBST([FAPI_CFLAGS])
AC_SUBST([FAPI_LIBS])
AC_SUBST([PROTOBUF_CFLAGS])
AC_SUBST([PROTOBUF_LIBS])
AC_SUBST([GRPCPP_CFLAGS])
AC_SUBST([GRPCPP_LIBS])
AC_SUBST([PROTOC])
AC_SUBST([GRPC_CPP_PLUGIN])

AC_CONFIG_FILES([
    Makefile
    idl/Makefile
    kms-sidecar/Makefile
    sample-service/Makefile
])
AC_OUTPUT
