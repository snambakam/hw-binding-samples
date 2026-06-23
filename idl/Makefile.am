# Generated gRPC/protobuf C++ sources for the KMS IDL.
#
# protoc emits the .pb.{cc,h} and .grpc.pb.{cc,h} into the build tree; both
# programs link the resulting static library.

AM_CPPFLAGS = $(PROTOBUF_CFLAGS) $(GRPCPP_CFLAGS)
AM_CXXFLAGS = -std=c++17

noinst_LIBRARIES = libkmsidl.a

nodist_libkmsidl_a_SOURCES = \
    kms.pb.cc \
    kms.grpc.pb.cc

BUILT_SOURCES = \
    kms.pb.cc \
    kms.pb.h \
    kms.grpc.pb.cc \
    kms.grpc.pb.h

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = kms.proto

PROTO = $(top_srcdir)/idl/kms.proto

kms.pb.cc kms.pb.h: $(PROTO)
	$(PROTOC) --proto_path=$(top_srcdir)/idl --cpp_out=$(builddir) $(PROTO)

kms.grpc.pb.cc kms.grpc.pb.h: $(PROTO)
	$(PROTOC) --proto_path=$(top_srcdir)/idl \
	    --grpc_out=$(builddir) \
	    --plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN) $(PROTO)
