/* Copyright (c) 2013, Vsevolod Stakhov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *       * Redistributions of source code must retain the above copyright
 *         notice, this list of conditions and the following disclaimer.
 *       * Redistributions in binary form must reproduce the above copyright
 *         notice, this list of conditions and the following disclaimer in the
 *         documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ucl.h"
#include "ucl_internal.h"

#ifdef HAVE_OPENSSL
#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/evp.h>
#endif

/**
 * @file rcl_util.c
 * Utilities for rcl parsing
 */


static void
ucl_obj_free_internal (ucl_object_t *obj, bool allow_rec)
{
	ucl_object_t *sub, *tmp;

	while (obj != NULL) {
		if (obj->key != NULL) {
			free (obj->key);
		}

		if (obj->type == UCL_STRING) {
			free (obj->value.sv);
		}
		else if (obj->type == UCL_ARRAY) {
			sub = obj->value.ov;
			while (sub != NULL) {
				tmp = sub->next;
				ucl_obj_free_internal (sub, false);
				sub = tmp;
			}
		}
		else if (obj->type == UCL_OBJECT) {
			HASH_ITER (hh, obj->value.ov, sub, tmp) {
				HASH_DELETE (hh, obj->value.ov, sub);
				ucl_obj_free_internal (sub, true);
			}
		}
		tmp = obj->next;
		UCL_FREE (sizeof (ucl_object_t), obj);
		obj = tmp;

		if (!allow_rec) {
			break;
		}
	}
}

void
ucl_obj_free (ucl_object_t *obj)
{
	ucl_obj_free_internal (obj, true);
}

void
ucl_unescape_json_string (char *str)
{
	char *t = str, *h = str;
	int i, uval;

	/* t is target (tortoise), h is source (hare) */

	while (*h != '\0') {
		if (*h == '\\') {
			h ++;
			switch (*h) {
			case 'n':
				*t++ = '\n';
				break;
			case 'r':
				*t++ = '\r';
				break;
			case 'b':
				*t++ = '\b';
				break;
			case 't':
				*t++ = '\t';
				break;
			case 'f':
				*t++ = '\f';
				break;
			case '\\':
				*t++ = '\\';
				break;
			case '"':
				*t++ = '"';
				break;
			case 'u':
				/* Unicode escape */
				uval = 0;
				for (i = 0; i < 4; i++) {
					uval <<= 4;
					if (isdigit (h[i])) {
						uval += h[i] - '0';
					}
					else if (h[i] >= 'a' && h[i] <= 'f') {
						uval += h[i] - 'a' + 10;
					}
					else if (h[i] >= 'A' && h[i] <= 'F') {
						uval += h[i] - 'A' + 10;
					}
				}
				h += 3;
				/* Encode */
				if(uval < 0x80) {
					t[0] = (char)uval;
					t ++;
				}
				else if(uval < 0x800) {
					t[0] = 0xC0 + ((uval & 0x7C0) >> 6);
					t[1] = 0x80 + ((uval & 0x03F));
					t += 2;
				}
				else if(uval < 0x10000) {
					t[0] = 0xE0 + ((uval & 0xF000) >> 12);
					t[1] = 0x80 + ((uval & 0x0FC0) >> 6);
					t[2] = 0x80 + ((uval & 0x003F));
					t += 3;
				}
				else if(uval <= 0x10FFFF) {
					t[0] = 0xF0 + ((uval & 0x1C0000) >> 18);
					t[1] = 0x80 + ((uval & 0x03F000) >> 12);
					t[2] = 0x80 + ((uval & 0x000FC0) >> 6);
					t[3] = 0x80 + ((uval & 0x00003F));
					t += 4;
				}
				else {
					*t++ = '?';
				}
				break;
			default:
				*t++ = '?';
				break;
			}
			h ++;
		}
		else {
			*t++ = *h++;
		}
	}
}

ucl_object_t*
ucl_parser_get_object (struct ucl_parser *parser, UT_string **err)
{
	if (parser->state != UCL_STATE_INIT && parser->state != UCL_STATE_ERROR) {
		return ucl_obj_ref (parser->top_obj);
	}

	return NULL;
}

void
ucl_parser_free (struct ucl_parser *parser)
{
	struct ucl_stack *stack, *stmp;
	struct ucl_macro *macro, *mtmp;
	struct ucl_chunk *chunk, *ctmp;
	struct ucl_pubkey *key, *ktmp;

	if (parser->top_obj != NULL) {
		ucl_obj_unref (parser->top_obj);
	}

	LL_FOREACH_SAFE (parser->stack, stack, stmp) {
		free (stack);
	}
	HASH_ITER (hh, parser->macroes, macro, mtmp) {
		UCL_FREE (sizeof (struct ucl_macro), macro);
	}
	LL_FOREACH_SAFE (parser->chunks, chunk, ctmp) {
		UCL_FREE (sizeof (struct ucl_chunk), chunk);
	}
	LL_FOREACH_SAFE (parser->keys, key, ktmp) {
		UCL_FREE (sizeof (struct ucl_pubkey), key);
	}

	UCL_FREE (sizeof (struct ucl_parser), parser);
}

bool
ucl_pubkey_add (struct ucl_parser *parser, const unsigned char *key, size_t len, UT_string **err)
{
	struct ucl_pubkey *nkey;
#ifndef HAVE_OPENSSL
	ucl_create_err (err, "cannot check signatures without openssl");
	return false;
#else
# if (OPENSSL_VERSION_NUMBER < 0x10000000L)
	ucl_create_err (err, "cannot check signatures, openssl version is unsupported");
	return EXIT_FAILURE;
# else
	BIO *mem;

	mem = BIO_new_mem_buf ((void *)key, len);
	nkey = UCL_ALLOC (sizeof (struct ucl_pubkey));
	nkey->key = PEM_read_bio_PUBKEY (mem, &nkey->key, NULL, NULL);
	BIO_free (mem);
	if (nkey->key == NULL) {
		UCL_FREE (sizeof (struct ucl_pubkey), nkey);
		ucl_create_err (err, "%s",
				ERR_error_string (ERR_get_error (), NULL));
		return false;
	}
	LL_PREPEND (parser->keys, nkey);
# endif
#endif
	return true;
}

#ifdef CURL_FOUND
struct ucl_curl_cbdata {
	unsigned char *buf;
	size_t buflen;
};

static size_t
ucl_curl_write_callback (void* contents, size_t size, size_t nmemb, void* ud)
{
	struct ucl_curl_cbdata *cbdata = ud;
	size_t realsize = size * nmemb;

	cbdata->buf = g_realloc (cbdata->buf, cbdata->buflen + realsize + 1);
	if (cbdata->buf == NULL) {
		return 0;
	}

	memcpy (&(cbdata->buf[cbdata->buflen]), contents, realsize);
	cbdata->buflen += realsize;
	cbdata->buf[cbdata->buflen] = 0;

	return realsize;
}
#endif

/**
 * Fetch a url and save results to the memory buffer
 * @param url url to fetch
 * @param len length of url
 * @param buf target buffer
 * @param buflen target length
 * @return
 */
static bool
ucl_fetch_url (const unsigned char *url, unsigned char **buf, size_t *buflen, UT_string **err)
{

#ifdef HAVE_FETCH_H
	struct url *fetch_url;
	struct url_stat us;
	FILE *in;

	fetch_url = fetchParseURL (url);
	if (fetch_url == NULL) {
		ucl_create_err (err, "invalid URL %s: %s",
				url, strerror (errno));
		return false;
	}
	if ((in = fetchXGet (fetch_url, &us, "")) == NULL) {
		ucl_create_err (err, "cannot fetch URL %s: %s",
				url, strerror (errno));
		fetchFreeURL (fetch_url);
		return false;
	}

	*buflen = us.size;
	*buf = g_malloc (*buflen);
	if (*buf == NULL) {
		ucl_create_err (err, "cannot allocate buffer for URL %s: %s",
				url, strerror (errno));
		fclose (in);
		fetchFreeURL (fetch_url);
		return false;
	}

	if (fread (*buf, *buflen, 1, in) != 1) {
		ucl_create_err (err, "cannot read URL %s: %s",
				url, strerror (errno));
		fclose (in);
		fetchFreeURL (fetch_url);
		return false;
	}

	fetchFreeURL (fetch_url);
	return true;
#elif defined(CURL_FOUND)
	CURL *curl;
	int r;
	struct ucl_curl_cbdata cbdata;

	curl = curl_easy_init ();
	if (curl == NULL) {
		ucl_create_err (err, "CURL interface is broken");
		return false;
	}
	if ((r = curl_easy_setopt (curl, CURLOPT_URL, url)) != CURLE_OK) {
		ucl_create_err (err, "invalid URL %s: %s",
				url, curl_easy_strerror (r));
		curl_easy_cleanup (curl);
		return false;
	}
	curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, ucl_curl_write_callback);
	cbdata.buf = *buf;
	cbdata.buflen = *buflen;
	curl_easy_setopt (curl, CURLOPT_WRITEDATA, &cbdata);

	if ((r = curl_easy_perform (curl)) != CURLE_OK) {
		ucl_create_err (err, "error fetching URL %s: %s",
				url, curl_easy_strerror (r));
		curl_easy_cleanup (curl);
		if (buf != NULL) {
			free (buf);
		}
		return false;
	}
	*buf = cbdata.buf;
	*buflen = cbdata.buflen;

	return true;
#else
	ucl_create_err (err, "URL support is disabled");
	return false;
#endif
}

/**
 * Fetch a file and save results to the memory buffer
 * @param filename filename to fetch
 * @param len length of filename
 * @param buf target buffer
 * @param buflen target length
 * @return
 */
static bool
ucl_fetch_file (const unsigned char *filename, unsigned char **buf, size_t *buflen, UT_string **err)
{
	int fd;
	struct stat st;

	if (stat (filename, &st) == -1) {
		ucl_create_err (err, "cannot stat file %s: %s",
				filename, strerror (errno));
		return false;
	}
	if ((fd = open (filename, O_RDONLY)) == -1) {
		ucl_create_err (err, "cannot open file %s: %s",
				filename, strerror (errno));
		return false;
	}
	if ((*buf = mmap (NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0)) == MAP_FAILED) {
		close (fd);
		ucl_create_err (err, "cannot mmap file %s: %s",
				filename, strerror (errno));
		return false;
	}
	*buflen = st.st_size;
	close (fd);

	return true;
}


#if (defined(HAVE_OPENSSL) && OPENSSL_VERSION_NUMBER >= 0x10000000L)
static inline bool
ucl_sig_check (const unsigned char *data, size_t datalen,
		const unsigned char *sig, size_t siglen, struct ucl_parser *parser)
{
	struct ucl_pubkey *key;
	char dig[EVP_MAX_MD_SIZE];
	unsigned int diglen;
	EVP_PKEY_CTX *key_ctx;
	EVP_MD_CTX *sign_ctx = NULL;

	sign_ctx = EVP_MD_CTX_create ();

	LL_FOREACH (parser->keys, key) {
		key_ctx = EVP_PKEY_CTX_new (key->key, NULL);
		if (key_ctx != NULL) {
			if (EVP_PKEY_verify_init (key_ctx) <= 0) {
				EVP_PKEY_CTX_free (key_ctx);
				continue;
			}
			if (EVP_PKEY_CTX_set_rsa_padding (key_ctx, RSA_PKCS1_PADDING) <= 0) {
				EVP_PKEY_CTX_free (key_ctx);
				continue;
			}
			if (EVP_PKEY_CTX_set_signature_md (key_ctx, EVP_sha256 ()) <= 0) {
				EVP_PKEY_CTX_free (key_ctx);
				continue;
			}
			EVP_DigestInit (sign_ctx, EVP_sha256 ());
			EVP_DigestUpdate (sign_ctx, data, datalen);
			EVP_DigestFinal (sign_ctx, dig, &diglen);

			if (EVP_PKEY_verify (key_ctx, sig, siglen, dig, diglen) == 1) {
				EVP_MD_CTX_destroy (sign_ctx);
				EVP_PKEY_CTX_free (key_ctx);
				return true;
			}

			EVP_PKEY_CTX_free (key_ctx);
		}
	}

	EVP_MD_CTX_destroy (sign_ctx);

	return false;
}
#endif

/**
 * Include an url to configuration
 * @param data
 * @param len
 * @param parser
 * @param err
 * @return
 */
static bool
ucl_include_url (const unsigned char *data, size_t len,
		struct ucl_parser *parser, bool check_signature, UT_string **err)
{

	bool res;
	unsigned char *buf = NULL, *sigbuf = NULL;
	size_t buflen = 0, siglen = 0;
	struct ucl_chunk *chunk;
	char urlbuf[PATH_MAX];

	snprintf (urlbuf, sizeof (urlbuf), "%.*s", (int)len, data);

	if (!ucl_fetch_url (urlbuf, &buf, &buflen, err)) {
		return false;
	}

	if (check_signature) {
#if (defined(HAVE_OPENSSL) && OPENSSL_VERSION_NUMBER >= 0x10000000L)
		/* We need to check signature first */
		snprintf (urlbuf, sizeof (urlbuf), "%.*s.sig", (int)len, data);
		if (!ucl_fetch_file (urlbuf, &sigbuf, &siglen, err)) {
			return false;
		}
		if (!ucl_sig_check (buf, buflen, sigbuf, siglen, parser)) {
			ucl_create_err (err, "cannot verify url %s: %s",
							urlbuf,
							ERR_error_string (ERR_get_error (), NULL));
			munmap (sigbuf, siglen);
			return false;
		}
		munmap (sigbuf, siglen);
#endif
	}

	res = ucl_parser_add_chunk (parser, buf, buflen, err);
	if (res == true) {
		/* Remove chunk from the stack */
		chunk = parser->chunks;
		if (chunk != NULL) {
			parser->chunks = chunk->next;
			UCL_FREE (sizeof (struct ucl_chunk), chunk);
		}
	}
	free (buf);

	return res;
}

/**
 * Include a file to configuration
 * @param data
 * @param len
 * @param parser
 * @param err
 * @return
 */
static bool
ucl_include_file (const unsigned char *data, size_t len,
		struct ucl_parser *parser, bool check_signature, UT_string **err)
{
	bool res;
	struct ucl_chunk *chunk;
	unsigned char *buf = NULL, *sigbuf = NULL;
	size_t buflen, siglen;
	char filebuf[PATH_MAX], realbuf[PATH_MAX];

	snprintf (filebuf, sizeof (filebuf), "%.*s", (int)len, data);
	if (realpath (filebuf, realbuf) == NULL) {
		ucl_create_err (err, "cannot open file %s: %s",
									filebuf,
									strerror (errno));
		return false;
	}

	if (!ucl_fetch_file (realbuf, &buf, &buflen, err)) {
		return false;
	}

	if (check_signature) {
#if (defined(HAVE_OPENSSL) && OPENSSL_VERSION_NUMBER >= 0x10000000L)
		/* We need to check signature first */
		snprintf (filebuf, sizeof (filebuf), "%s.sig", realbuf);
		if (!ucl_fetch_file (filebuf, &sigbuf, &siglen, err)) {
			return false;
		}
		if (!ucl_sig_check (buf, buflen, sigbuf, siglen, parser)) {
			ucl_create_err (err, "cannot verify file %s: %s",
							filebuf,
							ERR_error_string (ERR_get_error (), NULL));
			munmap (sigbuf, siglen);
			return false;
		}
		munmap (sigbuf, siglen);
#endif
	}

	res = ucl_parser_add_chunk (parser, buf, buflen, err);
	if (res == true) {
		/* Remove chunk from the stack */
		chunk = parser->chunks;
		if (chunk != NULL) {
			parser->chunks = chunk->next;
			UCL_FREE (sizeof (struct ucl_chunk), chunk);
		}
	}
	munmap (buf, buflen);

	return res;
}

/**
 * Handle include macro
 * @param data include data
 * @param len length of data
 * @param ud user data
 * @param err error ptr
 * @return
 */
bool
ucl_include_handler (const unsigned char *data, size_t len, void* ud, UT_string **err)
{
	struct ucl_parser *parser = ud;

	if (*data == '/' || *data == '.') {
		/* Try to load a file */
		return ucl_include_file (data, len, parser, false, err);
	}

	return ucl_include_url (data, len, parser, false, err);
}

/**
 * Handle includes macro
 * @param data include data
 * @param len length of data
 * @param ud user data
 * @param err error ptr
 * @return
 */
bool
ucl_includes_handler (const unsigned char *data, size_t len, void* ud, UT_string **err)
{
	struct ucl_parser *parser = ud;

	if (*data == '/' || *data == '.') {
		/* Try to load a file */
		return ucl_include_file (data, len, parser, true, err);
	}

	return ucl_include_url (data, len, parser, true, err);
}

bool
ucl_parser_add_file (struct ucl_parser *parser, const char *filename,
		UT_string **err)
{
	unsigned char *buf;
	size_t len;
	bool ret;

	if (!ucl_fetch_file (filename, &buf, &len, err)) {
		return false;
	}

	ret = ucl_parser_add_chunk (parser, buf, len, err);

	munmap (buf, len);

	return ret;
}

size_t
ucl_strlcpy (char *dst, const char *src, size_t siz)
{
	char *d = dst;
	const char *s = src;
	size_t n = siz;

	/* Copy as many bytes as will fit */
	if (n != 0) {
		while (--n != 0) {
			if ((*d++ = *s++) == '\0') {
				break;
			}
		}
	}

	if (n == 0 && siz != 0) {
		*d = '\0';
	}

	return (s - src - 1);    /* count does not include NUL */
}

size_t
ucl_strlcpy_tolower (char *dst, const char *src, size_t siz)
{
	char *d = dst;
	const char *s = src;
	size_t n = siz;

	/* Copy as many bytes as will fit */
	if (n != 0) {
		while (--n != 0) {
			if ((*d++ = tolower (*s++)) == '\0') {
				break;
			}
		}
	}

	if (n == 0 && siz != 0) {
		*d = '\0';
	}

	return (s - src - 1);    /* count does not include NUL */
}
