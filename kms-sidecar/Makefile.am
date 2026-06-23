AM_CPPFLAGS = -I$(top_srcdir)/kms-sidecar/include -I$(top_builddir) $(OPENSSL_CFLAGS) $(FAPI_CFLAGS)

bin_PROGRAMS = kms-sidecar

kms_sidecar_SOURCES = \
    src/main.c \
    src/kms_sidecar.c \
    src/globals.c

kms_sidecar_LDADD = $(OPENSSL_LIBS) $(FAPI_LIBS)
