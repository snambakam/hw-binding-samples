AM_CPPFLAGS = -I$(top_srcdir)/sample-service/include -I$(top_builddir) -I$(top_builddir)/idl $(OPENSSL_CFLAGS) $(GRPCPP_CFLAGS) $(PROTOBUF_CFLAGS)
AM_CXXFLAGS = -std=c++17

bin_PROGRAMS = sample-service

sample_service_SOURCES = \
    src/main.c \
    src/sample_service.c \
    src/grpc_client.cc \
    src/globals.c

sample_service_LDADD = $(top_builddir)/idl/libkmsidl.a $(OPENSSL_LIBS) $(GRPCPP_LIBS) $(PROTOBUF_LIBS)

