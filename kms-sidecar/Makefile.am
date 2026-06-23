AM_CPPFLAGS = -I$(top_srcdir)/kms-sidecar/include -I$(top_builddir) -I$(top_builddir)/idl $(OPENSSL_CFLAGS) $(FAPI_CFLAGS) $(GRPCPP_CFLAGS) $(PROTOBUF_CFLAGS)
AM_CXXFLAGS = -std=c++17

bin_PROGRAMS = kms-sidecar

kms_sidecar_SOURCES = \
    src/main.c \
    src/kms_sidecar.c \
    src/grpc_server.cc \
    src/globals.c

kms_sidecar_LDADD = $(top_builddir)/idl/libkmsidl.a $(OPENSSL_LIBS) $(FAPI_LIBS) $(GRPCPP_LIBS) $(PROTOBUF_LIBS)

