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

#ifndef UCL_H_
#define UCL_H_

#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef _WIN32
# define UCL_EXTERN __declspec(dllexport)
#else
# define UCL_EXTERN
#endif

/**
 * @mainpage
 * This is a reference manual for UCL API. You may find the description of UCL format by following this
 * [github repository](https://github.com/vstakhov/libucl).
 *
 * This manual has several main sections:
 *  - @ref structures
 *  - @ref utils
 *  - @ref parser
 *  - @ref emitter
 */

/**
 * @file ucl.h
 * @brief UCL parsing and emitting functions
 *
 * UCL is universal configuration language, which is a form of
 * JSON with less strict rules that make it more comfortable for
 * using as a configuration language
 */
#ifdef  __cplusplus
extern "C" {
#endif
/*
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

#if    __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
#define UCL_WARN_UNUSED_RESULT               \
  __attribute__((warn_unused_result))
#else
#define UCL_WARN_UNUSED_RESULT
#endif

#ifdef __GNUC__
#define UCL_DEPRECATED(func) func __attribute__ ((deprecated))
#elif defined(_MSC_VER)
#define UCL_DEPRECATED(func) __declspec(deprecated) func
#else
#define UCL_DEPRECATED(func) func
#endif

/**
 * @defgroup structures Structures and types
 * UCL defines several enumeration types used for error reporting or specifying flags and attributes.
 *
 * @{
 */

/**
 * The common error codes returned by ucl parser
 */
typedef enum ucl_error {
	UCL_EOK = 0, /**< No error */
	UCL_ESYNTAX, /**< Syntax error occurred during parsing */
	UCL_EIO, /**< IO error occurred during parsing */
	UCL_ESTATE, /**< Invalid state machine state */
	UCL_ENESTED, /**< Input has too many recursion levels */
	UCL_EMACRO, /**< Error processing a macro */
	UCL_EINTERNAL, /**< Internal unclassified error */
	UCL_ESSL /**< SSL error */
} ucl_error_t;

/**
 * #ucl_object_t may have one of specified types, some types are compatible with each other and some are not.
 * For example, you can always convert #UCL_TIME to #UCL_FLOAT. Also you can convert #UCL_FLOAT to #UCL_INTEGER
 * by loosing floating point. Every object may be converted to a string by #ucl_object_tostring_forced() function.
 *
 */
typedef enum ucl_type {
	UCL_OBJECT = 0, /**< UCL object - key/value pairs */
	UCL_ARRAY, /**< UCL array */
	UCL_INT, /**< Integer number */
	UCL_FLOAT, /**< Floating point number */
	UCL_STRING, /**< Null terminated string */
	UCL_BOOLEAN, /**< Boolean value */
	UCL_TIME, /**< Time value (floating point number of seconds) */
	UCL_USERDATA, /**< Opaque userdata pointer (may be used in macros) */
	UCL_NULL /**< Null value */
} ucl_type_t;

/**
 * You can use one of these types to serialise #ucl_object_t by using ucl_object_emit().
 */
typedef enum ucl_emitter {
	UCL_EMIT_JSON = 0, /**< Emit fine formatted JSON */
	UCL_EMIT_JSON_COMPACT, /**< Emit compacted JSON */
	UCL_EMIT_CONFIG, /**< Emit human readable config format */
	UCL_EMIT_YAML /**< Emit embedded YAML format */
} ucl_emitter_t;

/**
 * These flags defines parser behaviour. If you specify #UCL_PARSER_ZEROCOPY you must ensure
 * that the input memory is not freed if an object is in use. Moreover, if you want to use
 * zero-terminated keys and string values then you should not use zero-copy mode, as in this case
 * UCL still has to perform copying implicitly.
 */
typedef enum ucl_parser_flags {
	UCL_PARSER_KEY_LOWERCASE = 0x1, /**< Convert all keys to lower case */
	UCL_PARSER_ZEROCOPY = 0x2, /**< Parse input in zero-copy mode if possible */
	UCL_PARSER_NO_TIME = 0x4 /**< Do not parse time and treat time values as strings */
} ucl_parser_flags_t;

/**
 * String conversion flags, that are used in #ucl_object_fromstring_common function.
 */
typedef enum ucl_string_flags {
	UCL_STRING_ESCAPE = 0x1,  /**< Perform JSON escape */
	UCL_STRING_TRIM = 0x2,    /**< Trim leading and trailing whitespaces */
	UCL_STRING_PARSE_BOOLEAN = 0x4,    /**< Parse passed string and detect boolean */
	UCL_STRING_PARSE_INT = 0x8,    /**< Parse passed string and detect integer number */
	UCL_STRING_PARSE_DOUBLE = 0x10,    /**< Parse passed string and detect integer or float number */
	UCL_STRING_PARSE_TIME = 0x20, /**< Parse time strings */
	UCL_STRING_PARSE_NUMBER =  UCL_STRING_PARSE_INT|UCL_STRING_PARSE_DOUBLE|UCL_STRING_PARSE_TIME,  /**<
									Parse passed string and detect number */
	UCL_STRING_PARSE =  UCL_STRING_PARSE_BOOLEAN|UCL_STRING_PARSE_NUMBER,   /**<
									Parse passed string (and detect booleans and numbers) */
	UCL_STRING_PARSE_BYTES = 0x40  /**< Treat numbers as bytes */
} ucl_string_flags_t;

/**
 * Basic flags for an object
 */
typedef enum ucl_object_flags {
	UCL_OBJECT_ALLOCATED_KEY = 0x1, /**< An object has key allocated internally */
	UCL_OBJECT_ALLOCATED_VALUE = 0x2, /**< An object has a string value allocated internally */
	UCL_OBJECT_NEED_KEY_ESCAPE = 0x4, /**< The key of an object need to be escaped on output */
	UCL_OBJECT_EPHEMERAL = 0x8, /**< Temporary object that does not need to be freed really */
	UCL_OBJECT_MULTILINE = 0x16 /**< String should be displayed as multiline string */
} ucl_object_flags_t;

/**
 * UCL object structure. Please mention that the most of fields should not be touched by
 * UCL users. In future, this structure may be converted to private one.
 */
typedef struct ucl_object_s {
	/**
	 * Variant value type
	 */
	union {
		int64_t iv;							/**< Int value of an object */
		const char *sv;					/**< String value of an object */
		double dv;							/**< Double value of an object */
		struct ucl_object_s *av;			/**< Array					*/
		void *ov;							/**< Object					*/
		void* ud;							/**< Opaque user data		*/
	} value;
	const char *key;						/**< Key of an object		*/
	struct ucl_object_s *next;				/**< Array handle			*/
	struct ucl_object_s *prev;				/**< Array handle			*/
	uint32_t keylen;						/**< Lenght of a key		*/
	uint32_t len;							/**< Size of an object		*/
	uint32_t ref;							/**< Reference count		*/
	uint16_t flags;							/**< Object flags			*/
	uint16_t type;							/**< Real type				*/
	unsigned char* trash_stack[2];			/**< Pointer to allocated chunks */
} ucl_object_t;

/**
 * Destructor type for userdata objects
 * @param ud user specified data pointer
 */
typedef void (*ucl_userdata_dtor)(void *ud);
typedef const char* (*ucl_userdata_emitter)(void *ud);

/** @} */

/**
 * @defgroup utils Utility functions
 * A number of utility functions simplify handling of UCL objects
 *
 * @{
 */
/**
 * Copy and return a key of an object, returned key is zero-terminated
 * @param obj CL object
 * @return zero terminated key
 */
UCL_EXTERN char* ucl_copy_key_trash (const ucl_object_t *obj);

/**
 * Copy and return a string value of an object, returned key is zero-terminated
 * @param obj CL object
 * @return zero terminated string representation of object value
 */
UCL_EXTERN char* ucl_copy_value_trash (const ucl_object_t *obj);

/**
 * Creates a new object
 * @return new object
 */
UCL_EXTERN ucl_object_t* ucl_object_new (void) UCL_WARN_UNUSED_RESULT;

/**
 * Create new object with type specified
 * @param type type of a new object
 * @return new object
 */
UCL_EXTERN ucl_object_t* ucl_object_typed_new (ucl_type_t type) UCL_WARN_UNUSED_RESULT;

/**
 * Create new object with type and priority specified
 * @param type type of a new object
 * @param priority priority of an object
 * @return new object
 */
UCL_EXTERN ucl_object_t* ucl_object_new_full (ucl_type_t type, unsigned priority)
	UCL_WARN_UNUSED_RESULT;

/**
 * Create new object with userdata dtor
 * @param dtor destructor function
 * @return new object
 */
UCL_EXTERN ucl_object_t* ucl_object_new_userdata (ucl_userdata_dtor dtor,
		ucl_userdata_emitter emitter) UCL_WARN_UNUSED_RESULT;

/**
 * Perform deep copy of an object copying everything
 * @param other object to copy
 * @return new object with refcount equal to 1
 */
UCL_EXTERN ucl_object_t * ucl_object_copy (const ucl_object_t *other)
	UCL_WARN_UNUSED_RESULT;

/**
 * Return the type of an object
 * @return the object type
 */
UCL_EXTERN ucl_type_t ucl_object_type (const ucl_object_t *obj);

/**
 * Convert any string to an ucl object making the specified transformations
 * @param str fixed size or NULL terminated string
 * @param len length (if len is zero, than str is treated as NULL terminated)
 * @param flags conversion flags
 * @return new object
 */
UCL_EXTERN ucl_object_t * ucl_object_fromstring_common (const char *str, size_t len,
		enum ucl_string_flags flags) UCL_WARN_UNUSED_RESULT;

/**
 * Create a UCL object from the specified string
 * @param str NULL terminated string, will be json escaped
 * @return new object
 */
UCL_EXTERN ucl_object_t *ucl_object_fromstring (const char *str) UCL_WARN_UNUSED_RESULT;

/**
 * Create a UCL object from the specified string
 * @param str fixed size string, will be json escaped
 * @param len length of a string
 * @return new object
 */
UCL_EXTERN ucl_object_t *ucl_object_fromlstring (const char *str,
		size_t len) UCL_WARN_UNUSED_RESULT;

/**
 * Create an object from an integer number
 * @param iv number
 * @return new object
 */
UCL_EXTERN ucl_object_t* ucl_object_fromint (int64_t iv) UCL_WARN_UNUSED_RESULT;

/**
 * Create an object from a float number
 * @param dv number
 * @return new object
 */
UCL_EXTERN ucl_object_t* ucl_object_fromdouble (double dv) UCL_WARN_UNUSED_RESULT;

/**
 * Create an object from a boolean
 * @param bv bool value
 * @return new object
 */
UCL_EXTERN ucl_object_t* ucl_object_frombool (bool bv) UCL_WARN_UNUSED_RESULT;

/**
 * Insert a object 'elt' to the hash 'top' and associate it with key 'key'
 * @param top destination object (will be created automatically if top is NULL)
 * @param elt element to insert (must NOT be NULL)
 * @param key key to associate with this object (either const or preallocated)
 * @param keylen length of the key (or 0 for NULL terminated keys)
 * @param copy_key make an internal copy of key
 * @return true if key has been inserted
 */
UCL_EXTERN bool ucl_object_insert_key (ucl_object_t *top, ucl_object_t *elt,
		const char *key, size_t keylen, bool copy_key);

/**
 * Replace a object 'elt' to the hash 'top' and associate it with key 'key', old object will be unrefed,
 * if no object has been found this function works like ucl_object_insert_key()
 * @param top destination object (will be created automatically if top is NULL)
 * @param elt element to insert (must NOT be NULL)
 * @param key key to associate with this object (either const or preallocated)
 * @param keylen length of the key (or 0 for NULL terminated keys)
 * @param copy_key make an internal copy of key
 * @return true if key has been inserted
 */
UCL_EXTERN bool ucl_object_replace_key (ucl_object_t *top, ucl_object_t *elt,
		const char *key, size_t keylen, bool copy_key);

/**
 * Delete a object associated with key 'key', old object will be unrefered,
 * @param top object
 * @param key key associated to the object to remove
 * @param keylen length of the key (or 0 for NULL terminated keys)
 */
UCL_EXTERN bool ucl_object_delete_keyl (ucl_object_t *top,
		const char *key, size_t keylen);

/**
 * Delete a object associated with key 'key', old object will be unrefered,
 * @param top object
 * @param key key associated to the object to remove
 */
UCL_EXTERN bool ucl_object_delete_key (ucl_object_t *top,
		const char *key);


/**
 * Delete key from `top` object returning the object deleted. This object is not
 * released
 * @param top object
 * @param key key to remove
 * @param keylen length of the key (or 0 for NULL terminated keys)
 * @return removed object or NULL if object has not been found
 */
UCL_EXTERN ucl_object_t* ucl_object_pop_keyl (ucl_object_t *top, const char *key,
		size_t keylen) UCL_WARN_UNUSED_RESULT;

/**
 * Delete key from `top` object returning the object deleted. This object is not
 * released
 * @param top object
 * @param key key to remove
 * @return removed object or NULL if object has not been found
 */
UCL_EXTERN ucl_object_t* ucl_object_pop_key (ucl_object_t *top, const char *key)
	UCL_WARN_UNUSED_RESULT;

/**
 * Insert a object 'elt' to the hash 'top' and associate it with key 'key', if the specified key exist,
 * try to merge its content
 * @param top destination object (will be created automatically if top is NULL)
 * @param elt element to insert (must NOT be NULL)
 * @param key key to associate with this object (either const or preallocated)
 * @param keylen length of the key (or 0 for NULL terminated keys)
 * @param copy_key make an internal copy of key
 * @return true if key has been inserted
 */
UCL_EXTERN bool ucl_object_insert_key_merged (ucl_object_t *top, ucl_object_t *elt,
		const char *key, size_t keylen, bool copy_key);

/**
 * Append an element to the front of array object
 * @param top destination object (will be created automatically if top is NULL)
 * @param elt element to append (must NOT be NULL)
 * @return true if value has been inserted
 */
UCL_EXTERN bool ucl_array_append (ucl_object_t *top,
		ucl_object_t *elt);

/**
 * Append an element to the start of array object
 * @param top destination object (will be created automatically if top is NULL)
 * @param elt element to append (must NOT be NULL)
 * @return true if value has been inserted
 */
UCL_EXTERN bool ucl_array_prepend (ucl_object_t *top,
		ucl_object_t *elt);

/**
 * Removes an element `elt` from the array `top`. Caller must unref the returned object when it is not
 * needed.
 * @param top array ucl object
 * @param elt element to remove
 * @return removed element or NULL if `top` is NULL or not an array
 */
UCL_EXTERN ucl_object_t* ucl_array_delete (ucl_object_t *top,
		ucl_object_t *elt);

/**
 * Returns the first element of the array `top`
 * @param top array ucl object
 * @return element or NULL if `top` is NULL or not an array
 */
UCL_EXTERN const ucl_object_t* ucl_array_head (const ucl_object_t *top);

/**
 * Returns the last element of the array `top`
 * @param top array ucl object
 * @return element or NULL if `top` is NULL or not an array
 */
UCL_EXTERN const ucl_object_t* ucl_array_tail (const ucl_object_t *top);

/**
 * Removes the last element from the array `top`. Caller must unref the returned object when it is not
 * needed.
 * @param top array ucl object
 * @return removed element or NULL if `top` is NULL or not an array
 */
UCL_EXTERN ucl_object_t* ucl_array_pop_last (ucl_object_t *top);

/**
 * Return object identified by an index of the array `top`
 * @param obj object to get a key from (must be of type UCL_ARRAY)
 * @param index index to return
 * @return object at the specified index or NULL if index is not found
 */
UCL_EXTERN const ucl_object_t* ucl_array_find_index (const ucl_object_t *top,
		unsigned int index);

/**
 * Removes the first element from the array `top`. Caller must unref the returned object when it is not
 * needed.
 * @param top array ucl object
 * @return removed element or NULL if `top` is NULL or not an array
 */
UCL_EXTERN ucl_object_t* ucl_array_pop_first (ucl_object_t *top);

/**
 * Append a element to another element forming an implicit array
 * @param head head to append (may be NULL)
 * @param elt new element
 * @return true if element has been inserted
 */
UCL_EXTERN ucl_object_t * ucl_elt_append (ucl_object_t *head,
		ucl_object_t *elt);

/**
 * Converts an object to double value
 * @param obj CL object
 * @param target target double variable
 * @return true if conversion was successful
 */
UCL_EXTERN bool ucl_object_todouble_safe (const ucl_object_t *obj, double *target);

/**
 * Unsafe version of \ref ucl_obj_todouble_safe
 * @param obj CL object
 * @return double value
 */
UCL_EXTERN double ucl_object_todouble (const ucl_object_t *obj);

/**
 * Converts an object to integer value
 * @param obj CL object
 * @param target target integer variable
 * @return true if conversion was successful
 */
UCL_EXTERN bool ucl_object_toint_safe (const ucl_object_t *obj, int64_t *target);

/**
 * Unsafe version of \ref ucl_obj_toint_safe
 * @param obj CL object
 * @return int value
 */
UCL_EXTERN int64_t ucl_object_toint (const ucl_object_t *obj);

/**
 * Converts an object to boolean value
 * @param obj CL object
 * @param target target boolean variable
 * @return true if conversion was successful
 */
UCL_EXTERN bool ucl_object_toboolean_safe (const ucl_object_t *obj, bool *target);

/**
 * Unsafe version of \ref ucl_obj_toboolean_safe
 * @param obj CL object
 * @return boolean value
 */
UCL_EXTERN bool ucl_object_toboolean (const ucl_object_t *obj);

/**
 * Converts an object to string value
 * @param obj CL object
 * @param target target string variable, no need to free value
 * @return true if conversion was successful
 */
UCL_EXTERN bool ucl_object_tostring_safe (const ucl_object_t *obj, const char **target);

/**
 * Unsafe version of \ref ucl_obj_tostring_safe
 * @param obj CL object
 * @return string value
 */
UCL_EXTERN const char* ucl_object_tostring (const ucl_object_t *obj);

/**
 * Convert any object to a string in JSON notation if needed
 * @param obj CL object
 * @return string value
 */
UCL_EXTERN const char* ucl_object_tostring_forced (const ucl_object_t *obj);

/**
 * Return string as char * and len, string may be not zero terminated, more efficient that \ref ucl_obj_tostring as it
 * allows zero-copy (if #UCL_PARSER_ZEROCOPY has been used during parsing)
 * @param obj CL object
 * @param target target string variable, no need to free value
 * @param tlen target length
 * @return true if conversion was successful
 */
UCL_EXTERN bool ucl_object_tolstring_safe (const ucl_object_t *obj,
		const char **target, size_t *tlen);

/**
 * Unsafe version of \ref ucl_obj_tolstring_safe
 * @param obj CL object
 * @return string value
 */
UCL_EXTERN const char* ucl_object_tolstring (const ucl_object_t *obj, size_t *tlen);

/**
 * Return object identified by a key in the specified object
 * @param obj object to get a key from (must be of type UCL_OBJECT)
 * @param key key to search
 * @return object matched the specified key or NULL if key is not found
 */
UCL_EXTERN const ucl_object_t* ucl_object_find_key (const ucl_object_t *obj,
		const char *key);

/**
 * Return object identified by a fixed size key in the specified object
 * @param obj object to get a key from (must be of type UCL_OBJECT)
 * @param key key to search
 * @param klen length of a key
 * @return object matched the specified key or NULL if key is not found
 */
UCL_EXTERN const ucl_object_t* ucl_object_find_keyl (const ucl_object_t *obj,
		const char *key, size_t klen);

/**
 * Return object identified by dot notation string
 * @param obj object to search in
 * @param path dot.notation.path to the path to lookup. May use numeric .index on arrays
 * @return object matched the specified path or NULL if path is not found
 */
UCL_EXTERN const ucl_object_t *ucl_lookup_path (const ucl_object_t *obj,
		const char *path);

/**
 * Returns a key of an object as a NULL terminated string
 * @param obj CL object
 * @return key or NULL if there is no key
 */
UCL_EXTERN const char* ucl_object_key (const ucl_object_t *obj);

/**
 * Returns a key of an object as a fixed size string (may be more efficient)
 * @param obj CL object
 * @param len target key length
 * @return key pointer
 */
UCL_EXTERN const char* ucl_object_keyl (const ucl_object_t *obj, size_t *len);

/**
 * Increase reference count for an object
 * @param obj object to ref
 */
UCL_EXTERN ucl_object_t* ucl_object_ref (const ucl_object_t *obj);

/**
 * Free ucl object
 * @param obj ucl object to free
 */
UCL_DEPRECATED(UCL_EXTERN void ucl_object_free (ucl_object_t *obj));

/**
 * Decrease reference count for an object
 * @param obj object to unref
 */
UCL_EXTERN void ucl_object_unref (ucl_object_t *obj);

/**
 * Compare objects `o1` and `o2`
 * @param o1 the first object
 * @param o2 the second object
 * @return values >0, 0 and <0 if `o1` is more than, equal and less than `o2`.
 * The order of comparison:
 * 1) Type of objects
 * 2) Size of objects
 * 3) Content of objects
 */
UCL_EXTERN int ucl_object_compare (const ucl_object_t *o1,
		const ucl_object_t *o2);

/**
 * Sort UCL array using `cmp` compare function
 * @param ar
 * @param cmp
 */
UCL_EXTERN void ucl_object_array_sort (ucl_object_t *ar,
		int (*cmp)(const ucl_object_t *o1, const ucl_object_t *o2));

/**
 * Opaque iterator object
 */
typedef void* ucl_object_iter_t;

/**
 * Get next key from an object
 * @param obj object to iterate
 * @param iter opaque iterator, must be set to NULL on the first call:
 * ucl_object_iter_t it = NULL;
 * while ((cur = ucl_iterate_object (obj, &it)) != NULL) ...
 * @return the next object or NULL
 */
UCL_EXTERN const ucl_object_t* ucl_iterate_object (const ucl_object_t *obj,
		ucl_object_iter_t *iter, bool expand_values);
/** @} */


/**
 * @defgroup parser Parsing functions
 * These functions are used to parse UCL objects
 *
 * @{
 */

/**
 * Macro handler for a parser
 * @param data the content of macro
 * @param len the length of content
 * @param arguments arguments object
 * @param ud opaque user data
 * @param err error pointer
 * @return true if macro has been parsed
 */
typedef bool (*ucl_macro_handler) (const unsigned char *data, size_t len,
		const ucl_object_t *arguments,
		void* ud);

/* Opaque parser */
struct ucl_parser;

/**
 * Creates new parser object
 * @param pool pool to allocate memory from
 * @return new parser object
 */
UCL_EXTERN struct ucl_parser* ucl_parser_new (int flags);

/**
 * Register new handler for a macro
 * @param parser parser object
 * @param macro macro name (without leading dot)
 * @param handler handler (it is called immediately after macro is parsed)
 * @param ud opaque user data for a handler
 */
UCL_EXTERN void ucl_parser_register_macro (struct ucl_parser *parser, const char *macro,
		ucl_macro_handler handler, void* ud);

/**
 * Handler to detect unregistered variables
 * @param data variable data
 * @param len length of variable
 * @param replace (out) replace value for variable
 * @param replace_len (out) replace length for variable
 * @param need_free (out) UCL will free `dest` after usage
 * @param ud opaque userdata
 * @return true if variable
 */
typedef bool (*ucl_variable_handler) (const unsigned char *data, size_t len,
		unsigned char **replace, size_t *replace_len, bool *need_free, void* ud);

/**
 * Register new parser variable
 * @param parser parser object
 * @param var variable name
 * @param value variable value
 */
UCL_EXTERN void ucl_parser_register_variable (struct ucl_parser *parser, const char *var,
		const char *value);

/**
 * Set handler for unknown variables
 * @param parser parser structure
 * @param handler desired handler
 * @param ud opaque data for the handler
 */
UCL_EXTERN void ucl_parser_set_variables_handler (struct ucl_parser *parser,
		ucl_variable_handler handler, void *ud);

/**
 * Load new chunk to a parser
 * @param parser parser structure
 * @param data the pointer to the beginning of a chunk
 * @param len the length of a chunk
 * @return true if chunk has been added and false in case of error
 */
UCL_EXTERN bool ucl_parser_add_chunk (struct ucl_parser *parser,
		const unsigned char *data, size_t len);

/**
 * Load new chunk to a parser with the specified priority
 * @param parser parser structure
 * @param data the pointer to the beginning of a chunk
 * @param len the length of a chunk
 * @param priority the desired priority of a chunk (only 4 least significant bits
 * are considered for this parameter)
 * @return true if chunk has been added and false in case of error
 */
UCL_EXTERN bool ucl_parser_add_chunk_priority (struct ucl_parser *parser,
		const unsigned char *data, size_t len, unsigned priority);

/**
 * Load ucl object from a string
 * @param parser parser structure
 * @param data the pointer to the string
 * @param len the length of the string, if `len` is 0 then `data` must be zero-terminated string
 * @return true if string has been added and false in case of error
 */
UCL_EXTERN bool ucl_parser_add_string (struct ucl_parser *parser,
		const char *data,size_t len);

/**
 * Load and add data from a file
 * @param parser parser structure
 * @param filename the name of file
 * @param err if *err is NULL it is set to parser error
 * @return true if chunk has been added and false in case of error
 */
UCL_EXTERN bool ucl_parser_add_file (struct ucl_parser *parser,
		const char *filename);

/**
 * Load and add data from a file descriptor
 * @param parser parser structure
 * @param filename the name of file
 * @param err if *err is NULL it is set to parser error
 * @return true if chunk has been added and false in case of error
 */
UCL_EXTERN bool ucl_parser_add_fd (struct ucl_parser *parser,
		int fd);

/**
 * Get a top object for a parser (refcount is increased)
 * @param parser parser structure
 * @param err if *err is NULL it is set to parser error
 * @return top parser object or NULL
 */
UCL_EXTERN ucl_object_t* ucl_parser_get_object (struct ucl_parser *parser);

/**
 * Get the error string if failing
 * @param parser parser object
 */
UCL_EXTERN const char *ucl_parser_get_error(struct ucl_parser *parser);
/**
 * Free ucl parser object
 * @param parser parser object
 */
UCL_EXTERN void ucl_parser_free (struct ucl_parser *parser);

/**
 * Add new public key to parser for signatures check
 * @param parser parser object
 * @param key PEM representation of a key
 * @param len length of the key
 * @param err if *err is NULL it is set to parser error
 * @return true if a key has been successfully added
 */
UCL_EXTERN bool ucl_pubkey_add (struct ucl_parser *parser, const unsigned char *key, size_t len);

/**
 * Set FILENAME and CURDIR variables in parser
 * @param parser parser object
 * @param filename filename to set or NULL to set FILENAME to "undef" and CURDIR to getcwd()
 * @param need_expand perform realpath() if this variable is true and filename is not NULL
 * @return true if variables has been set
 */
UCL_EXTERN bool ucl_parser_set_filevars (struct ucl_parser *parser, const char *filename,
		bool need_expand);

/** @} */

/**
 * @defgroup emitter Emitting functions
 * These functions are used to serialise UCL objects to some string representation.
 *
 * @{
 */

struct ucl_emitter_context;
/**
 * Structure using for emitter callbacks
 */
struct ucl_emitter_functions {
	/** Append a single character */
	int (*ucl_emitter_append_character) (unsigned char c, size_t nchars, void *ud);
	/** Append a string of a specified length */
	int (*ucl_emitter_append_len) (unsigned const char *str, size_t len, void *ud);
	/** Append a 64 bit integer */
	int (*ucl_emitter_append_int) (int64_t elt, void *ud);
	/** Append floating point element */
	int (*ucl_emitter_append_double) (double elt, void *ud);
	/** Free userdata */
	void (*ucl_emitter_free_func)(void *ud);
	/** Opaque userdata pointer */
	void *ud;
};

struct ucl_emitter_operations {
	/** Write a primitive element */
	void (*ucl_emitter_write_elt) (struct ucl_emitter_context *ctx,
		const ucl_object_t *obj, bool first, bool print_key);
	/** Start ucl object */
	void (*ucl_emitter_start_object) (struct ucl_emitter_context *ctx,
		const ucl_object_t *obj, bool print_key);
	/** End ucl object */
	void (*ucl_emitter_end_object) (struct ucl_emitter_context *ctx,
		const ucl_object_t *obj);
	/** Start ucl array */
	void (*ucl_emitter_start_array) (struct ucl_emitter_context *ctx,
		const ucl_object_t *obj, bool print_key);
	void (*ucl_emitter_end_array) (struct ucl_emitter_context *ctx,
		const ucl_object_t *obj);
};

/**
 * Structure that defines emitter functions
 */
struct ucl_emitter_context {
	/** Name of emitter (e.g. json, compact_json) */
	const char *name;
	/** Unique id (e.g. UCL_EMIT_JSON for standard emitters */
	int id;
	/** A set of output functions */
	const struct ucl_emitter_functions *func;
	/** A set of output operations */
	const struct ucl_emitter_operations *ops;
	/** Current amount of indent tabs */
	unsigned int indent;
	/** Top level object */
	const ucl_object_t *top;
	/** The rest of context */
	unsigned char data[1];
};

/**
 * Emit object to a string
 * @param obj object
 * @param emit_type if type is #UCL_EMIT_JSON then emit json, if type is
 * #UCL_EMIT_CONFIG then emit config like object
 * @return dump of an object (must be freed after using) or NULL in case of error
 */
UCL_EXTERN unsigned char *ucl_object_emit (const ucl_object_t *obj,
		enum ucl_emitter emit_type);

/**
 * Emit object to a string
 * @param obj object
 * @param emit_type if type is #UCL_EMIT_JSON then emit json, if type is
 * #UCL_EMIT_CONFIG then emit config like object
 * @param emitter a set of emitter functions
 * @return dump of an object (must be freed after using) or NULL in case of error
 */
UCL_EXTERN bool ucl_object_emit_full (const ucl_object_t *obj,
		enum ucl_emitter emit_type,
		struct ucl_emitter_functions *emitter);

/**
 * Start streamlined UCL object emitter
 * @param obj top UCL object
 * @param emit_type emit type
 * @param emitter a set of emitter functions
 * @return new streamlined context that should be freed by
 * `ucl_object_emit_streamline_finish`
 */
UCL_EXTERN struct ucl_emitter_context* ucl_object_emit_streamline_new (
		const ucl_object_t *obj, enum ucl_emitter emit_type,
		struct ucl_emitter_functions *emitter);

/**
 * Start object or array container for the streamlined output
 * @param ctx streamlined context
 * @param obj container object
 */
UCL_EXTERN void ucl_object_emit_streamline_start_container (
		struct ucl_emitter_context *ctx, const ucl_object_t *obj);
/**
 * Add a complete UCL object to streamlined output
 * @param ctx streamlined context
 * @param obj object to output
 */
UCL_EXTERN void ucl_object_emit_streamline_add_object (
		struct ucl_emitter_context *ctx, const ucl_object_t *obj);
/**
 * End previously added container
 * @param ctx streamlined context
 */
UCL_EXTERN void ucl_object_emit_streamline_end_container (
		struct ucl_emitter_context *ctx);
/**
 * Terminate streamlined container finishing all containers in it
 * @param ctx streamlined context
 */
UCL_EXTERN void ucl_object_emit_streamline_finish (
		struct ucl_emitter_context *ctx);

/**
 * Returns functions to emit object to memory
 * @param pmem target pointer (should be freed by caller)
 * @return emitter functions structure
 */
UCL_EXTERN struct ucl_emitter_functions* ucl_object_emit_memory_funcs (
		void **pmem);

/**
 * Returns functions to emit object to FILE *
 * @param fp FILE * object
 * @return emitter functions structure
 */
UCL_EXTERN struct ucl_emitter_functions* ucl_object_emit_file_funcs (
		FILE *fp);
/**
 * Returns functions to emit object to a file descriptor
 * @param fd file descriptor
 * @return emitter functions structure
 */
UCL_EXTERN struct ucl_emitter_functions* ucl_object_emit_fd_funcs (
		int fd);

/**
 * Free emitter functions
 * @param f pointer to functions
 */
UCL_EXTERN void ucl_object_emit_funcs_free (struct ucl_emitter_functions *f);

/** @} */

/**
 * @defgroup schema Schema functions
 * These functions are used to validate UCL objects using json schema format
 *
 * @{
 */

/**
 * Used to define UCL schema error
 */
enum ucl_schema_error_code {
	UCL_SCHEMA_OK = 0,          /**< no error */
	UCL_SCHEMA_TYPE_MISMATCH,   /**< type of object is incorrect */
	UCL_SCHEMA_INVALID_SCHEMA,  /**< schema is invalid */
	UCL_SCHEMA_MISSING_PROPERTY,/**< one or more missing properties */
	UCL_SCHEMA_CONSTRAINT,      /**< constraint found */
	UCL_SCHEMA_MISSING_DEPENDENCY, /**< missing dependency */
	UCL_SCHEMA_UNKNOWN          /**< generic error */
};

/**
 * Generic ucl schema error
 */
struct ucl_schema_error {
	enum ucl_schema_error_code code;	/**< error code */
	char msg[128];						/**< error message */
	const ucl_object_t *obj;			/**< object where error occured */
};

/**
 * Validate object `obj` using schema object `schema`.
 * @param schema schema object
 * @param obj object to validate
 * @param err error pointer, if this parameter is not NULL and error has been
 * occured, then `err` is filled with the exact error definition.
 * @return true if `obj` is valid using `schema`
 */
UCL_EXTERN bool ucl_object_validate (const ucl_object_t *schema,
		const ucl_object_t *obj, struct ucl_schema_error *err);

/** @} */

#ifdef  __cplusplus
}
#endif
/*
 * XXX: Poorly named API functions, need to replace them with the appropriate
 * named function. All API functions *must* use naming ucl_object_*. Usage of
 * ucl_obj* should be avoided.
 */
#define ucl_obj_todouble_safe ucl_object_todouble_safe
#define ucl_obj_todouble ucl_object_todouble
#define ucl_obj_tostring ucl_object_tostring
#define ucl_obj_tostring_safe ucl_object_tostring_safe
#define ucl_obj_tolstring ucl_object_tolstring
#define ucl_obj_tolstring_safe ucl_object_tolstring_safe
#define ucl_obj_toint ucl_object_toint
#define ucl_obj_toint_safe ucl_object_toint_safe
#define ucl_obj_toboolean ucl_object_toboolean
#define ucl_obj_toboolean_safe ucl_object_toboolean_safe
#define ucl_obj_get_key ucl_object_find_key
#define ucl_obj_get_keyl ucl_object_find_keyl
#define ucl_obj_unref ucl_object_unref
#define ucl_obj_ref ucl_object_ref
#define ucl_obj_free ucl_object_free

#endif /* UCL_H_ */
