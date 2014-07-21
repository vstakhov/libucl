libucl_lua_common_cflags=	-I$(top_srcdir)/src \
			-I$(top_srcdir)/include \
			-I$(top_srcdir)/uthash \
			-Wall -W -Wno-unused-parameter -Wno-pointer-sign
lib_LTLIBRARIES=	libucl_lua.la
libucl_lua_la_SOURCES=	lua_ucl.c
libucl_lua_la_CFLAGS=	$(libucl_lua_common_cflags) \
					@LUA_INCLUDE@
libucl_lua_la_LDFLAGS = -version-info @SO_VERSION@
libucl_lua_la_LIBADD=	@LIBFETCH_LIBS@ \
					@LIBCRYPTO_LIB@ \
					@LIBREGEX_LIB@ \
					@CURL_LIBS@ \
					@LUA_LIB@

include_HEADERS=	$(top_srcdir)/include/lua_ucl.h
