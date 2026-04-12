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
 * Round-trip regression test for UCL_EMIT_CONFIG.
 *
 * Each case builds an object programmatically, emits it as UCL_EMIT_CONFIG,
 * re-parses the emitted text, and asserts that the re-parsed object is
 * structurally equal to the original via ucl_object_compare().
 *
 * Originally added to cover the emitter-escaping bugs fixed in PR #373:
 *   - embedded "\nEOD\n" in a heredoc-eligible string prematurely closed
 *     the heredoc block on re-parse
 *   - a literal "\'" in a squoted string was misinterpreted on re-parse
 *
 * New cases can be added without any .res files — the test is
 * self-validating.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ucl.h"

struct roundtrip_case {
	const char *name;
	/* Builds and returns the object to round-trip. */
	ucl_object_t *(*build)(void);
};

/* Helper: insert a string value with a given length (so we can include NULs
 * or embedded newlines without C-string terminator ambiguity). */
static void
insert_lstring(ucl_object_t *obj, const char *key, const char *val, size_t len)
{
	ucl_object_t *cur = ucl_object_fromlstring(val, len);
	ucl_object_insert_key(obj, cur, key, 0, true);
}

/* Case 1: multiline string with "\nEOD\n" embedded — pre-fix this broke
 * the heredoc block on re-parse. */
static ucl_object_t *
build_heredoc_eod_middle(void)
{
	ucl_object_t *obj = ucl_object_typed_new(UCL_OBJECT);
	const char v[] =
		"a string that is longer than eighty characters so that "
		"the emitter picks heredoc mode\nEOD\ntrailing content";
	insert_lstring(obj, "k", v, sizeof(v) - 1);
	return obj;
}

/* Case 2: heredoc-eligible string whose embedded EOD sits at the very end
 * (no trailing newline after it). Exercises the boundary condition in the
 * emitter's scan loop. */
static ucl_object_t *
build_heredoc_eod_tail(void)
{
	ucl_object_t *obj = ucl_object_typed_new(UCL_OBJECT);
	const char v[] =
		"another long string padded so the emitter selects multiline "
		"heredoc output for sure\nEOD";
	insert_lstring(obj, "k", v, sizeof(v) - 1);
	return obj;
}

/* Case 3: heredoc-eligible string with the letters "EOD" that are NOT
 * preceded by a newline — must still round-trip as a heredoc (i.e., the
 * fallback must NOT fire on false positives). */
static ucl_object_t *
build_heredoc_eod_substring(void)
{
	ucl_object_t *obj = ucl_object_typed_new(UCL_OBJECT);
	const char v[] =
		"plenty of content to exceed the long-string threshold so "
		"heredoc mode is picked\nsomething EOD here not at line start";
	insert_lstring(obj, "k", v, sizeof(v) - 1);
	return obj;
}

/* Case 4: normal multiline heredoc without any EOD — regression guard that
 * heredoc emission still works in the common case. */
static ucl_object_t *
build_heredoc_normal(void)
{
	ucl_object_t *obj = ucl_object_typed_new(UCL_OBJECT);
	const char v[] =
		"first line of a perfectly ordinary multiline string\n"
		"second line\n"
		"third line containing enough bytes to exceed the threshold";
	insert_lstring(obj, "k", v, sizeof(v) - 1);
	return obj;
}

/* Case 5: squoted string with literal "\'" — pre-fix the parser treated
 * this as an escape sequence and dropped the backslash on re-parse. */
static ucl_object_t *
build_squote_backslash_quote(void)
{
	ucl_object_t *obj = ucl_object_typed_new(UCL_OBJECT);
	ucl_object_t *cur = ucl_object_fromstring("value with \\' inside");
	cur->flags |= UCL_OBJECT_SQUOTED;
	ucl_object_insert_key(obj, cur, "k", 0, true);
	return obj;
}

/* Case 6: squoted string with a single quote and no backslash — regression
 * guard for normal squoted output. */
static ucl_object_t *
build_squote_simple(void)
{
	ucl_object_t *obj = ucl_object_typed_new(UCL_OBJECT);
	ucl_object_t *cur = ucl_object_fromstring("can't stop won't stop");
	cur->flags |= UCL_OBJECT_SQUOTED;
	ucl_object_insert_key(obj, cur, "k", 0, true);
	return obj;
}

/* Case 7: multiple backslashes adjacent to quotes and newlines, combining
 * both fix paths in one object. */
static ucl_object_t *
build_combined(void)
{
	ucl_object_t *obj = ucl_object_typed_new(UCL_OBJECT);

	/* long string with embedded EOD */
	const char v1[] =
		"combined case: first a long heredoc-eligible payload "
		"so the multiline branch is taken\nEOD\nmiddle\nEOD\nend";
	insert_lstring(obj, "multiline", v1, sizeof(v1) - 1);

	/* short squoted string with backslash-quote */
	ucl_object_t *sq = ucl_object_fromstring("\\'");
	sq->flags |= UCL_OBJECT_SQUOTED;
	ucl_object_insert_key(obj, sq, "squoted", 0, true);

	/* plain nested object for good measure */
	ucl_object_t *nested = ucl_object_typed_new(UCL_OBJECT);
	ucl_object_insert_key(nested, ucl_object_fromint(42), "answer", 0, true);
	ucl_object_insert_key(obj, nested, "nested", 0, true);

	return obj;
}

/* Case 8: very simple scalar object — baseline sanity check. */
static ucl_object_t *
build_simple(void)
{
	ucl_object_t *obj = ucl_object_typed_new(UCL_OBJECT);
	ucl_object_insert_key(obj, ucl_object_fromstring("hello"), "greeting", 0, true);
	ucl_object_insert_key(obj, ucl_object_fromint(123), "number", 0, true);
	ucl_object_insert_key(obj, ucl_object_frombool(true), "flag", 0, true);
	return obj;
}

static const struct roundtrip_case cases[] = {
	{"simple",                   build_simple},
	{"heredoc_normal",           build_heredoc_normal},
	{"heredoc_eod_middle",       build_heredoc_eod_middle},
	{"heredoc_eod_tail",         build_heredoc_eod_tail},
	{"heredoc_eod_substring",    build_heredoc_eod_substring},
	{"squote_simple",            build_squote_simple},
	{"squote_backslash_quote",   build_squote_backslash_quote},
	{"combined",                 build_combined},
};

static int
run_case(const struct roundtrip_case *tc)
{
	ucl_object_t *orig = tc->build();
	if (orig == NULL) {
		fprintf(stderr, "[%s] FAIL: build returned NULL\n", tc->name);
		return 1;
	}

	unsigned char *emitted = ucl_object_emit(orig, UCL_EMIT_CONFIG);
	if (emitted == NULL) {
		fprintf(stderr, "[%s] FAIL: emit returned NULL\n", tc->name);
		ucl_object_unref(orig);
		return 1;
	}

	struct ucl_parser *parser = ucl_parser_new(0);
	if (!ucl_parser_add_string(parser, (const char *)emitted, 0) ||
			ucl_parser_get_error(parser) != NULL) {
		fprintf(stderr, "[%s] FAIL: re-parse error: %s\nemitted was:\n%s\n",
				tc->name,
				ucl_parser_get_error(parser)
					? ucl_parser_get_error(parser)
					: "(unknown)",
				emitted);
		ucl_parser_free(parser);
		free(emitted);
		ucl_object_unref(orig);
		return 1;
	}

	ucl_object_t *reparsed = ucl_parser_get_object(parser);
	ucl_parser_free(parser);

	int cmp = ucl_object_compare(orig, reparsed);
	int rc = 0;
	if (cmp != 0) {
		fprintf(stderr,
				"[%s] FAIL: round-trip object mismatch (compare=%d)\n"
				"emitted was:\n%s\n",
				tc->name, cmp, emitted);
		rc = 1;
	}
	else {
		/* Second round-trip: emit the re-parsed object and verify the
		 * emitter is idempotent (the text is identical to the first
		 * emit). This catches emitters whose output depends on some
		 * flag that was stripped by the parser. */
		unsigned char *emitted2 = ucl_object_emit(reparsed, UCL_EMIT_CONFIG);
		if (emitted2 == NULL) {
			fprintf(stderr, "[%s] FAIL: second emit returned NULL\n",
					tc->name);
			rc = 1;
		}
		else if (strcmp((const char *)emitted, (const char *)emitted2) != 0) {
			fprintf(stderr,
					"[%s] FAIL: emit not idempotent\nfirst:\n%s\nsecond:\n%s\n",
					tc->name, emitted, emitted2);
			rc = 1;
		}
		free(emitted2);
	}

	if (rc == 0) {
		printf("[%s] ok\n", tc->name);
	}

	free(emitted);
	ucl_object_unref(orig);
	ucl_object_unref(reparsed);
	return rc;
}

int
main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	int failures = 0;
	size_t ncases = sizeof(cases) / sizeof(cases[0]);

	for (size_t i = 0; i < ncases; i++) {
		failures += run_case(&cases[i]);
	}

	if (failures != 0) {
		fprintf(stderr, "%d/%zu round-trip cases failed\n", failures, ncases);
		return 1;
	}

	printf("all %zu round-trip cases passed\n", ncases);
	return 0;
}
