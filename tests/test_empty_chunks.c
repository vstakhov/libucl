#include "ucl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(cond, message) \
	do { \
		if (!(cond)) { \
			fprintf(stderr, "%s: %s\n", __func__, message); \
			ok = false; \
			goto done; \
		} \
	} while (0)

static bool
add_empty(struct ucl_parser *parser)
{
	return ucl_parser_add_chunk(parser, (const unsigned char *) "", 0);
}

static bool
same_document(const ucl_object_t *a, const ucl_object_t *b)
{
	size_t alen = 0, blen = 0;
	unsigned char *abytes, *bbytes;
	bool equal;

	if (a == NULL || b == NULL) {
		return false;
	}
	/* Compare length-delimited contents, including CSEXP binary strings. */
	abytes = ucl_object_emit_len(a, UCL_EMIT_MSGPACK, &alen);
	bbytes = ucl_object_emit_len(b, UCL_EMIT_MSGPACK, &blen);
	equal = abytes != NULL && bbytes != NULL && alen == blen &&
			memcmp(abytes, bbytes, alen) == 0;
	free(abytes);
	free(bbytes);
	return equal;
}

/* Empty prefixes must not determine the root type, contents, or priority. */
static bool
check_document(const unsigned char *data, size_t len, enum ucl_parse_type type)
{
	struct ucl_parser *parser = ucl_parser_new(0);
	struct ucl_parser *control = ucl_parser_new(0);
	ucl_object_t *placeholder = NULL, *result = NULL, *expected = NULL;
	ucl_object_t *after_empty = NULL;
	bool ok = true;

	CHECK(parser != NULL && control != NULL, "cannot create parsers");
	CHECK(add_empty(parser) && add_empty(parser), "empty prefix failed");
	placeholder = ucl_parser_get_object(parser);
	CHECK(placeholder != NULL && ucl_object_type(placeholder) == UCL_OBJECT &&
			  placeholder->len == 0, "empty input must expose an empty object");
	CHECK(ucl_parser_add_chunk_full(control, data, len, 7,
			  UCL_DUPLICATE_APPEND, type), "control parse failed");
	CHECK(ucl_parser_add_chunk_full(parser, data, len, 7,
			  UCL_DUPLICATE_APPEND, type), "parse after empty prefix failed");
	result = ucl_parser_get_object(parser);
	expected = ucl_parser_get_object(control);
	CHECK(same_document(result, expected),
		  "empty prefix changed the document");
	CHECK(ucl_object_get_priority(result) == ucl_object_get_priority(expected),
		  "empty prefix changed the root priority");
	CHECK(ucl_object_type(placeholder) == UCL_OBJECT && placeholder->len == 0,
		  "a retained placeholder must remain valid after replacement");
	CHECK(add_empty(parser), "empty suffix failed");
	after_empty = ucl_parser_get_object(parser);
	CHECK(after_empty == result, "empty suffix replaced an existing root");

done:
	ucl_object_unref(after_empty);
	ucl_object_unref(expected);
	ucl_object_unref(result);
	ucl_object_unref(placeholder);
	ucl_parser_free(control);
	ucl_parser_free(parser);
	return ok;
}

static bool
check_continuation(const char *first, const char *last, const char *whole)
{
	struct ucl_parser *parser = ucl_parser_new(0);
	struct ucl_parser *control = ucl_parser_new(0);
	ucl_object_t *result = NULL, *expected = NULL;
	bool ok = true;

	CHECK(parser != NULL && control != NULL, "cannot create parsers");
	CHECK(ucl_parser_add_string(parser, first, strlen(first)), "first chunk failed");
	CHECK(add_empty(parser), "middle empty chunk failed");
	CHECK(ucl_parser_add_string(parser, last, strlen(last)), "last chunk failed");
	CHECK(ucl_parser_add_string(control, whole, strlen(whole)), "control parse failed");
	result = ucl_parser_get_object(parser);
	expected = ucl_parser_get_object(control);
	CHECK(same_document(result, expected),
		  "middle empty chunk changed the document");

done:
	ucl_object_unref(expected);
	ucl_object_unref(result);
	ucl_parser_free(control);
	ucl_parser_free(parser);
	return ok;
}

static bool
check_root_limit(const char *input)
{
	struct ucl_parser *parser = ucl_parser_new(0);
	struct ucl_parser_limits limits = {0};
	bool ok = true;

	CHECK(parser != NULL, "cannot create parser");
	limits.max_nodes = 1;
	ucl_parser_set_limits(parser, &limits);
	CHECK(add_empty(parser), "empty prefix failed");
	CHECK(!ucl_parser_add_string(parser, input, strlen(input)),
		  "the root must count towards max_nodes");
	CHECK(ucl_parser_get_error_code(parser) == UCL_ELIMIT,
		  "expected the node limit error");

done:
	ucl_parser_free(parser);
	return ok;
}

static bool
check_finished_root(void)
{
	struct ucl_parser *parser = ucl_parser_new(0);
	ucl_object_t *before = NULL, *after = NULL;
	bool ok = true;

	CHECK(parser != NULL, "cannot create parser");
	CHECK(ucl_parser_add_string(parser, "{}", 2), "initial object failed");
	before = ucl_parser_get_object(parser);
	CHECK(add_empty(parser), "empty suffix failed");
	CHECK(!ucl_parser_insert_chunk(parser, (const unsigned char *) "{a=1}", 5),
		  "empty chunk must not reopen a finished root");
	after = ucl_parser_get_object(parser);
	CHECK(before != NULL && after == before && after->len == 0,
		  "finished root changed");

done:
	ucl_object_unref(after);
	ucl_object_unref(before);
	ucl_parser_free(parser);
	return ok;
}

static bool
check_keyless_chunk(const char *base, const char *chunk)
{
	/*
	 * A chunk that starts with [ inside an object frame has no key to attach
	 * its value to. Reusing parser->cur_obj there gave away the previous key's
	 * value: an inserted [5 overwrote it silently, and [{ or [[ reused its
	 * scalar storage as the new container's, crashing in ucl_hash_destroy()
	 * when the tree was freed.
	 */
	struct ucl_parser *parser = ucl_parser_new(0);
	bool ok = true;

	CHECK(parser != NULL, "cannot create parser");
	CHECK(ucl_parser_add_string(parser, base, 0), "base object failed");
	CHECK(!ucl_parser_insert_chunk(parser, (const unsigned char *) chunk,
								   strlen(chunk)),
		  "a keyless array chunk must be rejected in an object frame");

done:
	ucl_parser_free(parser);
	return ok;
}

static bool
check_null_retyped_chunk(const char *base)
{
	/*
	 * Parsing null over the stale value first was enough to slip past a check
	 * on the type alone: it sets type = UCL_NULL and leaves value.* in place.
	 */
	struct ucl_parser *parser = ucl_parser_new(0);
	bool ok = true;

	CHECK(parser != NULL, "cannot create parser");
	CHECK(ucl_parser_add_string(parser, base, 0), "base object failed");
	CHECK(!ucl_parser_insert_chunk(parser, (const unsigned char *) "[null", 5),
		  "keyless [null must be rejected");
	CHECK(!ucl_parser_insert_chunk(parser, (const unsigned char *) "[{}", 3),
		  "keyless [{} must be rejected");

done:
	ucl_parser_free(parser);
	return ok;
}

int
main(void)
{
	static const struct {
		const unsigned char *data;
		size_t len;
		enum ucl_parse_type type;
	} cases[] = {
#define DOCUMENT(data, type) {(const unsigned char *) data, sizeof(data) - 1, type}
		DOCUMENT("[1]", UCL_PARSE_UCL),
		DOCUMENT("[]", UCL_PARSE_UCL),
		DOCUMENT("[1, {a=2}]", UCL_PARSE_UCL),
		DOCUMENT("{answer=42}", UCL_PARSE_UCL),
		DOCUMENT("answer=42;", UCL_PARSE_UCL),
		DOCUMENT("{}", UCL_PARSE_UCL),
		DOCUMENT("# comment\n[1]", UCL_PARSE_UCL),
		DOCUMENT("\x91\x01", UCL_PARSE_MSGPACK),
		DOCUMENT("\x81\xa1" "a\x01", UCL_PARSE_MSGPACK),
		DOCUMENT("\x81\x01", UCL_PARSE_CBOR),
		DOCUMENT("\x80", UCL_PARSE_CBOR),
		DOCUMENT("\xa0", UCL_PARSE_CBOR),
		DOCUMENT("\xa1\x61" "a\x01", UCL_PARSE_CBOR),
		DOCUMENT("(2:42)", UCL_PARSE_CSEXP),
		DOCUMENT("()", UCL_PARSE_CSEXP),
#undef DOCUMENT
	};
	bool ok = true;

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		if (!check_document(cases[i].data, cases[i].len, cases[i].type)) {
			fprintf(stderr, "document case %zu failed\n", i);
			ok = false;
		}
	}
	ok = check_continuation("a=1;", "b=2;", "a=1; b=2;") && ok;
	ok = check_root_limit("answer = 42;") && ok;
	ok = check_root_limit("[42]") && ok;
	ok = check_finished_root() && ok;
	ok = check_keyless_chunk("a = 1", "[{}") && ok;
	ok = check_keyless_chunk("a = 1", "[[") && ok;
	ok = check_keyless_chunk("a = 1", "[5") && ok;
	ok = check_keyless_chunk("a = \"str\"", "[{}") && ok;
	ok = check_null_retyped_chunk("a = 1") && ok;
	ok = check_null_retyped_chunk("a = \"str\"") && ok;
	return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
