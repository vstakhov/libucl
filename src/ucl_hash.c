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

#include "ucl_hash.h"
#include "utlist.h"

static ucl_hash_node_t* ucl_hash_insert_bucket (ucl_hash_t* hashlin,
		ucl_hash_node_t *bucket, ucl_hash_node_t *node);

ucl_hash_t*
ucl_hash_create (void)
{
	ucl_hash_t *new;

	new = UCL_ALLOC (sizeof (ucl_hash_t));
	if (new != NULL) {
		/* fixed initial size */
		new->nbuckets = UCL_HASH_INIT_BUCKETS;
		new->buckets = UCL_ALLOC (sizeof (ucl_hash_node_t) * UCL_HASH_INIT_BUCKETS);
		memset (new->buckets, 0, sizeof (ucl_hash_node_t) * UCL_HASH_INIT_BUCKETS);
		new->nodes_head = new->nodes_tail = NULL;
		new->count = 0;
	}
	return new;
}

void ucl_hash_destroy (ucl_hash_t* hashlin, ucl_hash_free_func *func)
{
	ucl_hash_node_t *elt, *tmp, *bucket;
	unsigned i;

	LL_FOREACH_SAFE2 (hashlin->nodes_head, elt, tmp, glob_next) {
		if (elt->allocated) {
			if (func != NULL && elt->data != NULL) {
				func (elt->data);
			}
			UCL_FREE (sizeof (ucl_hash_node_t), elt);
		}
	}
	UCL_FREE (hashlin->bucket_max * sizeof(ucl_hash_node_t*), hashlin->buckets);
	UCL_FREE (sizeof (ucl_hash_t), hashlin);

}


/**
 * Return the bucket to use.
 */
static inline ucl_hash_node_t*
ucl_hash_bucket_ptr (ucl_hash_t* hashlin,
		uint32_t hash)
{
	unsigned pos;

	pos = hash & (hashlin->nbuckets - 1);

	return &hashlin->buckets[pos];
}

static ucl_hash_node_t*
ucl_hash_insert_bucket (ucl_hash_t* hashlin, ucl_hash_node_t *bucket,
		ucl_hash_node_t* node)
{

	if (bucket->data == NULL) {
		/* Got hit */
		bucket->prev = bucket;
		bucket->next = NULL;
		if (node != NULL && node->allocated) {
			UCL_FREE (sizeof (ucl_hash_node_t), node);
		}
		node = bucket;
	}
	else {
		if (node == NULL) {
			node = UCL_ALLOC (sizeof (ucl_hash_node_t));
			node->allocated = true;
		}
		DL_APPEND (bucket, node);
	}

	return node;
}

static void
ucl_hash_insert_glob (ucl_hash_t* hashlin, ucl_hash_node_t *node) {
	if (hashlin->nodes_tail != NULL) {
		hashlin->nodes_tail->glob_next = node;
		hashlin->nodes_tail = node;
	}
	else {
		hashlin->nodes_tail = node;
		hashlin->nodes_head = node;
	}
	node->glob_next = NULL;
}

static void
ucl_hash_resize (ucl_hash_t* hashlin)
{
	ucl_hash_node_t *new_buckets, *bucket, *cur, *tmp1, *tmp2, *head, *node;
	unsigned new_count, pos, i, new_bit;

	printf ("RESIZE!!\n");
	new_count = hashlin->nbuckets << 1;
	new_buckets = UCL_ALLOC (sizeof (ucl_hash_node_t) * new_count);
	new_bit = new_count - 1;
	if (new_buckets != NULL) {
		memset (new_buckets, 0, sizeof (ucl_hash_node_t) * new_count);
	}

	head = hashlin->nodes_head;
	tmp1 = head;

	LL_FOREACH2 (head, cur, glob_next) {
		tmp2 = cur->next;
		pos = cur->key & new_bit;
		node = ucl_hash_insert_bucket (hashlin, &new_buckets[pos], cur);
		if (node != cur) {
			/* Need to replace pointer */
			if (cur == head) {
				hashlin->nodes_head = node;
			}
			else if (cur == hashlin->nodes_tail) {
				hashlin->nodes_tail = node;
			}
			node->glob_next = tmp2;
			tmp1->glob_next = node;
			cur = node;
		}
		tmp1 = cur;
	}
	UCL_FREE (sizeof (ucl_hash_node_t) * hashlin->nbuckets, hashlin->buckets);
	hashlin->nbuckets = new_count;
	hashlin->buckets = new_buckets;
}

static void
ucl_hash_maybe_resize (ucl_hash_t* hashlin)
{
	if (hashlin->nbuckets <= hashlin->count + hashlin->count / 16) {
		//ucl_hash_resize (hashlin);
	}
}

void
ucl_hash_insert (ucl_hash_t* hashlin, void* data, uint32_t hash)
{
	ucl_hash_node_t* pbucket, *node;
	pbucket = ucl_hash_bucket_ptr (hashlin, hash);

	node = ucl_hash_insert_bucket (hashlin, pbucket, NULL);
	ucl_hash_insert_glob (hashlin, node);
	node->data = data;
	node->key = hash;
	++hashlin->count;
	ucl_hash_maybe_resize (hashlin);
}

ucl_hash_node_t*
ucl_hash_bucket (ucl_hash_t* hashlin, uint32_t hash)
{
	return ucl_hash_bucket_ptr (hashlin, hash);
}

void*
ucl_hash_iterate (ucl_hash_t *hashlin, ucl_hash_iter_t *iter)
{
	ucl_hash_node_t *elt = *iter;

	if (elt == NULL) {
		elt = hashlin->nodes_head;
		if (elt == NULL) {
			return NULL;
		}
	}
	else if (elt == hashlin->nodes_head) {
		return NULL;
	}

	*iter = elt->glob_next ? elt->glob_next : hashlin->nodes_head;
	return elt->data;
}

bool
ucl_hash_iter_has_next (ucl_hash_iter_t iter)
{
	ucl_hash_node_t *elt = iter;

	return (elt != NULL && elt->glob_next != NULL);
}
