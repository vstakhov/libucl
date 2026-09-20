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
 * Parser budgets (ucl_parser_set_limits) and the iterative tree destructor.
 * The destruction tests matter as much as the limit ones: they are what keeps
 * a deeply nested document from exhausting the stack on cleanup, long after
 * the parser has returned successfully.
 */

#include "ucl.h"
#include "ucl_internal.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct ucl_parser *
make_parser(const struct ucl_parser_limits *limits)
{
	struct ucl_parser *p = ucl_parser_new(0);

	assert(p != NULL);

	if (limits != NULL) {
		ucl_parser_set_limits(p, limits);
	}

	return p;
}

static bool
parse(struct ucl_parser *p, const char *input, size_t len)
{
	return ucl_parser_add_chunk(p, (const unsigned char *) input, len);
}

static bool
parse_msgpack(struct ucl_parser *p, const char *input, size_t len)
{
	return ucl_parser_add_chunk_full(p, (const unsigned char *) input, len,
									 ucl_parser_get_default_priority(p),
									 UCL_DUPLICATE_APPEND, UCL_PARSE_MSGPACK);
}

/* `depth` nested arrays: [[[...]]] */
static char *
nested_arrays(unsigned depth, size_t *len)
{
	char *res = malloc(depth * 2 + 1);

	assert(res != NULL);
	memset(res, '[', depth);
	memset(res + depth, ']', depth);
	res[depth * 2] = '\0';
	*len = depth * 2;

	return res;
}

/* `depth` nested objects: {"a":{"a":...1...}} */
static char *
nested_objects(unsigned depth, size_t *len)
{
	size_t sz = (size_t) depth * 6 + 2;
	char *res = malloc(sz);
	size_t i, pos = 0;

	assert(res != NULL);

	for (i = 0; i < depth; i++) {
		memcpy(res + pos, "{\"a\":", 5);
		pos += 5;
	}

	res[pos++] = '1';

	for (i = 0; i < depth; i++) {
		res[pos++] = '}';
	}

	res[pos] = '\0';
	*len = pos;

	return res;
}

/* `depth` fixarrays each holding one element, innermost being int 1 */
static char *
msgpack_nested_arrays(unsigned depth, size_t *len)
{
	char *res = malloc(depth + 1);

	assert(res != NULL);
	memset(res, '\x91', depth);
	res[depth] = '\x01';
	*len = depth + 1;

	return res;
}

/* One array16 holding `n` ints */
static char *
msgpack_flat_array(unsigned n, size_t *len)
{
	char *res = malloc(n + 3);

	assert(res != NULL);
	res[0] = '\xdc';
	res[1] = (char) (n >> 8);
	res[2] = (char) (n & 0xff);
	memset(res + 3, '\x01', n);
	*len = n + 3;

	return res;
}

static void
test_depth(void)
{
	struct ucl_parser *p;
	struct ucl_parser_limits limits;
	ucl_object_t *top;
	char *input;
	size_t len;

	/* Nesting is bounded by default, for arrays and for objects alike */
	p = make_parser(NULL);
	input = nested_arrays(100000, &len);
	assert(parse(p, input, len) == false);
	assert(ucl_parser_get_error_code(p) == UCL_ENESTED);
	free(input);
	ucl_parser_free(p);

	p = make_parser(NULL);
	input = nested_objects(100000, &len);
	assert(parse(p, input, len) == false);
	assert(ucl_parser_get_error_code(p) == UCL_ENESTED);
	free(input);
	ucl_parser_free(p);

	/* Just under the default limit still parses - and still frees */
	p = make_parser(NULL);
	input = nested_arrays(UCL_MAX_NESTING - 1, &len);
	assert(parse(p, input, len) == true);
	top = ucl_parser_get_object(p);
	assert(top != NULL);
	ucl_object_unref(top);
	free(input);
	ucl_parser_free(p);

	/* A custom depth limit overrides the default, in both directions */
	memset(&limits, 0, sizeof(limits));
	limits.max_depth = 4;
	p = make_parser(&limits);
	input = nested_arrays(5, &len);
	assert(parse(p, input, len) == false);
	assert(ucl_parser_get_error_code(p) == UCL_ENESTED);
	free(input);
	ucl_parser_free(p);

	p = make_parser(&limits);
	input = nested_arrays(4, &len);
	assert(parse(p, input, len) == true);
	free(input);
	ucl_parser_free(p);

	/*
	 * Depth is measured per container rather than over the whole document:
	 * a long run of shallow siblings must not accumulate.
	 */
	memset(&limits, 0, sizeof(limits));
	limits.max_depth = 3;
	p = make_parser(&limits);
	assert(parse(p, "{\"a\": [1], \"b\": [2], \"c\": [3], \"d\": [4]}",
				 strlen("{\"a\": [1], \"b\": [2], \"c\": [3], \"d\": [4]}")) == true);
	ucl_parser_free(p);
}

static void
test_nodes_and_alloc(void)
{
	struct ucl_parser *p;
	struct ucl_parser_limits limits;

	/* max_nodes bounds the element count */
	memset(&limits, 0, sizeof(limits));
	limits.max_nodes = 5;
	p = make_parser(&limits);
	assert(parse(p, "[1, 2, 3, 4, 5, 6, 7, 8, 9, 10]",
				 strlen("[1, 2, 3, 4, 5, 6, 7, 8, 9, 10]")) == false);
	assert(ucl_parser_get_error_code(p) == UCL_ELIMIT);
	ucl_parser_free(p);

	/* ... and leaves conforming input alone */
	memset(&limits, 0, sizeof(limits));
	limits.max_nodes = 64;
	p = make_parser(&limits);
	assert(parse(p, "[1, 2, 3]", strlen("[1, 2, 3]")) == true);
	ucl_parser_free(p);

	/* max_alloc bounds the memory a small input can claim */
	memset(&limits, 0, sizeof(limits));
	limits.max_alloc = 512;
	p = make_parser(&limits);
	{
		char buf[8192];
		size_t pos = 0, i;

		buf[pos++] = '[';

		for (i = 0; i < 1000; i++) {
			buf[pos++] = '1';
			buf[pos++] = ',';
		}

		buf[pos - 1] = ']';
		assert(parse(p, buf, pos) == false);
		assert(ucl_parser_get_error_code(p) == UCL_ELIMIT);
	}
	ucl_parser_free(p);

	/*
	 * Duplicate keys build an implicit-array wrapper under
	 * NO_IMPLICIT_ARRAYS; that wrapper is an element too and has to be
	 * charged, or a run of duplicates grows the tree for free.
	 */
	memset(&limits, 0, sizeof(limits));
	limits.max_nodes = 3;
	p = ucl_parser_new(UCL_PARSER_NO_IMPLICIT_ARRAYS);
	assert(p != NULL);
	ucl_parser_set_limits(p, &limits);
	assert(parse(p, "{a = 1; a = 2; a = 3;}",
				 strlen("{a = 1; a = 2; a = 3;}")) == false);
	assert(ucl_parser_get_error_code(p) == UCL_ELIMIT);
	ucl_parser_free(p);
}

static void
test_string_and_key_limits(void)
{
	struct ucl_parser *p;
	struct ucl_parser_limits limits;

	memset(&limits, 0, sizeof(limits));
	limits.max_key_length = 4;

	p = make_parser(&limits);
	assert(parse(p, "{\"abcde\": 1}", strlen("{\"abcde\": 1}")) == false);
	assert(ucl_parser_get_error_code(p) == UCL_ELIMIT);
	ucl_parser_free(p);

	/* A key exactly at the limit is fine */
	p = make_parser(&limits);
	assert(parse(p, "{\"abcd\": 1}", strlen("{\"abcd\": 1}")) == true);
	ucl_parser_free(p);

	memset(&limits, 0, sizeof(limits));
	limits.max_string_length = 4;

	p = make_parser(&limits);
	assert(parse(p, "{\"a\": \"abcde\"}", strlen("{\"a\": \"abcde\"}")) == false);
	assert(ucl_parser_get_error_code(p) == UCL_ELIMIT);
	ucl_parser_free(p);

	p = make_parser(&limits);
	assert(parse(p, "{\"a\": \"abcd\"}", strlen("{\"a\": \"abcd\"}")) == true);
	ucl_parser_free(p);

	/*
	 * Escape-heavy strings are charged for what they allocate, not for what
	 * they decode to: eight source bytes per decoded byte would otherwise
	 * slip past the budget.
	 */
	memset(&limits, 0, sizeof(limits));
	limits.max_alloc = 256;
	p = make_parser(&limits);
	{
		char buf[8192];
		size_t pos = 0, i;

		memcpy(buf, "{\"a\": \"", 7);
		pos = 7;

		for (i = 0; i < 500; i++) {
			memcpy(buf + pos, "\\u0041", 6);
			pos += 6;
		}

		memcpy(buf + pos, "\"}", 2);
		pos += 2;

		assert(parse(p, buf, pos) == false);
		assert(ucl_parser_get_error_code(p) == UCL_ELIMIT);
	}
	ucl_parser_free(p);
}

static void
test_limits_accessors(void)
{
	struct ucl_parser *p;
	struct ucl_parser_limits set, got;

	/* Defaults: depth only */
	p = make_parser(NULL);
	memset(&got, 0xff, sizeof(got));
	ucl_parser_get_limits(p, &got);
	assert(got.max_depth == UCL_MAX_NESTING);
	assert(got.max_nodes == 0);
	assert(got.max_alloc == 0);
	assert(got.max_key_length == 0);
	assert(got.max_string_length == 0);

	/* Round trip */
	set.max_depth = 11;
	set.max_nodes = 22;
	set.max_alloc = 33;
	set.max_key_length = 44;
	set.max_string_length = 55;
	ucl_parser_set_limits(p, &set);
	memset(&got, 0, sizeof(got));
	ucl_parser_get_limits(p, &got);
	assert(memcmp(&set, &got, sizeof(set)) == 0);

	/* NULL restores the defaults */
	ucl_parser_set_limits(p, NULL);
	ucl_parser_get_limits(p, &got);
	assert(got.max_depth == UCL_MAX_NESTING);
	assert(got.max_nodes == 0);
	ucl_parser_free(p);

	/* Zero means unlimited, including for the depth */
	memset(&set, 0, sizeof(set));
	p = make_parser(&set);
	{
		size_t len;
		char *input = nested_arrays(5000, &len);

		assert(parse(p, input, len) == true);
		free(input);
	}
	ucl_parser_free(p);

	/* NULL parser must not crash either accessor */
	ucl_parser_set_limits(NULL, &set);
	ucl_parser_get_limits(NULL, &got);
}

static void
test_msgpack_limits(void)
{
	struct ucl_parser *p;
	struct ucl_parser_limits limits;
	char *input;
	size_t len;

	/* Nesting is bounded by default - 100k levels in 100k bytes */
	p = make_parser(NULL);
	input = msgpack_nested_arrays(100000, &len);
	assert(parse_msgpack(p, input, len) == false);
	assert(ucl_parser_get_error_code(p) == UCL_ENESTED);
	free(input);
	ucl_parser_free(p);

	/* A custom depth limit applies */
	memset(&limits, 0, sizeof(limits));
	limits.max_depth = 8;
	p = make_parser(&limits);
	input = msgpack_nested_arrays(9, &len);
	assert(parse_msgpack(p, input, len) == false);
	assert(ucl_parser_get_error_code(p) == UCL_ENESTED);
	free(input);
	ucl_parser_free(p);

	/* Within the limit it parses, and the tree frees */
	p = make_parser(&limits);
	input = msgpack_nested_arrays(8, &len);
	assert(parse_msgpack(p, input, len) == true);
	free(input);
	ucl_parser_free(p);

	/* max_nodes */
	memset(&limits, 0, sizeof(limits));
	limits.max_nodes = 16;
	p = make_parser(&limits);
	input = msgpack_flat_array(1000, &len);
	assert(parse_msgpack(p, input, len) == false);
	assert(ucl_parser_get_error_code(p) == UCL_ELIMIT);
	free(input);
	ucl_parser_free(p);

	/* max_alloc */
	memset(&limits, 0, sizeof(limits));
	limits.max_alloc = 512;
	p = make_parser(&limits);
	input = msgpack_flat_array(1000, &len);
	assert(parse_msgpack(p, input, len) == false);
	assert(ucl_parser_get_error_code(p) == UCL_ELIMIT);
	free(input);
	ucl_parser_free(p);

	/* max_string_length: a fixarray holding one 5-byte fixstr */
	memset(&limits, 0, sizeof(limits));
	limits.max_string_length = 4;
	p = make_parser(&limits);
	assert(parse_msgpack(p, "\x91\xa5" "abcde", 7) == false);
	assert(ucl_parser_get_error_code(p) == UCL_ELIMIT);
	ucl_parser_free(p);

	p = make_parser(&limits);
	assert(parse_msgpack(p, "\x91\xa4" "abcd", 6) == true);
	ucl_parser_free(p);

	/* max_key_length: fixmap of one 5-byte key */
	memset(&limits, 0, sizeof(limits));
	limits.max_key_length = 4;
	p = make_parser(&limits);
	assert(parse_msgpack(p, "\x81\xa5" "abcde" "\x01", 8) == false);
	assert(ucl_parser_get_error_code(p) == UCL_ELIMIT);
	ucl_parser_free(p);

	/* Conforming input is unaffected */
	memset(&limits, 0, sizeof(limits));
	limits.max_depth = 16;
	limits.max_nodes = 1024;
	limits.max_alloc = 1024 * 1024;
	limits.max_key_length = 64;
	limits.max_string_length = 1024;
	p = make_parser(&limits);
	assert(parse_msgpack(p, "\x81\xa1" "a" "\x93\x01\x02\x03", 7) == true);
	{
		ucl_object_t *top = ucl_parser_get_object(p);
		const ucl_object_t *arr;

		assert(top != NULL);
		arr = ucl_object_lookup(top, "a");
		assert(arr != NULL);
		assert(ucl_object_type(arr) == UCL_ARRAY);
		assert(ucl_array_size(arr) == 3);
		ucl_object_unref(top);
	}
	ucl_parser_free(p);
}

/*
 * The destructor walks an explicit worklist, so a tree far deeper than the
 * C stack could survive must still be released without recursing.
 */
static void
test_deep_destruction(void)
{
	ucl_object_t *top, *cur;
	unsigned i;

	top = ucl_object_typed_new(UCL_ARRAY);
	assert(top != NULL);
	cur = top;

	for (i = 0; i < 200000; i++) {
		ucl_object_t *next = ucl_object_typed_new(UCL_ARRAY);

		assert(next != NULL);
		assert(ucl_array_append(cur, next));
		cur = next;
	}

	ucl_object_unref(top);
}

/* A subtree that somebody else still references must outlive its container */
static void
test_shared_subtree(void)
{
	struct ucl_parser *p = make_parser(NULL);
	ucl_object_t *top, *kept;
	const ucl_object_t *inner, *arr;

	assert(parse(p, "{\"a\": {\"b\": [1, 2, 3]}}",
				 strlen("{\"a\": {\"b\": [1, 2, 3]}}")) == true);

	top = ucl_parser_get_object(p);
	assert(top != NULL);

	inner = ucl_object_lookup(top, "a");
	assert(inner != NULL);
	kept = ucl_object_ref(inner);

	/*
	 * The tree has to actually die for this to test anything:
	 * ucl_parser_get_object() hands out a second reference, so releasing
	 * `top` alone leaves the parser owning everything.
	 */
	ucl_object_unref(top);
	ucl_parser_free(p);

	arr = ucl_object_lookup(kept, "b");
	assert(arr != NULL);
	assert(ucl_object_type(arr) == UCL_ARRAY);
	assert(ucl_array_size(arr) == 3);
	assert(ucl_object_toint(ucl_array_find_index(arr, 0)) == 1);
	assert(ucl_object_toint(ucl_array_find_index(arr, 2)) == 3);

	ucl_object_unref(kept);
}

/* Implicit array chains hang off `next`, which the worklist reuses */
static void
test_implicit_array_chain(void)
{
	struct ucl_parser *p = make_parser(NULL);
	ucl_object_t *top;
	const ucl_object_t *elt;

	assert(parse(p, "a = 1; a = 2; a = 3; a = 4;",
				 strlen("a = 1; a = 2; a = 3; a = 4;")) == true);

	top = ucl_parser_get_object(p);
	assert(top != NULL);

	elt = ucl_object_lookup(top, "a");
	assert(elt != NULL);
	assert(ucl_object_toint(elt) == 1);
	assert(elt->next != NULL);

	ucl_object_unref(top);
	ucl_parser_free(p);
}

int main(int argc, char **argv)
{
	(void) argc;
	(void) argv;

	test_depth();
	test_nodes_and_alloc();
	test_string_and_key_limits();
	test_limits_accessors();
	test_msgpack_limits();
	test_deep_destruction();
	test_shared_subtree();
	test_implicit_array_chain();

	printf("All limits tests passed\n");

	return 0;
}
