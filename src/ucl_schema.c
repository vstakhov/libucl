/*
 * Copyright (c) 2014, Vsevolod Stakhov
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *	 * Redistributions of source code must retain the above copyright
 *	   notice, this list of conditions and the following disclaimer.
 *	 * Redistributions in binary form must reproduce the above copyright
 *	   notice, this list of conditions and the following disclaimer in the
 *	   documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR ''AS IS'' AND ANY
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

#include "ucl.h"
#include <stdarg.h>

static bool
ucl_string_to_type (const char *input, ucl_type_t *res)
{
	if (strcasecmp (input, "object") == 0) {
		*res = UCL_OBJECT;
	}
	else if (strcasecmp (input, "array") == 0) {
		*res = UCL_ARRAY;
	}
	else if (strcasecmp (input, "integer") == 0) {
		*res = UCL_INT;
	}
	else if (strcasecmp (input, "number") == 0) {
		*res = UCL_FLOAT;
	}
	else if (strcasecmp (input, "string") == 0) {
		*res = UCL_STRING;
	}
	else if (strcasecmp (input, "boolean") == 0) {
		*res = UCL_BOOLEAN;
	}
	else if (strcasecmp (input, "null") == 0) {
		*res = UCL_NULL;
	}
	else {
		return false;
	}

	return true;
}

static const char *
ucl_object_type_to_string (ucl_type_t type)
{
	const char *res = "unknown";

	switch (type) {
	case UCL_OBJECT:
		res = "object";
		break;
	case UCL_ARRAY:
		res = "array";
		break;
	case UCL_INT:
		res = "integer";
		break;
	case UCL_FLOAT:
	case UCL_TIME:
		res = "number";
		break;
	case UCL_STRING:
		res = "string";
		break;
	case UCL_BOOLEAN:
		res = "boolean";
		break;
	case UCL_NULL:
	case UCL_USERDATA:
		res = "null";
		break;
	}

	return res;
}

static void
ucl_schema_create_error (struct ucl_schema_error *err,
		enum ucl_schema_error_code code, ucl_object_t *obj,
		const char *fmt, ...)
{
	va_list va;

	if (err != NULL) {
		err->code = code;
		err->obj = obj;
		va_start (va, fmt);
		vsnprintf (err->msg, sizeof (err->msg), fmt, va);
		va_end (va);
	}
}

bool
ucl_object_validate (ucl_object_t *schema,
		ucl_object_t *obj, struct ucl_schema_error *err)
{
	ucl_type_t type;
	ucl_object_t *elt;
	const char *type_str;

	elt = ucl_object_find_key (schema, "type");

	/* Allow exactly one type attribute */
	if (elt == NULL || elt->next != NULL ||
			!ucl_object_tostring_safe (elt, &type_str) ||
			!ucl_string_to_type (type_str, &type)) {
		ucl_schema_create_error (err, UCL_SCHEMA_INVALID_SCHEMA, schema,
				"Type attribute is invalid in schema");
		return false;
	}

	if (obj->type != type) {
		/* Some types are actually compatible */
		if (obj->type != UCL_TIME && type != UCL_FLOAT) {
			ucl_schema_create_error (err, UCL_SCHEMA_TYPE_MISMATCH, obj,
					"Invalid type of %s, expected %s",
					ucl_object_type_to_string (obj->type),
					type);
			return false;
		}
	}

	/* XXX: to implement */
	switch (type) {
	case UCL_OBJECT:
		break;
	case UCL_ARRAY:
		break;
	case UCL_INT:
		break;
	case UCL_FLOAT:
		break;
	case UCL_STRING:
		break;
	case UCL_BOOLEAN:
		break;
	case UCL_NULL:
		break;
	default:
		break;
	}

	return false;
}
