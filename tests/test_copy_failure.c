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

/* Exercise copy and include cleanup with deterministic allocation failures in
 * ucl_util.c and ucl_hash.c. */
#include <stdlib.h>
#include <string.h>
#undef NDEBUG
#include <assert.h>

static size_t fail_at, allocations, live;
static void *owned[4096];
/* The size expression of the next UCL_ALLOC to fail, if any */
static const char *fail_site;

static void *
copy_test_alloc(size_t size)
{
	void *p;
	size_t i;

	if (++allocations == fail_at) {
		return NULL;
	}
	p = malloc(size);
	if (p != NULL) {
		for (i = 0; i < sizeof(owned) / sizeof(owned[0]); i++) {
			if (owned[i] == NULL) {
				owned[i] = p;
				live++;
				return p;
			}
		}
		abort();
	}
	return p;
}

static void
copy_test_free(void *p)
{
	size_t i;

	if (p != NULL) {
		for (i = 0; i < sizeof(owned) / sizeof(owned[0]); i++) {
			if (owned[i] == p) {
				owned[i] = NULL;
				live--;
				break;
			}
		}
	}
	free(p);
}

static void *
copy_test_alloc_site(size_t size, const char *site)
{
	if (fail_site != NULL && strcmp(site, fail_site) == 0) {
		fail_site = NULL;
		return NULL;
	}
	return copy_test_alloc(size);
}

#define malloc(size) copy_test_alloc(size)
#define free(ptr) copy_test_free(ptr)
#define UCL_ALLOC(size) copy_test_alloc_site(size, #size)
#define UCL_FREE(size, ptr) copy_test_free(ptr)
/* Compile these translation units with their allocator hooks; other library
 * sources use their normal allocators. No production fault-injection API. */
#include "../src/ucl_util.c"
#include "../src/ucl_hash.c"
#undef malloc
#undef free

static void
test_copy_failures(void)
{
	ucl_object_t *root = ucl_object_new();
	ucl_object_t *array = ucl_object_typed_new(UCL_ARRAY);
	ucl_object_t *copy;
	unsigned char *expected, *actual;
	size_t baseline, total, i;

	assert(root != NULL && array != NULL);
	assert(ucl_object_insert_key(root, ucl_object_fromstring("first"), "key", 3, true));
	assert(ucl_object_insert_key(root, ucl_object_fromstring("second"), "key", 3, true));
	assert(ucl_array_append(array, ucl_object_fromstring("nested")));
	assert(ucl_object_insert_key(root, array, "array", 5, true));
	expected = ucl_object_emit(root, UCL_EMIT_JSON_COMPACT);
	assert(expected != NULL);
	baseline = live;
	allocations = 0;
	copy = ucl_object_copy(root);
	assert(copy != NULL);
	total = allocations;
	actual = ucl_object_emit(copy, UCL_EMIT_JSON_COMPACT);
	assert(actual != NULL && strcmp((char *) actual, (char *) expected) == 0);
	free(actual);
	ucl_object_unref(copy);
	assert(live == baseline);

	for (i = 1; i <= total; i++) {
		allocations = 0;
		fail_at = i;
		copy = ucl_object_copy(root);
		fail_at = 0;
		assert(copy == NULL);
		assert(live == baseline);
		actual = ucl_object_emit(root, UCL_EMIT_JSON_COMPACT);
		assert(actual != NULL && strcmp((char *) actual, (char *) expected) == 0);
		free(actual);
	}
	free(expected);
	ucl_object_unref(root);
	assert(live == 0);
}

static void
test_escaped_sizes(void)
{
	size_t len = SIZE_MAX - 5;
	ucl_object_t *obj;
	const char input[] = {'A', '\0', '\v', '\n', '"', '\\', ' '};
	const char expected[] = "A\\u0000\\u000B\\n\\\"\\\\ ";

	assert(!ucl_escaped_len_add(&len, '\0') && len == SIZE_MAX - 5);
	assert(!ucl_escaped_len_add(&len, '\v') && len == SIZE_MAX - 5);
	len = SIZE_MAX;
	assert(!ucl_escaped_len_add(&len, 'A') && len == SIZE_MAX);
	len = SIZE_MAX - 1;
	assert(!ucl_escaped_len_add(&len, '\n') && len == SIZE_MAX - 1);
	assert(ucl_escaped_len_add(&len, 'A') && len == SIZE_MAX);
	obj = ucl_object_fromstring_common(input, sizeof(input), UCL_STRING_ESCAPE);
	assert(obj != NULL && obj->len == sizeof(expected) - 1);
	assert(memcmp(obj->value.sv, expected, sizeof(expected)) == 0);
	ucl_object_unref(obj);
	assert(live == 0);
	/* Object creation succeeds, but allocating the escaped bytes fails. */
	allocations = 0;
	fail_at = 2;
	obj = ucl_object_fromstring_common(input, sizeof(input), UCL_STRING_ESCAPE);
	fail_at = 0;
	assert(obj == NULL && live == 0);
}

static int userdata_freed;

static void
test_userdata_dtor(void *ptr)
{
	userdata_freed++;
	free(ptr);
}

static const char *
test_userdata_emitter(void *ptr)
{
	return ptr;
}

static ucl_object_t *
new_test_userdata(bool owning)
{
	char *payload = malloc(sizeof("userdata"));
	ucl_object_t *obj;

	assert(payload != NULL);
	memcpy(payload, "userdata", sizeof("userdata"));
	obj = ucl_object_new_userdata(owning ? test_userdata_dtor : NULL,
								 test_userdata_emitter, payload);
	assert(obj != NULL);
	return obj;
}

/* An opaque destructor cannot be duplicated safely, even for a NULL payload. */
static void
test_userdata_copy_rejected(void)
{
	ucl_object_t *obj, *copy;
	size_t baseline;
	int i;

	for (i = 0; i < 2; i++) {
		userdata_freed = 0;
		obj = i == 0 ? new_test_userdata(true) :
					   ucl_object_new_userdata(test_userdata_dtor, NULL, NULL);
		assert(obj != NULL);
		baseline = live;
		copy = ucl_object_copy(obj);
		assert(copy == NULL && live == baseline && userdata_freed == 0);
		assert(obj->ref == 1);
		if (obj->value.ud != NULL) {
			assert(strcmp(obj->value.ud, "userdata") == 0);
		}
		ucl_object_unref(obj);
		assert(userdata_freed == 1 && live == 0);
	}
}

/* Sharing the object keeps its payload alive until the last reference. */
static void
test_userdata_shared_refs(bool original_first)
{
	ucl_object_t *obj = new_test_userdata(true);
	ucl_object_t *shared = ucl_object_ref(obj);
	ucl_object_t *remaining = original_first ? shared : obj;

	userdata_freed = 0;
	assert(shared == obj && obj->ref == 2);
	ucl_object_unref(original_first ? obj : shared);
	assert(userdata_freed == 0 && remaining->ref == 1);
	assert(strcmp(remaining->value.ud, "userdata") == 0);
	ucl_object_unref(remaining);
	assert(userdata_freed == 1 && live == 0);
}

/* Non-owning copies have independent wrappers and borrow the same payload. */
static void
test_userdata_nonowning_copy(bool original_first)
{
	ucl_object_t *obj = new_test_userdata(false);
	ucl_object_t *copy = ucl_object_copy(obj);
	ucl_object_t *remaining = original_first ? copy : obj;
	void *payload = obj->value.ud;

	userdata_freed = 0;
	assert(copy != NULL && copy != obj);
	assert(obj->ref == 1 && copy->ref == 1);
	assert(copy->type == UCL_USERDATA && copy->value.ud == payload);
	assert(((struct ucl_object_userdata *) copy)->dtor == NULL);
	assert(((struct ucl_object_userdata *) copy)->emitter == test_userdata_emitter);
	ucl_object_unref(original_first ? obj : copy);
	assert(userdata_freed == 0 && remaining->ref == 1);
	assert(strcmp(remaining->value.ud, "userdata") == 0);
	ucl_object_unref(remaining);
	assert(userdata_freed == 0 && live == 0);
	assert(strcmp(payload, "userdata") == 0);
	free(payload);
}

static void
test_userdata_failure(void)
{
	char payload[] = "userdata";
	ucl_object_t *obj = ucl_object_new_userdata(NULL, test_userdata_emitter, payload);
	ucl_object_t *holder = ucl_object_new();
	ucl_object_t *copy;
	size_t baseline;

	userdata_freed = 0;
	assert(obj != NULL && holder != NULL);
	assert(ucl_object_insert_key(holder, obj, "userdata", 8, true));
	baseline = live;
	allocations = 0;
	fail_at = 2; /* The non-owning wrapper copies, but its key allocation fails. */
	copy = ucl_object_copy(obj);
	fail_at = 0;
	assert(allocations == 2);
	assert(copy == NULL && live == baseline && userdata_freed == 0);
	assert(obj->ref == 1 && obj->value.ud == payload);
	assert(strcmp(ucl_object_key(obj), "userdata") == 0);
	ucl_object_unref(holder);
	assert(userdata_freed == 0 && live == 0);
}

/* Failure after copying a borrowed wrapper must leave the source tree intact. */
static void
test_userdata_container_failure(void)
{
	ucl_object_t *root, *inner, *obj, *borrowed, *copy;
	size_t baseline;
	int i;

	for (i = 0; i < 3; i++) {
		userdata_freed = 0;
		root = ucl_object_typed_new(i == 0 ? UCL_ARRAY : UCL_OBJECT);
		obj = new_test_userdata(true);
		borrowed = ucl_object_new_userdata(NULL, test_userdata_emitter, obj->value.ud);
		assert(root != NULL && borrowed != NULL);

		if (i == 2) {
			/* An implicit array starts with non-owning userdata. */
			assert(ucl_object_insert_key(root, borrowed, "key", 3, true));
			assert(ucl_object_insert_key(root, obj, "key", 3, true));
			assert(borrowed->next == obj);
		}
		else {
			inner = ucl_object_typed_new(UCL_ARRAY);
			assert(inner != NULL && ucl_array_append(inner, obj));
			if (i == 0) {
				assert(ucl_array_append(root, borrowed));
				assert(ucl_array_append(root, inner));
			}
			else {
				assert(ucl_object_insert_key(root, borrowed, "borrowed", 8, true));
				assert(ucl_object_insert_key(root, inner, "inner", 5, true));
			}
		}

		baseline = live;
		copy = ucl_object_copy(root);
		assert(copy == NULL && live == baseline && userdata_freed == 0);
		assert(obj->ref == 1 && borrowed->ref == 1);
		assert(strcmp(obj->value.ud, "userdata") == 0);
		assert(borrowed->value.ud == obj->value.ud);
		ucl_object_unref(root);
		assert(userdata_freed == 1 && live == 0);
	}
}

static bool
is_live(const void *p)
{
	size_t i;

	for (i = 0; i < sizeof(owned) / sizeof(owned[0]); i++) {
		if (owned[i] == p) {
			return true;
		}
	}
	return false;
}

/*
 * A prefixed include puts the object that receives the file into the tree
 * before it allocates the stack frame nesting the file in it. When that
 * allocation fails, the object belongs to the tree and must stay there.
 */
static void
check_include_frame_failure(const char *path, const char *key,
							const char *target)
{
	struct ucl_parser *parser = ucl_parser_new(0);
	ucl_object_t *args = ucl_object_new(), *old;
	const ucl_object_t *nested;
	struct ucl_stack *stack;

	assert(parser != NULL && args != NULL);
	assert(ucl_parser_add_string(parser, "obj { a = 1; } arr = [1];", 0));
	assert(ucl_object_insert_key(args, ucl_object_fromstring(key), "key", 0, false));
	assert(ucl_object_insert_key(args, ucl_object_fromstring(target), "target",
								 0, false));
	old = __DECONST(ucl_object_t *, ucl_object_lookup(parser->top_obj, key));
	assert(old != NULL);
	ucl_object_ref(old);
	stack = parser->stack;

	fail_site = "sizeof(struct ucl_stack)";
	assert(!ucl_include_handler((const unsigned char *) path, strlen(path),
								args, parser));
	assert(fail_site == NULL && parser->stack == stack);
	assert(strstr(ucl_parser_get_error(parser), "cannot allocate") != NULL);
	if (ucl_object_type(old) == UCL_ARRAY) {
		/* A new element, which nothing but the array refers to */
		assert(ucl_array_size(old) == 2);
		nested = ucl_array_tail(old);
		assert(nested != NULL && is_live(nested) && nested->ref == 1);
	}
	else {
		/* The existing object, held by the tree and by this test */
		assert(old->ref == 2);
	}
	ucl_object_unref(old);
	ucl_object_unref(args);
	ucl_parser_free(parser);
}

static void
test_include_frame_failure(void)
{
	const char *dir = getenv("TEST_OUT_DIR");
	char path[PATH_MAX];
	FILE *out;

	snprintf(path, sizeof(path), "%s/copy_failure_include.conf",
			 dir != NULL ? dir : ".");
	out = fopen(path, "w");
	assert(out != NULL);
	fputs("b = 2;\n", out);
	assert(fclose(out) == 0);

	check_include_frame_failure(path, "obj", "object");
	check_include_frame_failure(path, "arr", "array");
	remove(path);
}

/* How many times a variable is registered in the parser */
static int
count_variable(struct ucl_parser *parser, const char *name)
{
	struct ucl_variable *var;
	int found = 0;

	for (var = parser->variables; var != NULL; var = var->next) {
		if (strcmp(var->var, name) == 0) {
			found++;
		}
	}

	return found;
}

static struct ucl_parser *
include_parser(const char *base)
{
	struct ucl_parser *parser = ucl_parser_new(0);

	assert(parser != NULL);
	assert(ucl_parser_add_string(parser, base, 0));

	return parser;
}

/*
 * A prefixed include creates or reuses the object that receives the included
 * file, and the stack frame for it, before the file is parsed. When one of
 * these allocations fails, the parent object must not keep a freed receiver
 * or one without a key, and the include must still give the file variables
 * back to the including file. The stack frame is released in ucl_parser.c,
 * out of reach of the allocation hooks, so leaks are left to leak checkers.
 */
static void
test_include_failures(void)
{
	static const struct {
		const char *base;
		const char *params;
	} cases[] = {
		{"a = 1;", "key=\"k\""},
		{"a = 1; k { x = 1 };", "key=\"k\""},
		{"a = 1;", "key=\"k\", target=\"array\""},
		{"a = 1; k [{ x = 1 }];", "key=\"k\", target=\"array\""},
		{"a = 1; k { x = 1 };", "key=\"k\", target=\"array\""},
	};
	const char *dir = getenv("TEST_OUT_DIR");
	char path[1024], include[1200];
	unsigned char *expected, *actual;
	struct ucl_parser *parser;
	size_t total, i, j;
	FILE *out;
	bool res;

	snprintf(path, sizeof(path), "%s/copy_failure.inc", dir != NULL ? dir : ".");
	out = fopen(path, "w");
	assert(out != NULL);
	fputs("b = 2;\n", out);
	assert(fclose(out) == 0);

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		snprintf(include, sizeof(include), ".include(%s) \"%s\"",
				 cases[i].params, path);
		parser = include_parser(cases[i].base);
		allocations = 0;
		assert(ucl_parser_add_string(parser, include, 0));
		total = allocations;
		expected = ucl_object_emit(parser->top_obj, UCL_EMIT_JSON_COMPACT);
		assert(expected != NULL);
		ucl_parser_free(parser);

		for (j = 1; j <= total; j++) {
			parser = include_parser(cases[i].base);
			allocations = 0;
			fail_at = j;
			res = ucl_parser_add_string(parser, include, 0);
			fail_at = 0;
			assert(ucl_parser_get_cur_file(parser) == NULL);
			assert(count_variable(parser, "FILENAME") == 1);
			assert(count_variable(parser, "CURDIR") == 1);
			/* Walks the whole tree, the receiver of a failed include too */
			actual = ucl_object_emit(parser->top_obj, UCL_EMIT_JSON_COMPACT);
			assert(actual != NULL);
			if (res) {
				assert(strcmp((char *) actual, (char *) expected) == 0);
			}
			else {
				assert(ucl_parser_get_error(parser) != NULL);
			}
			/* A failed insert of the receiver used to drop the whole parent */
			assert(strstr((char *) actual, "\"a\":1") != NULL);
			free(actual);
			ucl_parser_free(parser);
		}
		free(expected);
	}
	remove(path);
}

/*
 * Forget the allocations counted so far. Code out of reach of the hooks frees
 * some of them, and malloc can hand their addresses out again, which would
 * throw the count off: a leak check starts from a clean count instead.
 */
static void
forget_allocations(void)
{
	memset(owned, 0, sizeof(owned));
	live = 0;
}

/*
 * A value dropped for a key that already has a higher priority goes to the
 * parser's trash, with UCL_DUPLICATE_APPEND and UCL_DUPLICATE_MERGE alike.
 * The parser has to release its whole subtree, not only its direct children.
 */
static void
test_discarded_value(void)
{
	static const char *const docs[] = {
		"b { y { z = 1 } }",          /* control: nothing is discarded */
		"a { y { z = 1 } }",
		"a [[1, [2]], { w { v = 1 } }]",
	};
	static const enum ucl_duplicate_strategy strats[] = {
		UCL_DUPLICATE_APPEND,
		UCL_DUPLICATE_MERGE,
	};
	struct ucl_parser *parser;
	size_t baseline, i, j;

	for (i = 0; i < sizeof(docs) / sizeof(docs[0]); i++) {
		for (j = 0; j < sizeof(strats) / sizeof(strats[0]); j++) {
			forget_allocations();
			baseline = live;
			parser = ucl_parser_new(0);
			assert(parser != NULL);
			assert(ucl_parser_add_chunk_full(parser,
											 (const unsigned char *) "a = 1;", 6,
											 2, strats[j], UCL_PARSE_UCL));
			assert(ucl_parser_add_chunk_full(parser,
											 (const unsigned char *) docs[i],
											 strlen(docs[i]), 1, strats[j],
											 UCL_PARSE_UCL));
			assert((parser->trash_objs != NULL) == (i > 0));
			ucl_parser_free(parser);
			assert(live == baseline);
		}
	}
}

/*
 * Inserting or merging a key that fails to allocate must leave the target as
 * it was, with the caller still owning what was not inserted. A failed insert
 * into the hash replaced the whole hash with NULL, and a failed element
 * allocation in it was written through.
 */
static void
test_insert_failures(void)
{
	ucl_object_t *top, *elt, *src;
	size_t baseline, total, i, pass;
	bool res;

	for (pass = 0; pass < 2; pass++) {
		total = 0;

		for (i = 0; i == 0 || i <= total; i++) {
			forget_allocations();
			baseline = live;
			top = ucl_object_typed_new(UCL_OBJECT);
			assert(top != NULL);
			assert(ucl_object_insert_key(top, ucl_object_fromint(1), "a", 0, true));
			src = NULL;
			if (pass == 1) {
				src = ucl_object_typed_new(UCL_OBJECT);
				assert(src != NULL);
				assert(ucl_object_insert_key(src, ucl_object_fromint(2), "b", 0, true));
				assert(ucl_object_insert_key(src, ucl_object_fromint(3), "c", 0, true));
			}

			allocations = 0;
			fail_at = i;
			if (pass == 0) {
				elt = ucl_object_fromint(2);
				res = elt != NULL && ucl_object_insert_key(top, elt, "b", 0, true);
				if (!res) {
					ucl_object_unref(elt);
				}
			}
			else {
				res = ucl_object_merge(top, src, true);
			}
			fail_at = 0;
			if (i == 0) {
				total = allocations;
				assert(res);
			}

			assert(ucl_object_toint(ucl_object_lookup(top, "a")) == 1);
			if (res) {
				assert(ucl_object_toint(ucl_object_lookup(top, "b")) == 2);
			}
			ucl_object_unref(src);
			ucl_object_unref(top);
			assert(live == baseline);
		}
	}
}

/*
 * Iterating an object allocated an iterator, and a failed allocation ended
 * the iteration as if the object had no more keys, which cut short the
 * output of the emitters among others
 */
static void
test_iterate_without_memory(void)
{
	struct ucl_parser *parser;
	ucl_object_t *top;
	const ucl_object_t *cur;
	ucl_object_iter_t it;
	unsigned char *out;
	int ep, n;

	forget_allocations();
	parser = ucl_parser_new(0);
	assert(parser != NULL);
	assert(ucl_parser_add_string(parser, "x = 1; y = 2; e {} e { z = 3 }", 0));
	top = ucl_parser_get_object(parser);
	assert(top != NULL);
	ucl_parser_free(parser);

	allocations = 0;
	fail_at = 1;
	it = NULL;
	n = 0;
	while ((cur = ucl_object_iterate_with_error(top, &it, true, &ep)) != NULL) {
		n++;
	}
	assert(ep == 0 && n == 3);

	out = ucl_object_emit(top, UCL_EMIT_JSON_COMPACT);
	fail_at = 0;
	assert(allocations == 0);
	assert(out != NULL);
	assert(strcmp((const char *) out, "{\"x\":1,\"y\":2,\"e\":[{},{\"z\":3}]}") == 0);
	free(out);

	/* The safe iterator goes on past an empty object */
	it = ucl_object_iterate_new(ucl_object_lookup(top, "e"));
	assert(it != NULL);
	n = 0;
	while ((cur = ucl_object_iterate_safe(it, true)) != NULL) {
		assert(strcmp(ucl_object_key(cur), "z") == 0);
		n++;
	}
	assert(n == 1 && !ucl_object_iter_chk_excpn(it));
	ucl_object_iterate_free(it);

	ucl_object_unref(top);
}

int main(void)
{
	test_copy_failures();
	test_escaped_sizes();
	test_userdata_copy_rejected();
	test_userdata_shared_refs(true);
	test_userdata_shared_refs(false);
	test_userdata_nonowning_copy(true);
	test_userdata_nonowning_copy(false);
	test_userdata_failure();
	test_userdata_container_failure();
	test_include_frame_failure();
	test_include_failures();
	test_discarded_value();
	test_insert_failures();
	test_iterate_without_memory();
	return 0;
}
