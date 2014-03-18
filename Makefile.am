libucl_common_cflags=	-I$(top_srcdir)/src \
			-I$(top_srcdir)/include \
			-I$(top_srcdir)/uthash \
			-I$(top_srcdir)/src \
			-DPREFIX=\"$(prefix)\" \
			-Wall -W -Wno-unused-parameter -Wno-pointer-sign
lib_LTLIBRARIES=	libucl.la
libucl_la_SOURCES=	src/ucl_emitter.c \
					src/ucl_hash.c \
					src/ucl_parser.c \
					src/ucl_schema.c \
					src/ucl_util.c \
					src/xxhash.c
libucl_la_CFLAGS=	$(libucl_common_cflags)
libucl_la_LIBADD=	@LIBFETCH_LIBS@ \
					@CURL_LIBS@

include_HEADERS=	include/ucl.h

ACLOCAL_AMFLAGS = -I m4