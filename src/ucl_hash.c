/* Copyright (c) 2013, Vsevolod Stakhov
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

#include "ucl_internal.h"
#include "ucl_hash.h"
#include "khash.h"
#include "kvec.h"

struct ucl_hash_struct {
	void *hash;
	kvec_t(ucl_object_t *) ar;
	bool caseless;
};

static inline uint32_t
ucl_hash_func (const ucl_object_t *o)
{
	return XXH32 (o->key, o->keylen, 0xdeadbeef);
}

static inline int
ucl_hash_equal (const ucl_object_t *k1, const ucl_object_t *k2)
{
	if (k1->keylen == k2->keylen) {
		return strncmp (k1->key, k2->key, k1->keylen) == 0;
	}

	return 0;
}

KHASH_INIT (ucl_hash_node, const ucl_object_t *, const ucl_object_t *, 1,
		ucl_hash_func, ucl_hash_equal);

static inline uint32_t
ucl_hash_caseless_func (const ucl_object_t *o)
{
	void *xxh = XXH32_init (0xdeadbeef);
	char hash_buf[64], *c;
	const char *p;
	ssize_t remain = o->keylen;

	p = o->key;
	c = &hash_buf[0];

	while (remain > 0) {
		*c++ = tolower (*p++);

		if (c - &hash_buf[0] == sizeof (hash_buf)) {
			XXH32_update (xxh, hash_buf, sizeof (hash_buf));
			c = &hash_buf[0];
		}
		remain --;
	}

	if (c - &hash_buf[0] != 0) {
		XXH32_update (xxh, hash_buf, c - &hash_buf[0]);
	}

	return XXH32_digest (xxh);
}

static inline int
ucl_hash_caseless_equal (const ucl_object_t *k1, const ucl_object_t *k2)
{
	if (k1->keylen == k2->keylen) {
		return strncasecmp (k1->key, k2->key, k1->keylen) == 0;
	}

	return 0;
}

KHASH_INIT (ucl_hash_caseless_node, const ucl_object_t *, const ucl_object_t *, 1,
		ucl_hash_caseless_func, ucl_hash_caseless_equal);

ucl_hash_t*
ucl_hash_create (bool ignore_case)
{
	ucl_hash_t *new;

	new = UCL_ALLOC (sizeof (ucl_hash_t));
	if (new != NULL) {
		kv_init (new->ar);

		new->caseless = ignore_case;
		if (ignore_case) {
			khash_t(ucl_hash_caseless_node) *h = kh_init (ucl_hash_caseless_node);
			new->hash = (void *)h;
		}
		else {
			khash_t(ucl_hash_node) *h = kh_init (ucl_hash_node);
			new->hash = (void *)h;
		}
	}
	return new;
}

void ucl_hash_destroy (ucl_hash_t* hashlin, ucl_hash_free_func *func)
{
	if (func != NULL) {
		/* Iterate over the hash first */
		khash_t(ucl_hash_node) *h = (khash_t(ucl_hash_node) *)
				hashlin->hash;
		khiter_t k;

		for (k = kh_begin (h); k != kh_end (h); ++k) {
			if (kh_exist (h, k)) {
				func (__DECONST (ucl_object_t *, kh_value (h, k)));
			}
		}
	}

	if (hashlin->caseless) {
		khash_t(ucl_hash_caseless_node) *h = (khash_t(ucl_hash_caseless_node) *)
			hashlin->hash;
		kh_destroy (ucl_hash_caseless_node, h);
	}
	else {
		khash_t(ucl_hash_node) *h = (khash_t(ucl_hash_node) *)
			hashlin->hash;
		kh_destroy (ucl_hash_node, h);
	}
}

void
ucl_hash_insert (ucl_hash_t* hashlin, const ucl_object_t *obj,
		const char *key, unsigned keylen)
{
	khiter_t k;
	int ret;

	if (hashlin->caseless) {
		khash_t(ucl_hash_caseless_node) *h = (khash_t(ucl_hash_caseless_node) *)
				hashlin->hash;
		k = kh_put (ucl_hash_caseless_node, h, obj, &ret);
		if (ret > 0) {
			kh_value (h, k) = obj;
		}
	}
	else {
		khash_t(ucl_hash_node) *h = (khash_t(ucl_hash_node) *)
				hashlin->hash;
		k = kh_put (ucl_hash_node, h, obj, &ret);
		if (ret > 0) {
			kh_value (h, k) = obj;
		}
	}
}

void ucl_hash_replace (ucl_hash_t* hashlin, const ucl_object_t *old,
		const ucl_object_t *new)
{
	khiter_t k;
	int ret;

	if (hashlin->caseless) {
		khash_t(ucl_hash_caseless_node) *h = (khash_t(ucl_hash_caseless_node) *)
				hashlin->hash;
		k = kh_put (ucl_hash_caseless_node, h, old, &ret);
		if (ret == 0) {
			kh_value (h, k) = new;
		}
	}
	else {
		khash_t(ucl_hash_node) *h = (khash_t(ucl_hash_node) *)
				hashlin->hash;
		k = kh_put (ucl_hash_node, h, old, &ret);
		if (ret == 0) {
			kh_value (h, k) = new;
		}
	}
}

const void*
ucl_hash_iterate (ucl_hash_t *hashlin, ucl_hash_iter_t *iter)
{
	khiter_t it = (khiter_t)(uintptr_t)(*iter);
	const ucl_object_t *ret = NULL;

	/*
	 * For khash NULL means hash start, so we can here assume the same
	 */
	if (hashlin->caseless) {
		khash_t(ucl_hash_caseless_node) *h = (khash_t(ucl_hash_caseless_node) *)
			hashlin->hash;

		while (it != kh_end (h)) {
			if (kh_exist (h, it)) {
				ret = kh_val (h, it);
				it ++;
				break;
			}
			it ++;
		}
	}
	else {
		khash_t(ucl_hash_node) *h = (khash_t(ucl_hash_node) *)
			hashlin->hash;

		while (it != kh_end (h)) {
			if (kh_exist (h, it)) {
				ret = kh_val (h, it);
				it ++;
				break;
			}
			it ++;
		}
	}

	*iter = (void *)(uintptr_t)it;

	return ret;
}

bool
ucl_hash_iter_has_next (ucl_hash_t *hashlin, ucl_hash_iter_t iter)
{
	khiter_t it = (khiter_t)(uintptr_t)(iter);
	khash_t(ucl_hash_node) *h = (khash_t(ucl_hash_node) *)
			hashlin->hash;

	return (it + 1) < kh_end (h);
}


const ucl_object_t*
ucl_hash_search (ucl_hash_t* hashlin, const char *key, unsigned keylen)
{
	khiter_t k;
	const ucl_object_t *ret = NULL;
	ucl_object_t search;

	search.key = key;
	search.keylen = keylen;

	if (hashlin->caseless) {
		khash_t(ucl_hash_caseless_node) *h = (khash_t(ucl_hash_caseless_node) *)
						hashlin->hash;

		k = kh_get (ucl_hash_caseless_node, h, &search);
		if (k != kh_end (h)) {
			ret = kh_value (h, k);
		}
	}
	else {
		khash_t(ucl_hash_node) *h = (khash_t(ucl_hash_node) *)
						hashlin->hash;
		k = kh_get (ucl_hash_node, h, &search);
		if (k != kh_end (h)) {
			ret = kh_value (h, k);
		}
	}

	return ret;
}

void
ucl_hash_delete (ucl_hash_t* hashlin, const ucl_object_t *obj)
{
	khiter_t k;

	if (hashlin->caseless) {
		khash_t(ucl_hash_caseless_node) *h = (khash_t(ucl_hash_caseless_node) *)
			hashlin->hash;

		k = kh_get (ucl_hash_caseless_node, h, obj);
		if (k != kh_end (h)) {
			kh_del (ucl_hash_caseless_node, h, k);
		}
	}
	else {
		khash_t(ucl_hash_node) *h = (khash_t(ucl_hash_node) *)
			hashlin->hash;
		k = kh_get (ucl_hash_node, h, obj);
		if (k != kh_end (h)) {
			kh_del (ucl_hash_node, h, k);
		}
	}
}
