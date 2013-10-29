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

#ifndef __UCL_HASH_H
#define __UCL_HASH_H

#include "ucl.h"

/******************************************************************************/

typedef struct ucl_hash_node_s
{
	uint32_t key;
	bool allocated;

	ucl_object_t *data;

	struct ucl_hash_node_s* prev;
	struct ucl_hash_node_s* next;
	struct ucl_hash_node_s* glob_next;
} ucl_hash_node_t;

typedef int ucl_hash_cmp_func (const void* void_a, const void* void_b);
typedef void ucl_hash_free_func (void *ptr);
typedef void* ucl_hash_iter_t;

#define UCL_HASH_INIT_BUCKETS 8

/**
 * Linear chained hashtable.
 */
typedef struct ucl_hash_struct
{
	ucl_hash_node_t *buckets; /**< array of hash buckets. One list for each hash modulus. */
	unsigned nbuckets;
	unsigned count; /**< Number of elements. */
	ucl_hash_node_t *nodes_head, *nodes_tail;
} ucl_hash_t;


/**
 * Initializes the hashtable.
 */
ucl_hash_t* ucl_hash_create (void);

/**
 * Deinitializes the hashtable.
 */
void ucl_hash_destroy (ucl_hash_t* hashlin, ucl_hash_free_func *func);

/**
 * Inserts an element in the the hashtable.
 */
void ucl_hash_insert (ucl_hash_t* hashlin, void* data,
		uint32_t hash);

/**
 * Gets the bucket of the specified hash.
 * The bucket is guaranteed to contain ALL the elements with the specified hash,
 * but it can contain also others.
 * You can access elements in the bucket following the ::next pointer until 0.
 * \param hash Hash of the element to find.
 * \return The head of the bucket, or 0 if empty.
 */
ucl_hash_node_t* ucl_hash_bucket (ucl_hash_t* hashlin, uint32_t hash);

/**
 * Searches an element in the hashtable.
 * You have to provide a compare function and the hash of the element you want to find.
 * If more equal elements are present, the first one is returned.
 * \param cmp Compare function called with cmp_arg as first argument and with the element to compare as a second one.
 * The function should return 0 for equal elements, anything other for different elements.
 * \param cmp_arg Compare argument passed as first argument of the compare function.
 * \param hash Hash of the element to find.
 * \return The first element found, or 0 if none.
 */
static inline void* ucl_hash_search (ucl_hash_t* hashlin, ucl_hash_cmp_func* cmp,
		const void* cmp_arg, uint32_t hash)
{
	ucl_hash_node_t* i;

	i = ucl_hash_bucket (hashlin, hash);

	while (i) {
		/* we first check if the hash matches, as in the same bucket we may have multiples hash values */
		if (i->key == hash && cmp (cmp_arg, i->data) == 0)
			return i->data;
		i = i->next;
	}
	return 0;
}

/**
 * Gets the number of elements.
 */
static inline unsigned ucl_hash_count (ucl_hash_t* hashlin)
{
	return hashlin->count;
}

/**
 * Iterate over hash table
 * @param hashlin hash
 * @param iter iterator (must be NULL on first iteration)
 * @return the next object
 */
void* ucl_hash_iterate (ucl_hash_t *hashlin, ucl_hash_iter_t *iter);

/**
 * Check whether an iterator has next element
 */
bool ucl_hash_iter_has_next (ucl_hash_iter_t iter);

#endif
