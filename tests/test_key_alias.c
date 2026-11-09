/* Copyright (c) 2026, Vsevolod Stakhov
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

/*
 * Regression tests for keys that an object does not own outliving their
 * buffer. With UCL_PARSER_NO_IMPLICIT_ARRAYS, the explicit array of a
 * repeated key used to borrow the key of its first value, and
 * ucl_object_copy() kept such a borrowed key pointing at the source. Once the
 * source, or that first value, was freed, a lookup read freed memory.
 * AddressSanitizer reports the read; the checks below catch a stale key
 * without it only by luck.
 */

#include "ucl.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
check_multivalue(const ucl_object_t *top, const char *key, unsigned int len)
{
	const ucl_object_t *arr = ucl_object_lookup(top, key);

	assert(arr != NULL);
	assert(ucl_object_type(arr) == UCL_ARRAY);
	assert(arr->len == len);
	assert(arr->keylen == strlen(key));
	assert(memcmp(arr->key, key, arr->keylen) == 0);
}

/* A copy of a parsed tree must not depend on the tree it was copied from */
static void
test_copy_outlives_source(void)
{
	static const char doc[] = "a = 1; a = 2; b = 3;";
	struct ucl_parser *p = ucl_parser_new(UCL_PARSER_SAFE_FLAGS);
	ucl_object_t *top, *cp;

	assert(p != NULL);
	assert(ucl_parser_add_string(p, doc, sizeof(doc) - 1));
	top = ucl_parser_get_object(p);
	assert(top != NULL);
	ucl_parser_free(p);

	cp = ucl_object_copy(top);
	assert(cp != NULL);
	ucl_object_unref(top);

	check_multivalue(cp, "a", 2);
	assert(ucl_object_lookup(cp, "b") != NULL);
	ucl_object_unref(cp);
}

/* The explicit array must keep its key once its first value is gone */
static void
test_pop_first_value(void)
{
	static const char doc[] = "a = 1; a = 2; b = 3;";
	struct ucl_parser *p = ucl_parser_new(UCL_PARSER_SAFE_FLAGS);
	ucl_object_t *top, *arr, *first;

	assert(p != NULL);
	assert(ucl_parser_add_string(p, doc, sizeof(doc) - 1));
	top = ucl_parser_get_object(p);
	assert(top != NULL);
	ucl_parser_free(p);

	arr = (ucl_object_t *) ucl_object_lookup(top, "a");
	assert(arr != NULL);
	first = ucl_array_pop_first(arr);
	assert(first != NULL);
	ucl_object_unref(first);

	check_multivalue(top, "a", 1);
	assert(ucl_object_lookup(top, "b") != NULL);
	ucl_object_unref(top);
}

/*
 * .inherit copies the parent into the child, explicit arrays included. A
 * value of higher priority then replaces the array of the parent, freeing the
 * value whose key the copied array still used, and adding to the child
 * looked that key up.
 */
static void
test_inherit_then_replace(void)
{
	static const char doc[] =
		"base { defaults { key = 1; key = 2; } }\n"
		"child { .inherit \"base\" }\n"
		".priority 3\n"
		"base { defaults { key = 3; } }\n"
		"child { defaults { key = 4; other = 5; } }\n";
	struct ucl_parser *p = ucl_parser_new(UCL_PARSER_NO_IMPLICIT_ARRAYS);
	const ucl_object_t *elt;
	ucl_object_t *top;

	assert(p != NULL);
	assert(ucl_parser_add_chunk_full(p, (const unsigned char *) doc,
									 sizeof(doc) - 1, 0,
									 UCL_DUPLICATE_MERGE, UCL_PARSE_UCL));
	top = ucl_parser_get_object(p);
	assert(top != NULL);
	ucl_parser_free(p);

	/* Both priority 3 values replace the arrays of priority 0 */
	elt = ucl_object_lookup_path(top, "base.defaults.key");
	assert(elt != NULL && ucl_object_toint(elt) == 3);
	elt = ucl_object_lookup_path(top, "child.defaults.key");
	assert(elt != NULL && ucl_object_toint(elt) == 4);
	elt = ucl_object_lookup_path(top, "child.defaults.other");
	assert(elt != NULL && ucl_object_toint(elt) == 5);
	ucl_object_unref(top);
}

/*
 * With UCL_PARSER_ZEROCOPY the keys point into the input, which the caller
 * may release along with the parser: a copy has to own its keys.
 */
static void
test_copy_zerocopy_keys(void)
{
	static const char doc[] = "alpha = 1; beta = 2;";
	const size_t len = sizeof(doc) - 1;
	unsigned char *buf = malloc(len);
	struct ucl_parser *p = ucl_parser_new(UCL_PARSER_ZEROCOPY);
	ucl_object_t *top, *cp;
	const ucl_object_t *elt;

	assert(buf != NULL && p != NULL);
	memcpy(buf, doc, len);
	assert(ucl_parser_add_chunk(p, buf, len));
	top = ucl_parser_get_object(p);
	assert(top != NULL);

	cp = ucl_object_copy(top);
	assert(cp != NULL);
	ucl_object_unref(top);
	ucl_parser_free(p);
	free(buf);

	elt = ucl_object_lookup(cp, "beta");
	assert(elt != NULL);
	assert(ucl_object_toint(elt) == 2);
	assert(ucl_object_lookup(cp, "alpha") != NULL);
	ucl_object_unref(cp);
}

int main(void)
{
	test_copy_outlives_source();
	test_pop_first_value();
	test_inherit_then_replace();
	test_copy_zerocopy_keys();

	return 0;
}
