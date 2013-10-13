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

#ifndef RCL_H_
#define RCL_H_

#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>

#include "uthash.h"
#include "utstring.h"

/**
 * Memory allocation utilities
 * UCL_ALLOC(size) - allocate memory for UCL
 * UCL_FREE(size, ptr) - free memory of specified size at ptr
 * Default: malloc and free
 */
#ifndef UCL_ALLOC
#define UCL_ALLOC(size) malloc(size)
#endif
#ifndef UCL_FREE
#define UCL_FREE(size, ptr) free(ptr)
#endif

/**
 * @file rcl.h
 * RCL is an rspamd configuration language, which is a form of
 * JSON with less strict rules that make it more comfortable for
 * using as a configuration language
 */

enum ucl_error {
	UCL_EOK = 0,   //!< UCL_EOK
	UCL_ESYNTAX,   //!< UCL_ESYNTAX
	UCL_EIO,       //!< UCL_EIO
	UCL_ESTATE,    //!< UCL_ESTATE
	UCL_ENESTED,   //!< UCL_ENESTED
	UCL_EMACRO,    //!< UCL_EMACRO
	UCL_ERECURSION,//!< UCL_ERECURSION
	UCL_EINTERNAL, //!< UCL_EINTERNAL
	UCL_ESSL       //!< UCL_ESSL
};

enum ucl_type {
	UCL_OBJECT = 0,
	UCL_ARRAY,
	UCL_INT,
	UCL_FLOAT,
	UCL_STRING,
	UCL_BOOLEAN,
	UCL_TIME,
	UCL_USERDATA
};

enum ucl_emitter {
	UCL_EMIT_JSON = 0,
	UCL_EMIT_JSON_COMPACT,
	UCL_EMIT_CONFIG,
	UCL_EMIT_YAML
};

enum ucl_flags {
	UCL_FLAG_KEY_LOWERCASE = 0x1
};

typedef struct ucl_object_s {
	char *key;								/**< the key of an object */
	union {
		int64_t iv;							/**< int value of an object */
		char *sv;							/**< string value of an object */
		double dv;							/**< double value of an object */
		struct ucl_object_s *ov;		/**< array or hash 			*/
		void* ud;						/**< opaque user data		*/
	} value;
	enum ucl_type type;				/**< real type				*/
	int ref;								/**< reference count		*/
	struct ucl_object_s *next;		/**< array handle			*/
	UT_hash_handle hh;						/**< hash handle			*/
} ucl_object_t;


/**
 * Creates a new object
 * @return new object
 */
static inline ucl_object_t *
ucl_object_new (void)
{
	ucl_object_t *new;
	new = malloc (sizeof (ucl_object_t));
	if (new != NULL) {
		memset (new, 0, sizeof (ucl_object_t));
		new->ref = 1;
	}
	return new;
}


/**
 * Converts an object to double value
 * @param obj CL object
 * @param target target double variable
 * @return true if conversion was successful
 */
static inline bool
ucl_obj_todouble_safe (ucl_object_t *obj, double *target)
{
	if (obj == NULL) {
		return false;
	}
	switch (obj->type) {
	case UCL_INT:
		*target = obj->value.iv; /* Probaly could cause overflow */
		break;
	case UCL_FLOAT:
	case UCL_TIME:
		*target = obj->value.dv;
		break;
	default:
		return false;
	}

	return true;
}

/**
 * Unsafe version of \ref ucl_obj_todouble_safe
 * @param obj CL object
 * @return double value
 */
static inline double
ucl_obj_todouble (ucl_object_t *obj)
{
	double result = 0.;

	ucl_obj_todouble_safe (obj, &result);
	return result;
}

/**
 * Converts an object to integer value
 * @param obj CL object
 * @param target target integer variable
 * @return true if conversion was successful
 */
static inline bool
ucl_obj_toint_safe (ucl_object_t *obj, int64_t *target)
{
	if (obj == NULL) {
		return false;
	}
	switch (obj->type) {
	case UCL_INT:
		*target = obj->value.iv;
		break;
	case UCL_FLOAT:
	case UCL_TIME:
		*target = obj->value.dv; /* Loosing of decimal points */
		break;
	default:
		return false;
	}

	return true;
}

/**
 * Unsafe version of \ref ucl_obj_toint_safe
 * @param obj CL object
 * @return int value
 */
static inline int64_t
ucl_obj_toint (ucl_object_t *obj)
{
	int64_t result = 0;

	ucl_obj_toint_safe (obj, &result);
	return result;
}

/**
 * Converts an object to boolean value
 * @param obj CL object
 * @param target target boolean variable
 * @return true if conversion was successful
 */
static inline bool
ucl_obj_toboolean_safe (ucl_object_t *obj, bool *target)
{
	if (obj == NULL) {
		return false;
	}
	switch (obj->type) {
	case UCL_BOOLEAN:
		*target = (obj->value.iv == true);
		break;
	default:
		return false;
	}

	return true;
}

/**
 * Unsafe version of \ref ucl_obj_toboolean_safe
 * @param obj CL object
 * @return boolean value
 */
static inline bool
ucl_obj_toboolean (ucl_object_t *obj)
{
	bool result = false;

	ucl_obj_toboolean_safe (obj, &result);
	return result;
}

/**
 * Converts an object to string value
 * @param obj CL object
 * @param target target string variable, no need to free value
 * @return true if conversion was successful
 */
static inline bool
ucl_obj_tostring_safe (ucl_object_t *obj, const char **target)
{
	if (obj == NULL) {
		return false;
	}
	switch (obj->type) {
	case UCL_STRING:
		*target = obj->value.sv;
		break;
	default:
		return false;
	}

	return true;
}

/**
 * Unsafe version of \ref ucl_obj_tostring_safe
 * @param obj CL object
 * @return string value
 */
static inline const char *
ucl_obj_tostring (ucl_object_t *obj)
{
	const char *result = NULL;

	ucl_obj_tostring_safe (obj, &result);
	return result;
}

/**
 * Return object identified by a key in the specified object
 * @param obj object to get a key from (must be of type UCL_OBJECT)
 * @param key key to search
 * @return object matched the specified key or NULL if key is not found
 */
static inline ucl_object_t *
ucl_obj_get_key (ucl_object_t *obj, const char *key)
{
	size_t keylen;
	ucl_object_t *ret;

	if (obj == NULL || obj->type != UCL_OBJECT || key == NULL) {
		return NULL;
	}

	keylen = strlen (key);
	HASH_FIND(hh, obj->value.ov, key, keylen, ret);

	return ret;
}

/**
 * Macro handler for a parser
 * @param data the content of macro
 * @param len the length of content
 * @param ud opaque user data
 * @param err error pointer
 * @return true if macro has been parsed
 */
typedef bool (*ucl_macro_handler) (const unsigned char *data, size_t len, void* ud, UT_string **err);

/* Opaque parser */
struct ucl_parser;

/**
 * Creates new parser object
 * @param pool pool to allocate memory from
 * @return new parser object
 */
struct ucl_parser* ucl_parser_new (int flags);

/**
 * Register new handler for a macro
 * @param parser parser object
 * @param macro macro name (without leading dot)
 * @param handler handler (it is called immediately after macro is parsed)
 * @param ud opaque user data for a handler
 */
void ucl_parser_register_macro (struct ucl_parser *parser, const char *macro,
		ucl_macro_handler handler, void* ud);

/**
 * Load new chunk to a parser
 * @param parser parser structure
 * @param data the pointer to the beginning of a chunk
 * @param len the length of a chunk
 * @param err if *err is NULL it is set to parser error
 * @return true if chunk has been added and false in case of error
 */
bool ucl_parser_add_chunk (struct ucl_parser *parser, const unsigned char *data,
		size_t len, UT_string **err);

/**
 * Load and add data from a file
 * @param parser parser structure
 * @param filename the name of file
 * @param err if *err is NULL it is set to parser error
 * @return true if chunk has been added and false in case of error
 */
bool ucl_parser_add_file (struct ucl_parser *parser, const char *filename,
		UT_string **err);

/**
 * Get a top object for a parser
 * @param parser parser structure
 * @param err if *err is NULL it is set to parser error
 * @return top parser object or NULL
 */
ucl_object_t* ucl_parser_get_object (struct ucl_parser *parser, UT_string **err);

/**
 * Free cl parser object
 * @param parser parser object
 */
void ucl_parser_free (struct ucl_parser *parser);

/**
 * Free cl object
 * @param obj cl object to free
 */
void ucl_obj_free (ucl_object_t *obj);

/**
 * Icrease reference count for an object
 * @param obj object to ref
 */
static inline ucl_object_t *
ucl_obj_ref (ucl_object_t *obj) {
	obj->ref ++;
	return obj;
}

/**
 * Decrease reference count for an object
 * @param obj object to unref
 */
static inline void
ucl_obj_unref (ucl_object_t *obj) {
	if (--obj->ref <= 0) {
		ucl_obj_free (obj);
	}
}

/**
 * Emit object to a string
 * @param obj object
 * @param emit_type if type is UCL_EMIT_JSON then emit json, if type is
 * UCL_EMIT_CONFIG then emit config like object
 * @return dump of an object (must be freed after using) or NULL in case of error
 */
unsigned char *ucl_object_emit (ucl_object_t *obj, enum ucl_emitter emit_type);

/**
 * Add new public key to parser for signatures check
 * @param parser parser object
 * @param key PEM representation of a key
 * @param len length of the key
 * @param err if *err is NULL it is set to parser error
 * @return true if a key has been successfully added
 */
bool ucl_pubkey_add (struct ucl_parser *parser, const unsigned char *key, size_t len, UT_string **err);

#endif /* RCL_H_ */
