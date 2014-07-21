libucl_lua_common_cflags=	-I$(top_srcdir)/src \
			-I$(top_srcdir)/include \
			-I$(top_srcdir)/uthash \
			-Wall -W -Wno-unused-parameter -Wno-pointer-sign
lib_LTLIBRARIES=	libucl_lua.la
libucl_lua_la_SOURCES=	lua_ucl.c
libucl_lua_la_CFLAGS=	$(libucl_lua_common_cflags) \
					@LUA_INCLUDE@
libucl_lua_la_LDFLAGS = -module -avoid-version -rpath '$(libdir)'
libucl_lua_la_LIBADD=	@LIBFETCH_LIBS@ \
					@LIBCRYPTO_LIB@ \
					@LIBREGEX_LIB@ \
					@CURL_LIBS@ \
					@LUA_LIB@

include_HEADERS=	$(top_srcdir)/include/lua_ucl.h

ROCKSPEC = $(PACKAGE)-$(VERSION)-1.rockspec
EXTRA_DIST = $(PACKAGE).rockspec.in
DISTCLEANFILES = $(PACKAGE).rockspec

$(ROCKSPEC): $(PACKAGE).rockspec dist
	sed -e 's/@MD5@/'`$(MD5SUM) $(distdir).tar.gz | \
	cut -d " " -f 1`'/g' < $(PACKAGE).rockspec > $@