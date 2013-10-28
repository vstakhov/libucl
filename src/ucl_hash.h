/*
 * Copyright 2010 Andrea Mazzoleni. All rights reserved.
 * Copyright 2013 Vsevolod Stakhov. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY ANDREA MAZZOLENI AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL ANDREA MAZZOLENI OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/** \file
 * Linear chained hashtable.
 *
 * This hashtable resizes dynamically and progressively using a variation of the
 * linear hashing algorithm described in http://en.wikipedia.org/wiki/Linear_hashing
 *
 * It starts with the minimal size of 16 buckets, it doubles the size then it
 * reaches a load factor greater than 0.5 and it halves the size with a load
 * factor lower than 0.125.
 *
 * The progressive resize is good for real-time and interactive applications
 * as it makes insert and delete operations taking always the same time.
 *
 * For resizing it's used a dynamic array that supports access to not contigous
 * segments.
 * In this way we only allocate additional table segments on the heap, without
 * freeing the previous table, and then not increasing the heap fragmentation.
 *
 * The resize takes place inside ucl_hash_insert() and ucl_hash_remove().
 * No resize is done in the ucl_hash_search() operation.
 *
 * To initialize the hashtable you have to call ucl_hash_init().
 *
 * \code
 * ucl_hashslin hashlin;
 *
 * ucl_hash_init(&hashlin);
 * \endcode
 *
 * To insert elements in the hashtable you have to call ucl_hash_insert() for
 * each element.
 * In the insertion call you have to specify the address of the node, the
 * address of the object, and the hash value of the key to use.
 * The address of the object is used to initialize the ucl_node::data field
 * of the node, and the hash to initialize the ucl_node::key field.
 *
 * \code
 * struct object {
 *     ucl_node node;
 *     // other fields
 *     int value;
 * };
 *
 * struct object* obj = malloc(sizeof(struct object)); // creates the object
 *
 * obj->value = ...; // initializes the object
 *
 * ucl_hash_insert(&hashlin, &obj->node, obj, ucl_inthash_u32(obj->value)); // inserts the object
 * \endcode
 *
 * To find and element in the hashtable you have to call ucl_hashtable_search()
 * providing a comparison function, its argument, and the hash of the key to search.
 *
 * \code
 * int compare(const void* arg, const void* obj)
 * {
 *     return (*(const unsigned*)arg != ((const struct object*)obj)->value;
 * }
 *
 * struct object* obj = ucl_hash_search(&hashlin, compare, &value_to_find, ucl_inthash_u32(value_to_find));
 * if (!obj) {
 *     // not found
 * } else {
 *     // found
 * }
 * \endcode
 *
 * To iterate over all the elements in the hashtable with the same key, you have to
 * use ucl_hash_bucket() and follow the ucl_node::next pointer until NULL.
 * You have also to check explicitely for the key, as the bucket may contains
 * different keys.
 *
 * \code
 * ucl_node* i = ucl_hash_bucket(&hashlin, ucl_inthash_u32(value_to_find));
 * while (i) {
 *     struct object* obj = i->data; // gets the object pointer
 *
 *     if (obj->value == value_to_find) {
 *         printf("%d\n", obj->value); // process the object
 *     }
 *
 *     i = i->next; // goes to the next element
 * }
 * \endcode
 *
 * To remove an element from the hashtable you have to call ucl_hash_remove()
 * providing a comparison function, its argument, and the hash of the key to search
 * and remove.
 *
 * \code
 * struct object* obj = ucl_trie_remove(&hashtable, compare, &value_to_remove, ucl_inthash_u32(value_to_remove));
 * if (obj) {
 *     free(obj); // frees the object allocated memory
 * }
 * \endcode
 *
 * To destroy the hashtable you have to remove all the elements, and deinitialize
 * the hashtable calling ucl_hash_done().
 *
 * \code
 * ucl_hash_done(&hashlin);
 * \endcode
 *
 * Note that you cannot iterates over all the elements in the hashtable using the
 * hashtable itself. You have to insert all the elements also in a ::ucl_list,
 * and use the list to iterate. See the \ref multiindex example for more detail.  
 */

#ifndef __UCL_HASH_H
#define __UCL_HASH_H

#include "ucl.h"

/******************************************************************************/
/* hashlin */

/** \internal
 * Initial and minimal size of the hashtable expressed as a power of 2.
 * The initial size is 2^UCL_HASHLIN_BIT.
 */
#define UCL_HASHLIN_BIT 6

/**
 * Linear hashtable node.
 * This is the node that you have to include inside your objects.
 */
typedef struct ucl_hash_node_s
{
	/**
	 * Next node.
	 * The tail node has it at 0, like a 0 terminated list.
	 */
	struct ucl_hash_node_s* next;

	/**
	 * Previous node.
	 * The head node points to the tail node, like a circular list.
	 */
	struct ucl_hash_node_s* prev;

	/**
	 * The object containing the node.
	 * This field is initialized when inserting nodes into a data structure.
	 */
	void *data;

	/**
	 * Key used to store the node.
	 * With hashtables this field is used to store the hash value.
	 * With lists this field is not used.
	 */
	uint32_t key;
} ucl_hash_node_t;
/** \internal
 * Max number of elements as a power of 2.
 */
#define UCL_HASHLIN_BIT_MAX 32

typedef int ucl_hash_cmp_func (const void* void_a, const void* void_b);

/**
 * Linear chained hashtable.
 */
typedef struct ucl_hash_struct
{
	ucl_hash_node_t** bucket[UCL_HASHLIN_BIT_MAX]; /**< Dynamic array of hash buckets. One list for each hash modulus. */
	unsigned bucket_bit; /**< Bits used in the bit mask. */
	unsigned bucket_max; /**< Number of buckets. */
	unsigned bucket_mask; /**< Bit mask to access the buckets. */
	unsigned bucket_mac; /**< Number of vectors allocated. */
	unsigned low_max; /**< Low order max value. */
	unsigned low_mask; /**< Low order mask value. */
	unsigned split; /**< Split position. */
	unsigned state; /**< Reallocation state. */
	unsigned count; /**< Number of elements. */
} ucl_hash_t;

/**
 * Initializes the hashtable.
 */
ucl_hash_t* ucl_hash_create (void);

/**
 * Deinitializes the hashtable.
 */
void ucl_hash_destroy (ucl_hash_t* hashlin);

/**
 * Inserts an element in the the hashtable.
 */
void ucl_hash_insert_hash (ucl_hash_t* hashlin, ucl_hash_node_t* node, void* data,
		uint32_t hash);

/**
 * Searches and removes an element from the hashtable.
 * You have to provide a compare function and the hash of the element you want to remove.
 * If the element is not found, 0 is returned.
 * If more equal elements are present, the first one is removed.
 * This operation is faster than calling ucl_hash_bucket() and ucl_hash_remove_existing() separately.
 * \param cmp Compare function called with cmp_arg as first argument and with the element to compare as a second one.
 * The function should return 0 for equal elements, anything other for different elements.
 * \param cmp_arg Compare argument passed as first argument of the compare function.
 * \param hash Hash of the element to find and remove.
 * \return The removed element, or 0 if not found.
 */
void* ucl_hash_remove (ucl_hash_t* hashlin, ucl_hash_cmp_func* cmp,
		const void* cmp_arg, uint32_t hash);

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
	ucl_hash_node_t* i = ucl_hash_bucket (hashlin, hash);
	while (i) {
		/* we first check if the hash matches, as in the same bucket we may have multiples hash values */
		if (i->key == hash && cmp (cmp_arg, i->data) == 0)
			return i->data;
		i = i->next;
	}
	return 0;
}

/**
 * Removes an element from the hashtable.
 * You must already have the address of the element to remove.
 * \return The ucl_node::data field of the node removed.
 */
void* ucl_hash_remove_existing (ucl_hash_t* hashlin, ucl_hash_node_t* node);

/**
 * Gets the number of elements.
 */
static inline unsigned ucl_hash_count (ucl_hash_t* hashlin)
{
	return hashlin->count;
}

/**
 * Gets the size of allocated memory.
 * It includes the size of the ::ucl_hash_node of the stored elements.
 */
size_t ucl_hash_memory_usage (ucl_hash_t* hashlin);

#endif

