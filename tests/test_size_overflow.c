/* Copyright (c) 2026, Nathan Chandrasekar
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
 * Overflow checked size helpers (ucl_size_add/ucl_size_mul and their
 * portable fallbacks) and the kvec _safe macros built on top of them.
 */

#include "ucl_internal.h"
#include "kvec.h"
#include "khash.h"

/* These checks must also run in release builds */
#undef NDEBUG
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef bool (*size_op_t)(size_t, size_t, size_t *);

static void
check_op(size_op_t op, size_t a, size_t b, bool ok, size_t expected)
{
	size_t res = 42;

	assert(op(a, b, &res) == ok);
	/* The result is only written on success */
	assert(res == (ok ? expected : 42));
}

static void
test_add(size_op_t add)
{
	check_op(add, 0, 0, true, 0);
	check_op(add, 1, 2, true, 3);
	check_op(add, SIZE_MAX, 0, true, SIZE_MAX);
	check_op(add, 0, SIZE_MAX, true, SIZE_MAX);
	check_op(add, SIZE_MAX - 1, 1, true, SIZE_MAX);
	check_op(add, SIZE_MAX / 2, SIZE_MAX / 2 + 1, true, SIZE_MAX);
	check_op(add, SIZE_MAX, 1, false, 0);
	check_op(add, 1, SIZE_MAX, false, 0);
	check_op(add, SIZE_MAX / 2 + 1, SIZE_MAX / 2 + 1, false, 0);
	check_op(add, SIZE_MAX, SIZE_MAX, false, 0);
}

static void
test_mul(size_op_t mul)
{
	check_op(mul, 0, 0, true, 0);
	check_op(mul, 0, SIZE_MAX, true, 0);
	check_op(mul, SIZE_MAX, 0, true, 0);
	check_op(mul, 1, SIZE_MAX, true, SIZE_MAX);
	check_op(mul, SIZE_MAX, 1, true, SIZE_MAX);
	check_op(mul, 6, 7, true, 42);
	check_op(mul, SIZE_MAX / 2, 2, true, SIZE_MAX - 1);
	/* SIZE_MAX is 2^n - 1 with an even n, hence a multiple of 3 */
	check_op(mul, SIZE_MAX / 3, 3, true, SIZE_MAX);
	check_op(mul, SIZE_MAX / 2 + 1, 2, false, 0);
	check_op(mul, 2, SIZE_MAX / 2 + 1, false, 0);
	check_op(mul, SIZE_MAX / 3 + 1, 3, false, 0);
	check_op(mul, SIZE_MAX, SIZE_MAX, false, 0);
}

typedef kvec_t(uint64_t) u64_vec_t;

static void
test_kvec_growth(void)
{
	u64_vec_t v;
	size_t i, prev_m = 0;

	kv_init(v);

	/* Same capacities as the original `m * 1.5` growth */
	for (i = 0; i < 1000; i++) {
		kv_push_safe(uint64_t, v, i, enomem);

		if (kv_max(v) != prev_m) {
			assert(kv_max(v) == (prev_m > 1 ? (size_t) (prev_m * 1.5) : 2));
			prev_m = kv_max(v);
		}
	}

	for (i = 0; i < 1000; i++) {
		assert(kv_A(v, i) == i);
	}

	kv_destroy(v);

	return;
enomem:
	assert(0);
}

static void
test_kvec_resize_too_big(void)
{
	u64_vec_t v;
	uint64_t *a;

	kv_init(v);
	kv_push_safe(uint64_t, v, 1, enomem);
	a = v.a;

	kv_resize_safe(uint64_t, v, SIZE_MAX / sizeof(uint64_t) + 1, too_big);
	assert(0);

too_big:
	/* Untouched and still usable */
	assert(v.a == a && kv_size(v) == 1 && kv_max(v) == 2);
	kv_push_safe(uint64_t, v, 2, enomem);
	kv_push_safe(uint64_t, v, 3, enomem);
	assert(kv_size(v) == 3 && kv_A(v, 2) == 3);

	kv_destroy(v);

	return;
enomem:
	assert(0);
}

static void
test_kvec_grow_too_big(void)
{
	u64_vec_t v;

	/* Only the size computation runs, nothing is dereferenced */
	v.a = NULL;
	v.n = v.m = SIZE_MAX / 3 * 2 + 2;
	kv_grow_safe(uint64_t, v, too_big);
	assert(0);

too_big:
	assert(v.a == NULL && v.m == SIZE_MAX / 3 * 2 + 2);
}

static void
test_kvec_concat_too_big(void)
{
	u64_vec_t v1, v0;

	kv_init(v1);
	kv_push_safe(uint64_t, v1, 1, enomem);

	/* Only the size computation runs, nothing is dereferenced */
	v0.a = NULL;
	v0.n = v0.m = SIZE_MAX;
	kv_concat_safe(uint64_t, v1, v0, too_big);
	assert(0);

too_big:
	assert(kv_size(v1) == 1 && kv_A(v1, 0) == 1);

	kv_destroy(v1);

	return;
enomem:
	assert(0);
}

static void
test_hash_reserve(void)
{
	ucl_hash_t *h = ucl_hash_create(false);
	ucl_hash_t *hc = ucl_hash_create(true);

	assert(h != NULL && hc != NULL);

	assert(ucl_hash_reserve(h, 16));
	assert(ucl_hash_reserve(hc, 16));
	assert(!ucl_hash_reserve(h, SIZE_MAX));
	assert(!ucl_hash_reserve(hc, SIZE_MAX));
	assert(!ucl_hash_reserve(h, SIZE_MAX / 2 + 1));
	/* Doubled, this is above the largest power of two a khint_t can hold */
	assert(!ucl_hash_reserve(h, (size_t) 1 << 31));
	assert(!ucl_hash_reserve(NULL, 16));

	ucl_hash_destroy(h, NULL, NULL);
	ucl_hash_destroy(hc, NULL, NULL);
}

static void
test_hash_sizes(void)
{
	khint_t buckets = 0;
	size_t flags = 0, keys = 0, vals = 0;
	khint_t largest = (khint_t) 1 << (sizeof(khint_t) * CHAR_BIT - 1);

	assert(kh_resize_sizes(17, sizeof(void *), sizeof(void *),
		&buckets, &flags, &keys, &vals) == 0);
	assert(buckets == 32 && flags == 8);
	assert(keys == 32 * sizeof(void *) && vals == keys);
	assert(kh_resize_sizes(largest + 1, 1, 1,
		&buckets, &flags, &keys, &vals) == -1);
	/* Exercise multiplication rejection on every host without huge allocations. */
	assert(kh_resize_sizes(17, SIZE_MAX / 32 + 1, 1,
		&buckets, &flags, &keys, &vals) == -1);
	assert(kh_resize_sizes(17, 1, SIZE_MAX / 32 + 1,
		&buckets, &flags, &keys, &vals) == -1);
	assert(buckets == 32 && flags == 8 && keys == 32 * sizeof(void *));
#if SIZE_MAX == UINT32_MAX
	/* The request fits khint_t, but its rounded pointer arrays do not fit. */
	assert(kh_resize_sizes(((khint_t) 1 << 28) + 1, sizeof(void *), sizeof(void *),
		&buckets, &flags, &keys, &vals) == -1);
#endif
}

static void
test_public_reserve(void)
{
	ucl_object_t *obj = ucl_object_typed_new(UCL_OBJECT);
	ucl_object_t *copy;

	assert(obj != NULL);
	copy = ucl_object_copy(obj);
	assert(copy != NULL && ucl_object_reserve(copy, 16));
	ucl_object_unref(copy);
	assert(ucl_object_reserve(obj, 16));
	assert(!ucl_object_reserve(obj, SIZE_MAX));
	assert(ucl_object_insert_key(obj, ucl_object_fromint(42), "key", 3, true));
	assert(!ucl_object_reserve(obj, SIZE_MAX));
	assert(ucl_object_toint(ucl_object_lookup(obj, "key")) == 42);
	copy = ucl_object_copy(obj);
	assert(copy != NULL);
	assert(!ucl_object_reserve(copy, SIZE_MAX));
	ucl_object_unref(copy);
	ucl_object_unref(obj);
}

int main(void)
{
	test_add(ucl_size_add);
	test_add(ucl_size_add_portable);
	test_mul(ucl_size_mul);
	test_mul(ucl_size_mul_portable);

	test_kvec_growth();
	test_kvec_resize_too_big();
	test_kvec_grow_too_big();
	test_kvec_concat_too_big();

	test_hash_reserve();
	test_public_reserve();
	test_hash_sizes();

	return 0;
}
