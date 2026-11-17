/*
 * Regression test: a macro whose value is missing at the very end of the
 * input must be rejected without reading past the end of the chunk.
 *
 * Each input is placed so that its last byte is the last byte of a mapped
 * page and the following page is PROT_NONE, mirroring the mmap() layout
 * used by ucl_parser_add_fd(). Any overread faults even without sanitizers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "ucl.h"

struct macro_eof_case {
	const char *input;
	bool expect_ok;
};

static const struct macro_eof_case cases[] = {
	{".priority(a=1)", false},
	{".priority(a=1) ", false},
	{".priority(a=1)\n", false},
	{".priority(a=1)#c", false},
	{".priority(a=1)/* c */", false},
	{"a = 1; .priority(a=1)", false},
	{".priority(a=1) 2", true},
	{".priority(a=1) \"2\"", true},
	{".priority 1", true},
};

int
main(void)
{
	size_t pg = (size_t) sysconf(_SC_PAGESIZE);
	unsigned char *map;
	unsigned int i, failures = 0;

	map = mmap(NULL, pg * 2, PROT_READ | PROT_WRITE,
			   MAP_PRIVATE | MAP_ANON, -1, 0);
	if (map == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}
	if (mprotect(map + pg, pg, PROT_NONE) != 0) {
		perror("mprotect");
		return EXIT_FAILURE;
	}

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		size_t len = strlen(cases[i].input);
		unsigned char *buf = map + pg - len;
		struct ucl_parser *parser;
		bool ok;

		memset(map, 0, pg);
		memcpy(buf, cases[i].input, len);

		parser = ucl_parser_new(0);
		if (parser == NULL) {
			fprintf(stderr, "cannot create parser\n");
			return EXIT_FAILURE;
		}
		ok = ucl_parser_add_chunk(parser, buf, len);
		if (ok != cases[i].expect_ok) {
			fprintf(stderr, "case %u (%s): expected %s, got %s%s\n", i,
					cases[i].input, cases[i].expect_ok ? "success" : "error",
					ok ? "success" : "error: ",
					ok ? "" : ucl_parser_get_error(parser));
			failures++;
		}
		ucl_parser_free(parser);
	}

	munmap(map, pg * 2);

	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
