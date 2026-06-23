AM_CPPFLAGS = -I$(top_srcdir)/sample-service/include -I$(top_builddir) $(OPENSSL_CFLAGS)

bin_PROGRAMS = sample-service

sample_service_SOURCES = \
    src/main.c \
    src/sample_service.c \
    src/globals.c

sample_service_LDADD = $(OPENSSL_LIBS)
