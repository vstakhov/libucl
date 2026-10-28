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
 * Regression test for the duplicate-key error message in
 * ucl_parser_process_object_element. With UCL_PARSER_ZEROCOPY the parsed keys
 * are unterminated pointers into the caller's input (documented in ucl.h), so
 * the UCL_DUPLICATE_ERROR branch must not format nobj->key with a plain "%s":
 * that runs off the end of the input buffer. We feed the parser from an
 * exact-size heap buffer (no trailing NUL) so AddressSanitizer catches the
 * over-read at the first byte past the input.
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
test_zerocopy_duplicate_error(void)
{
	/* Two entries with the same key trigger UCL_DUPLICATE_ERROR. */
	static const char doc[] = "a=1;a=2";
	const size_t len = sizeof(doc) - 1;

	/*
	 * Exact-size buffer: no room for a terminating NUL, so any read past
	 * the key relies on whatever follows it in the input and, here, past
	 * the allocation itself.
	 */
	unsigned char *buf = malloc(len);
	assert(buf != NULL);
	memcpy(buf, doc, len);

	struct ucl_parser *p = ucl_parser_new(UCL_PARSER_ZEROCOPY);
	assert(p != NULL);

	bool res = ucl_parser_add_chunk_full(p, buf, len,
										 ucl_parser_get_default_priority(p),
										 UCL_DUPLICATE_ERROR, UCL_PARSE_UCL);

	/* The duplicate key must be reported as an error, not silently merged. */
	assert(!res);
	const char *err = ucl_parser_get_error(p);
	assert(err != NULL);
	/* The message names exactly the key, not the rest of the input. */
	assert(strstr(err, "duplicate element for key 'a' found") != NULL);

	ucl_parser_free(p);
	free(buf);
}

int main(int argc, char **argv)
{
	(void) argc;
	(void) argv;

	test_zerocopy_duplicate_error();

	printf("All duplicate-key error tests passed\n");

	return 0;
}
