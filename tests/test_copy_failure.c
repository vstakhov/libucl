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

/* Exercise copy cleanup with deterministic allocation failures in ucl_util.c. */
#include <stdlib.h>
#include <string.h>
#undef NDEBUG
#include <assert.h>

static size_t fail_at, allocations, live;
static void *owned[4096];

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

#define malloc(size) copy_test_alloc(size)
#define free(ptr) copy_test_free(ptr)
#define UCL_ALLOC(size) copy_test_alloc(size)
#define UCL_FREE(size, ptr) copy_test_free(ptr)
/* Compile this translation unit with its allocator hooks; other library
 * sources use their normal allocators. No production fault-injection API. */
#include "../src/ucl_util.c"
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
	(void) ptr;
	userdata_freed++;
}

static void
test_userdata_failure(void)
{
	ucl_object_t *obj = ucl_object_new_userdata(test_userdata_dtor, NULL, NULL);
	ucl_object_t *holder = ucl_object_new();
	ucl_object_t *copy;
	size_t baseline;

	assert(obj != NULL && holder != NULL);
	assert(ucl_object_insert_key(holder, obj, "userdata", 8, true));
	baseline = live;
	allocations = 0;
	fail_at = 2; /* The userdata object copies, but its key allocation fails. */
	copy = ucl_object_copy(obj);
	fail_at = 0;
	assert(copy == NULL && live == baseline && userdata_freed == 0);
	ucl_object_unref(holder);
	assert(userdata_freed == 1 && live == 0);
}

int main(void)
{
	test_copy_failures();
	test_escaped_sizes();
	test_userdata_failure();
	return 0;
}
