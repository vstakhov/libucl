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
check_keyed_chunk(const char *base, const char *chunk)
{
	/*
	 * The mirror image of a keyless chunk: a chunk that starts with a key or
	 * with { needs an object frame. In an array frame ucl_parse_key() passed
	 * the array's storage to ucl_hash_insert() as if it were a hash, and {}
	 * marked the array as braced, so that its own ] no longer closed it.
	 */
	struct ucl_parser *parser = ucl_parser_new(0);
	struct ucl_parser *control = ucl_parser_new(0);
	ucl_object_t *result = NULL, *expected = NULL;
	char whole[256];
	bool ok = true;

	CHECK(parser != NULL && control != NULL, "cannot create parsers");
	CHECK(!ucl_parser_add_string(parser, base, 0),
		  "the base must leave an array frame open");
	CHECK(!ucl_parser_insert_chunk(parser, (const unsigned char *) chunk,
								   strlen(chunk)),
		  "a keyed chunk must be rejected in an array frame");
	CHECK(ucl_parser_get_error_code(parser) == UCL_ESYNTAX,
		  "expected a syntax error");
	/* The rejected chunk must leave the array frame as it was */
	CHECK(ucl_parser_add_string(parser, "3]; z = 4", 0),
		  "closing the array failed");
	snprintf(whole, sizeof(whole), "%s3]; z = 4", base);
	CHECK(ucl_parser_add_string(control, whole, 0), "control parse failed");
	result = ucl_parser_get_object(parser);
	expected = ucl_parser_get_object(control);
	CHECK(same_document(result, expected),
		  "a rejected chunk changed the document");

done:
	ucl_object_unref(expected);
	ucl_object_unref(result);
	ucl_parser_free(control);
	ucl_parser_free(parser);
	return ok;
}

static bool
check_keyed_chunk_error(const char *chunk)
{
	/*
	 * The first error message is kept, so after the unmatched brace that the
	 * base leaves behind, a rejected keyed chunk shows only in the error code.
	 * With that error cleared first, the message names the problem as well.
	 */
	struct ucl_parser *parser = ucl_parser_new(0);
	const char *err;
	bool ok = true;

	CHECK(parser != NULL, "cannot create parser");
	CHECK(!ucl_parser_add_string(parser, "a = [1, ", 0),
		  "the base must leave an array frame open");
	CHECK(ucl_parser_get_error_code(parser) == UCL_EUNPAIRED,
		  "expected an unmatched brace");
	ucl_parser_clear_error(parser);
	CHECK(!ucl_parser_insert_chunk(parser, (const unsigned char *) chunk,
								   strlen(chunk)),
		  "a keyed chunk must be rejected in an array frame");
	err = ucl_parser_get_error(parser);
	CHECK(ucl_parser_get_error_code(parser) == UCL_ESYNTAX && err != NULL &&
			  strstr(err, "key outside of an object") != NULL,
		  "expected a key outside of an object error");

done:
	ucl_parser_free(parser);
	return ok;
}

static bool
check_resumed_chunk(const char *base, const char *chunk, const char *rest,
					const char *whole)
{
	/*
	 * An inserted chunk can close the frame it was inserted into: a keyless
	 * [2] closes the array, as the [ opens no container of its own. It can
	 * also leave new frames open. ucl_parser_insert_chunk() restored the state
	 * saved on the old frame anyway, so after an array the next chunk was
	 * parsed as a value into the stale parser->cur_obj, overwriting a value
	 * already in the tree and leaking a container it replaced.
	 * With whole == NULL the rest of the document must be rejected.
	 */
	struct ucl_parser *parser = ucl_parser_new(0);
	struct ucl_parser *control = ucl_parser_new(0);
	ucl_object_t *result = NULL, *expected = NULL;
	bool ok = true;

	CHECK(parser != NULL && control != NULL, "cannot create parsers");
	CHECK(!ucl_parser_add_string(parser, base, 0),
		  "the base must leave a frame open");
	ucl_parser_insert_chunk(parser, (const unsigned char *) chunk,
							strlen(chunk));
	if (whole == NULL) {
		CHECK(!ucl_parser_add_string(parser, rest, 0),
			  "the rest must not be parsed into a stale value");
	}
	else {
		CHECK(ucl_parser_add_string(parser, rest, 0), "the rest failed");
		CHECK(ucl_parser_add_string(control, whole, 0),
			  "control parse failed");
		result = ucl_parser_get_object(parser);
		expected = ucl_parser_get_object(control);
		CHECK(same_document(result, expected),
			  "the rest was not parsed where the inserted chunk ended");
	}

done:
	ucl_object_unref(expected);
	ucl_object_unref(result);
	ucl_parser_free(control);
	ucl_parser_free(parser);
	return ok;
}

static bool
check_failed_insert(const char *base, const char *chunk)
{
	/*
	 * ucl_parser_add_chunk() refuses to continue a failed parse, but
	 * ucl_parser_insert_chunk() reset UCL_STATE_ERROR and went on adding to
	 * the half-built tree.
	 */
	struct ucl_parser *parser = ucl_parser_new(0);
	bool ok = true;

	CHECK(parser != NULL, "cannot create parser");
	CHECK(!ucl_parser_add_string(parser, base, 0), "the base must fail");
	CHECK(!ucl_parser_insert_chunk(parser, (const unsigned char *) chunk,
								   strlen(chunk)),
		  "a failed parser must not take an inserted chunk");

done:
	ucl_parser_free(parser);
	return ok;
}

static bool
check_null_retyped_chunk(const char *base)
{
	/*
	 * Parsing null over the stale value sets type = UCL_NULL and leaves value.*
	 * in place, so [null followed by [{} slipped past a check on the type alone.
	 * Both chunks are now rejected before a value is parsed; the UCL_NULL
	 * storage check in ucl_parser_add_container() is a backstop that this
	 * sequence no longer reaches.
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

static bool
check_keyless_include(const char *base, const char *included)
{
	/*
	 * .include feeds a file through the same path as an inserted chunk, so a
	 * single malformed included file was enough to reach the stale value, or
	 * the root object itself when the include is the first statement.
	 */
	const char *dir = getenv("TEST_OUT_DIR");
	struct ucl_parser *parser = ucl_parser_new(0);
	char path[1024], conf[256];
	FILE *out;
	bool ok = true;

	snprintf(path, sizeof(path), "%s/empty_chunks_keyless.inc",
			 dir != NULL ? dir : ".");
	snprintf(conf, sizeof(conf), "%s\n.include \"$KEYLESS_INC\"\n", base);
	CHECK(parser != NULL, "cannot create parser");
	out = fopen(path, "w");
	CHECK(out != NULL, "cannot create the included file");
	fputs(included, out);
	CHECK(fclose(out) == 0, "cannot write the included file");
	ucl_parser_register_variable(parser, "KEYLESS_INC", path);
	CHECK(!ucl_parser_add_string(parser, conf, 0),
		  "a keyless array include must be rejected in an object frame");
	CHECK(ucl_parser_get_error_code(parser) == UCL_ESYNTAX,
		  "expected a syntax error from the included file");

done:
	remove(path);
	ucl_parser_free(parser);
	return ok;
}

static bool
check_closing_include(const char *base, const char *macro, const char *rest,
					  int code)
{
	/*
	 * .include restores the state it was called in, so an included } that
	 * closed an object inside an array left the parser expecting a key with
	 * the array on top of the stack: the next key in the including file went
	 * to ucl_hash_insert() with the array's storage.
	 * A prefixed include pushes a frame of its own and pops it after the
	 * included file. That frame copied the braces of the enclosing one, so an
	 * included } popped and freed it first, and the include then popped it
	 * again.
	 */
	const char *dir = getenv("TEST_OUT_DIR");
	struct ucl_parser *parser = ucl_parser_new(0);
	char path[1024], conf[256];
	FILE *out;
	bool ok = true;

	snprintf(path, sizeof(path), "%s/empty_chunks_closing.inc",
			 dir != NULL ? dir : ".");
	snprintf(conf, sizeof(conf), "%s\n%s \"$CLOSING_INC\"\n%s\n",
			 base, macro, rest);
	CHECK(parser != NULL, "cannot create parser");
	out = fopen(path, "w");
	CHECK(out != NULL, "cannot create the included file");
	fputs("}", out);
	CHECK(fclose(out) == 0, "cannot write the included file");
	ucl_parser_register_variable(parser, "CLOSING_INC", path);
	CHECK(!ucl_parser_add_string(parser, conf, 0),
		  "an included } must not close the frame of the include");
	CHECK(ucl_parser_get_error_code(parser) == code,
		  "unexpected error code");

done:
	remove(path);
	ucl_parser_free(parser);
	return ok;
}

static bool
write_file(const char *path, const char *content)
{
	FILE *out = fopen(path, "w");

	if (out == NULL) {
		return false;
	}
	fputs(content, out);
	return fclose(out) == 0;
}

static bool
check_failed_include(const char *conf, const char *outer, const char *inner,
					 const char *failing)
{
	/*
	 * ucl_include_file_single() swaps in FILENAME, CURDIR and the current file
	 * for the included file, and put the old ones back only if the include
	 * succeeded: a failed include leaked them, at every level of a nested
	 * include. The current file must still name the file that failed.
	 */
	const char *dir = getenv("TEST_OUT_DIR");
	struct ucl_parser *parser = ucl_parser_new(0);
	char outer_path[1024], inner_path[1024];
	const char *cur;
	bool ok = true;

	snprintf(outer_path, sizeof(outer_path), "%s/empty_chunks_outer.inc",
			 dir != NULL ? dir : ".");
	snprintf(inner_path, sizeof(inner_path), "%s/empty_chunks_inner.inc",
			 dir != NULL ? dir : ".");
	CHECK(parser != NULL, "cannot create parser");
	CHECK(write_file(outer_path, outer) && write_file(inner_path, inner),
		  "cannot write the included files");
	ucl_parser_register_variable(parser, "OUTER_INC", outer_path);
	ucl_parser_register_variable(parser, "INNER_INC", inner_path);
	CHECK(!ucl_parser_add_string(parser, conf, 0), "the include must fail");
	cur = ucl_parser_get_cur_file(parser);
	CHECK(cur != NULL && strstr(cur, failing) != NULL,
		  "the current file must name the file that failed");

done:
	remove(outer_path);
	remove(inner_path);
	ucl_parser_free(parser);
	return ok;
}

static bool
check_unadded_chunk(void)
{
	/*
	 * ucl_parser_add_chunk_full() can fail before it adds the chunk, as for
	 * data == NULL with a length. ucl_parser_insert_chunk() removed the head
	 * of parser->chunks anyway, which is the chunk of the caller, and the next
	 * insert read parser->chunks through NULL.
	 */
	struct ucl_parser *parser = ucl_parser_new(0);
	struct ucl_parser *control = ucl_parser_new(0);
	ucl_object_t *result = NULL, *expected = NULL;
	bool ok = true;

	CHECK(parser != NULL && control != NULL, "cannot create parsers");
	CHECK(ucl_parser_add_string(parser, "a = 1", 0), "base object failed");
	CHECK(!ucl_parser_insert_chunk(parser, NULL, 5),
		  "a NULL chunk must be rejected");
	CHECK(ucl_parser_insert_chunk(parser, (const unsigned char *) "b = 2", 5),
		  "an insert after a rejected chunk failed");
	CHECK(ucl_parser_add_string(control, "a = 1; b = 2", 0),
		  "control parse failed");
	result = ucl_parser_get_object(parser);
	expected = ucl_parser_get_object(control);
	CHECK(same_document(result, expected),
		  "a rejected chunk changed the document");

done:
	ucl_object_unref(expected);
	ucl_object_unref(result);
	ucl_parser_free(control);
	ucl_parser_free(parser);
	return ok;
}

struct insert_macro {
	struct ucl_parser *parser;
	bool inserted;
};

static bool
insert_macro_handler(const unsigned char *data, size_t len,
					 const ucl_object_t *arguments, void *ud)
{
	struct insert_macro *m = ud;

	/* Go on whatever the insert returns, so the outer chunk is resumed */
	m->inserted = ucl_parser_insert_chunk(m->parser, data, len);

	return true;
}

static bool
reject_handler(struct ucl_parser *parser, const unsigned char *source,
			   size_t source_len, unsigned char **destination,
			   size_t *dest_len, void *user_data)
{
	/* As a failed decryption or signature check would */
	return false;
}

static bool
check_macro_insert(const char *conf, bool inserted, const char *whole)
{
	/*
	 * The .insert macro splices its value into the document with
	 * ucl_parser_insert_chunk(), and the outer chunk resumes after it.
	 * A value starting with XX is refused by a special handler, and $INC
	 * names a file that fails with an unmatched brace.
	 * With whole == NULL the document must be rejected.
	 */
	const char *dir = getenv("TEST_OUT_DIR");
	struct ucl_parser *parser = ucl_parser_new(0);
	struct ucl_parser *control = ucl_parser_new(0);
	struct ucl_parser_special_handler rejecting = {
		.magic = (const unsigned char *) "XX",
		.magic_len = 2,
		.handler = reject_handler,
	};
	struct insert_macro m = {parser, !inserted};
	ucl_object_t *result = NULL, *expected = NULL;
	char path[1024];
	bool ok = true;

	snprintf(path, sizeof(path), "%s/empty_chunks_insert.inc",
			 dir != NULL ? dir : ".");
	CHECK(parser != NULL && control != NULL, "cannot create parsers");
	CHECK(write_file(path, "x {"), "cannot write the included file");
	ucl_parser_register_variable(parser, "INC", path);
	CHECK(ucl_parser_register_macro(parser, "insert", insert_macro_handler, &m),
		  "cannot register the macro");
	ucl_parser_add_special_handler(parser, &rejecting);
	if (whole == NULL) {
		CHECK(!ucl_parser_add_string(parser, conf, 0),
			  "the document must be rejected");
	}
	else {
		CHECK(ucl_parser_add_string(parser, conf, 0), "the document failed");
		CHECK(ucl_parser_add_string(control, whole, 0),
			  "control parse failed");
		result = ucl_parser_get_object(parser);
		expected = ucl_parser_get_object(control);
		CHECK(same_document(result, expected),
			  "the outer chunk did not resume after the insert");
	}
	CHECK(m.inserted == inserted, "unexpected result of the insert");

done:
	remove(path);
	ucl_object_unref(expected);
	ucl_object_unref(result);
	ucl_parser_free(control);
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
	/* Nothing parsed yet: the stale value is the root object */
	ok = check_keyless_chunk(" ", "[5") && ok;
	ok = check_keyed_chunk("a = [1, ", "b = 1") && ok;
	ok = check_keyed_chunk("a = [1, ", "{b = 1}") && ok;
	ok = check_keyed_chunk("a = [1, ", "{}") && ok;
	ok = check_keyed_chunk("a = [1, ", ".priority 1") && ok;
	/* The root itself is the array frame */
	ok = check_keyed_chunk("[1, ", "b = 1") && ok;
	ok = check_keyed_chunk("[1, ", "{b = 1}") && ok;
	ok = check_keyed_chunk_error("b = 1") && ok;
	ok = check_keyed_chunk_error("{b = 1}") && ok;
	ok = check_resumed_chunk("a = [1, ", "[{x = 1}]", "c", NULL) && ok;
	ok = check_resumed_chunk("a = [1, ", "[{}]", "c", NULL) && ok;
	ok = check_resumed_chunk("a = [1, ", "[]", "c", NULL) && ok;
	ok = check_resumed_chunk("a = [1, ", "[{x = 1}]", "c = 2; d = 3",
							 "a = [1, {x = 1}]; c = 2; d = 3") && ok;
	ok = check_resumed_chunk("a = [1, ", "[2]", "z = 4",
							 "a = [1, 2]; z = 4") && ok;
	/* New frames left open: resume inside them, not as an array value */
	ok = check_resumed_chunk("a = [1, ", "[{x = {}", "5", NULL) && ok;
	ok = check_resumed_chunk("a = [1, ", "[{x = {}", "y = 2}]; z = 4",
							 "a = [1, {x = {}, y = 2}]; z = 4") && ok;
	/* A hard error after closing the frame must stick */
	ok = check_resumed_chunk("a { b = [1, ", "[2]]", "c = 3}", NULL) && ok;
	ok = check_failed_insert("a = 1; b = }", "c = 2") && ok;
	ok = check_failed_insert("a = [1, }", "[2") && ok;
	ok = check_unadded_chunk() && ok;
	/* Refused by a special handler before the chunk is added */
	ok = check_macro_insert("a = 1; .insert \"XX\" b = 2", false,
							"a = 1; b = 2") && ok;
	/* A failed .include leaves its chunk above the inserted one */
	ok = check_macro_insert("a { .insert \".include $INC; c = 1\" b = 2 } }",
							false, "a { x { b = 2 } }") && ok;
	/* Braces opened by the inserted chunk stay unmatched in the outer one */
	ok = check_macro_insert("a { .insert \"b {\"", false, NULL) && ok;
	/*
	 * A macro cannot go on in the outer chunk, which would run .insert on
	 * "y = 2" in place of .priority, or read a name from the wrong chunk
	 */
	ok = check_macro_insert("a { b { .insert \"x = 1 } .priority \" \"y = 2\" }",
							false, NULL) && ok;
	ok = check_macro_insert("a { b { .insert \"x = 1 } .pri\" \"y = 2\" }",
							false, NULL) && ok;
	/* On the same frame the outer chunk goes on, but without the macro */
	ok = check_macro_insert("a { .insert \"x = 1; .priority \" y = 2 }",
							false, "a { x = 1; y = 2 }") && ok;
	ok = check_null_retyped_chunk("a = 1") && ok;
	ok = check_null_retyped_chunk("a = \"str\"") && ok;
	ok = check_keyless_include("a = 1", "[{}") && ok;
	ok = check_keyless_include("", "[5") && ok;
	ok = check_failed_include("a { .include \"$OUTER_INC\" }",
							  ".include \"$INNER_INC\"\nc = 3\n", "x = }",
							  "empty_chunks_inner.inc") && ok;
	/* Fails before the included file is parsed */
	ok = check_failed_include("p = 1\n"
							  ".include(prefix=true; key=\"p\") \"$OUTER_INC\"\n",
							  "q = 2\n", "", "empty_chunks_outer.inc") && ok;
	ok = check_closing_include("a = [{", ".include", "b = 1 }]",
							   UCL_ESYNTAX) && ok;
	ok = check_closing_include("[{", ".include", "b = 1 }]", UCL_ESYNTAX) && ok;
	ok = check_closing_include("x {", ".include(prefix=true; key=\"p\")",
							   "y = 1 }", UCL_EUNPAIRED) && ok;
	ok = check_closing_include("x {",
							   ".include(prefix=true; key=\"p\"; target=\"array\")",
							   "y = 1 }", UCL_EUNPAIRED) && ok;
	return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
