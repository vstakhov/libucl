/*
 * Regression test for the key of a merged duplicate in ucl_parse_key. Under
 * UCL_DUPLICATE_MERGE, a key whose earlier value is an object merges into
 * that object and ucl_parser_process_object_element releases the new one, so
 * ucl_parse_key must not touch it afterwards to record that the key was
 * escaped. The second key below is written with a \u escape that decodes to
 * the same "foo", so it both merges and carries the escape flag.
 */

#include "ucl.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>

static void
test_merge_escaped_duplicate_key(void)
{
	static const char doc[] = "foo { a = 1 }\n\"\\u0066oo\" { b = 2 }\n";

	struct ucl_parser *p = ucl_parser_new(0);
	assert(p != NULL);

	bool res = ucl_parser_add_chunk_full(p, (const unsigned char *) doc,
										 sizeof(doc) - 1,
										 ucl_parser_get_default_priority(p),
										 UCL_DUPLICATE_MERGE, UCL_PARSE_UCL);
	assert(res);

	ucl_object_t *top = ucl_parser_get_object(p);
	assert(top != NULL);

	/* Both keys name one object holding both sets of members */
	const ucl_object_t *foo = ucl_object_lookup(top, "foo");
	assert(foo != NULL);
	assert(ucl_object_type(foo) == UCL_OBJECT);
	assert(foo->next == NULL);
	assert(ucl_object_toint(ucl_object_lookup(foo, "a")) == 1);
	assert(ucl_object_toint(ucl_object_lookup(foo, "b")) == 2);

	ucl_object_unref(top);
	ucl_parser_free(p);
}

int main(int argc, char **argv)
{
	(void) argc;
	(void) argv;

	test_merge_escaped_duplicate_key();

	printf("All duplicate merge key tests passed\n");

	return 0;
}
