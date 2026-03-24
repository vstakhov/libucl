/*
 * Copyright (c) 2026, Vsevolod Stakhov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *	 * Redistributions of source code must retain the above copyright
 *	   notice, this list of conditions and the following disclaimer.
 *	 * Redistributions in binary form must reproduce the above copyright
 *	   notice, this list of conditions and the following disclaimer in the
 *	   documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR ''AS IS'' AND ANY
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
 * Tests for malformed/truncated msgpack input.
 * Regression tests for issue #363 (heap-buffer-overread in
 * ucl_msgpack_parse_ignore due to missing bounds check after length
 * reassignment for fixext/ext types).
 */

#include "ucl.h"
#include <stdio.h>
#include <string.h>

static int failed = 0;

#define FAIL(fmt, ...) \
	do { \
		fprintf(stderr, "FAIL [%s]: " fmt "\n", __func__, ##__VA_ARGS__); \
		failed++; \
		return; \
	} while (0)

#define CHECK(cond, msg) \
	do { \
		if (!(cond)) { FAIL("%s", msg); } \
	} while (0)

/* Return true if parsing the input as msgpack fails (does not crash). */
static bool
parse_msgpack_fails(const unsigned char *data, size_t len)
{
	struct ucl_parser *parser = ucl_parser_new(UCL_PARSER_DISABLE_MACRO);
	bool ok;

	ok = ucl_parser_add_chunk_full(parser, data, len,
								   0, UCL_DUPLICATE_APPEND, UCL_PARSE_MSGPACK);
	ucl_parser_free(parser);
	return !ok;
}

/* Return true if parsing succeeds and produces a non-NULL object. */
static bool
parse_msgpack_ok(const unsigned char *data, size_t len)
{
	struct ucl_parser *parser = ucl_parser_new(UCL_PARSER_DISABLE_MACRO);
	ucl_object_t *obj = NULL;
	bool ok;

	ok = ucl_parser_add_chunk_full(parser, data, len,
								   0, UCL_DUPLICATE_APPEND, UCL_PARSE_MSGPACK);
	if (ok) {
		obj = ucl_parser_get_object(parser);
	}
	ucl_parser_free(parser);

	if (obj) {
		ucl_object_unref(obj);
		return true;
	}
	return false;
}

/* ------------------------------------------------------------------ */
/* Issue #363: fixext/ext types with insufficient remaining data       */
/* ------------------------------------------------------------------ */

/*
 * Original reproducer from the issue report:
 * fixarray(11) + fixext2 with only 2 bytes of data instead of 3.
 * 0x9b = fixarray with 11 elements
 * 0xd5 = fixext2 (needs type byte + 2 data bytes = 3 bytes after format byte)
 * Only 0x61,0x3a remain — 2 bytes, not enough.
 */
static void
test_issue363_fixext2_truncated(void)
{
	const unsigned char input[] = {0x9b, 0xd5, 0x61, 0x3a};
	CHECK(parse_msgpack_fails(input, sizeof(input)),
		  "fixext2 with insufficient data should fail, not overread");
}

/*
 * fixarray(1) + fixext1: needs 2 bytes (type + 1 data), provide only 1.
 * 0x91 = fixarray(1)
 * 0xd4 = fixext1, len from table = 1, parse_ignore sets len = 2
 * Only 1 byte (0xAA) remains.
 */
static void
test_fixext1_truncated(void)
{
	const unsigned char input[] = {0x91, 0xd4, 0xaa};
	CHECK(parse_msgpack_fails(input, sizeof(input)),
		  "fixext1 with insufficient data should fail");
}

/*
 * fixarray(1) + fixext4: needs 5 bytes (type + 4 data), provide only 3.
 * 0x91 = fixarray(1)
 * 0xd6 = fixext4, len from table = 4, parse_ignore sets len = 5
 */
static void
test_fixext4_truncated(void)
{
	const unsigned char input[] = {0x91, 0xd6, 0x01, 0x02, 0x03};
	CHECK(parse_msgpack_fails(input, sizeof(input)),
		  "fixext4 with insufficient data should fail");
}

/*
 * fixarray(1) + fixext8: needs 9 bytes (type + 8 data), provide only 4.
 * 0x91 = fixarray(1)
 * 0xd7 = fixext8, len from table = 8, parse_ignore sets len = 9
 */
static void
test_fixext8_truncated(void)
{
	const unsigned char input[] = {0x91, 0xd7, 0x01, 0x02, 0x03, 0x04};
	CHECK(parse_msgpack_fails(input, sizeof(input)),
		  "fixext8 with insufficient data should fail");
}

/*
 * fixarray(1) + fixext16: needs 17 bytes (type + 16 data), provide only 4.
 * 0x91 = fixarray(1)
 * 0xd8 = fixext16, len from table = 16, parse_ignore sets len = 17
 */
static void
test_fixext16_truncated(void)
{
	const unsigned char input[] = {0x91, 0xd8, 0x01, 0x02, 0x03, 0x04};
	CHECK(parse_msgpack_fails(input, sizeof(input)),
		  "fixext16 with insufficient data should fail");
}

/*
 * fixarray(1) + ext8 with length byte claiming 10 data bytes, but only 2 remain.
 * 0x91 = fixarray(1)
 * 0xc7 = ext8 (1-byte length prefix)
 * 0x0a = length = 10
 * parse_ignore does len = 10 + 1 = 11 (for the type byte)
 * Only 0xBB,0xCC remain (2 bytes).
 */
static void
test_ext8_truncated(void)
{
	const unsigned char input[] = {0x91, 0xc7, 0x0a, 0xbb, 0xcc};
	CHECK(parse_msgpack_fails(input, sizeof(input)),
		  "ext8 with insufficient data should fail");
}

/*
 * fixarray(1) + ext16 with 2-byte length claiming 256 data bytes, but only 1 remains.
 * 0x91 = fixarray(1)
 * 0xc8 = ext16 (2-byte length prefix, big-endian)
 * 0x01,0x00 = length = 256
 * parse_ignore does len = 256 + 1 = 257
 * Only 0xFF remains.
 */
static void
test_ext16_truncated(void)
{
	const unsigned char input[] = {0x91, 0xc8, 0x01, 0x00, 0xff};
	CHECK(parse_msgpack_fails(input, sizeof(input)),
		  "ext16 with insufficient data should fail");
}

/* ------------------------------------------------------------------ */
/* Other truncation / malformed input tests                            */
/* ------------------------------------------------------------------ */

/* Empty input — libucl accepts this (returns empty top-level object) */
static void
test_empty_input(void)
{
	const unsigned char input[] = {0};
	CHECK(parse_msgpack_ok(NULL, 0),
		  "empty input should succeed (empty top-level object)");
}

/* Single type byte with no payload for a type that requires one. */
static void
test_truncated_string(void)
{
	/* 0xd9 = str8 — needs at least 1 length byte, none available */
	const unsigned char input[] = {0x91, 0xd9};
	CHECK(parse_msgpack_fails(input, sizeof(input)),
		  "str8 with no length byte should fail");
}

/* Map with truncated key */
static void
test_truncated_map_key(void)
{
	/* 0x81 = fixmap(1) — expects 1 key-value pair
	 * 0xa5 = fixstr(5) — but only 2 bytes of string follow */
	const unsigned char input[] = {0x81, 0xa5, 'a', 'b'};
	CHECK(parse_msgpack_fails(input, sizeof(input)),
		  "map with truncated key should fail");
}

/* Truncated integer */
static void
test_truncated_int(void)
{
	/* 0x91 = fixarray(1)
	 * 0xce = uint32, needs 4 bytes, only 2 available */
	const unsigned char input[] = {0x91, 0xce, 0x00, 0x01};
	CHECK(parse_msgpack_fails(input, sizeof(input)),
		  "uint32 with insufficient data should fail");
}

/* Unknown format byte */
static void
test_unknown_format(void)
{
	/* 0xc1 is never-used / reserved in msgpack */
	const unsigned char input[] = {0x91, 0xc1};
	CHECK(parse_msgpack_fails(input, sizeof(input)),
		  "reserved format byte 0xc1 should fail");
}

/* ------------------------------------------------------------------ */

int
main(void)
{
	/* Issue #363 regression tests */
	test_issue363_fixext2_truncated();
	test_fixext1_truncated();
	test_fixext4_truncated();
	test_fixext8_truncated();
	test_fixext16_truncated();
	test_ext8_truncated();
	test_ext16_truncated();

	/* General malformed input tests */
	test_empty_input();
	test_truncated_string();
	test_truncated_map_key();
	test_truncated_int();
	test_unknown_format();

	if (failed) {
		fprintf(stderr, "%d test(s) FAILED\n", failed);
		return 1;
	}

	printf("All msgpack malformed-input tests passed\n");
	return 0;
}
