m4_define([maj_ver], [0])
m4_define([med_ver], [3])
m4_define([min_ver], [0])
m4_define([UCL_VERSION], [maj_ver.med_ver.min_ver])

AC_SUBST([UCL_VERSION])
AC_INIT([libucl],[UCL_VERSION],[https://github.com/vstakhov/libucl],[libucl])
AC_CONFIG_SRCDIR([configure.ac])
AM_INIT_AUTOMAKE([1.11 foreign -Wno-portability no-dist-gzip dist-xz])

AC_PROG_CC_C99
LT_INIT
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

AC_C_CONST
AC_TYPE_SIZE_T

AC_CHECK_HEADERS_ONCE([fcntl.h unistd.h])
AC_TYPE_OFF_T
AC_FUNC_MMAP

if test "$ac_cv_func_mmap_fixed_mapped" = "yes"; then
    FILEMAP=unixfilemap
else
    FILEMAP=readfilemap
fi
AC_SUBST(FILEMAP)
AC_CHECK_HEADERS_ONCE([fcntl.h])
AC_CHECK_HEADERS_ONCE([sys/types.h])
AC_CHECK_HEADERS_ONCE([sys/stat.h])
AC_CHECK_HEADERS_ONCE([sys/param.h])
AC_CHECK_HEADERS_ONCE([stdlib.h])
AC_CHECK_HEADERS_ONCE([string.h])
AC_CHECK_HEADERS_ONCE([unistd.h])
AC_CHECK_HEADERS_ONCE([stdbool.h])
AC_CHECK_HEADERS_ONCE([ctype.h])
AC_CHECK_HEADERS_ONCE([errno.h])
AC_CHECK_HEADERS_ONCE([limits.h])
AC_CHECK_HEADERS_ONCE([stdio.h])

AC_SEARCH_LIBS([SSL_library_init], [ssl], [
	AC_DEFINE(HAVE_OPENSSL, 1, [Define to 1 if you have the 'ssl' library (-lssl).])
	LIBSSL_LIB="-lssl"
	LIBS_EXTRA="${LIBS_EXTRA} -lssl"
	], [-lcrypto])
	
AC_CHECK_HEADER([fetch.h], [
	AC_DEFINE(HAVE_FETCH_H, 1, [Define to 1 if you have the <fetch.h> header file.])
	AC_CHECK_LIB(sbuf, fetchXGet, [
		AC_DEFINE(HAVE_LIBFETCH, 1, [Define to 1 if you have the 'fetch' library (-fetch).])
		LIBFETCH_LIBS="-lfetch"
		have_libfetch="yes"
		LIBS_EXTRA="${LIBS_EXTRA} -lfetch"
	])
], [])
AC_SUBST([LIBFETCH_LIBS])

AS_IF([ test "x$have_libfetch" != "xyes"], [
dnl Fallback to libcurl
PKG_CHECK_MODULES([CURL], [libcurl], [
	AC_DEFINE(CURL_FOUND, 1, [Use libcurl])
	LIBS_EXTRA="${LIBS_EXTRA} -lcurl"], 
[])], [])
AC_SUBST([CURL_FOUND])
AC_SUBST([CURL_LIBS])
AC_SUBST([CURL_CFLAGS])

AC_CONFIG_FILES(Makefile \
	libucl.pc)
AC_OUTPUT