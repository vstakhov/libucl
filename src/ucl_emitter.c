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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ucl.h"
#include "ucl_internal.h"
#include "ucl_chartable.h"
#ifdef HAVE_FLOAT_H
#include <float.h>
#endif
#ifdef HAVE_MATH_H
#include <math.h>
#endif

/**
 * @file rcl_emitter.c
 * Serialise UCL object to various of output formats
 */

static void ucl_emitter_common_elt (struct ucl_emitter_context *ctx,
		const ucl_object_t *obj, bool first, bool print_key, bool compact);

#define UCL_EMIT_TYPE_OPS(type)		\
	static void ucl_emit_ ## type ## _elt (struct ucl_emitter_context *ctx,	\
		const ucl_object_t *obj, bool first, bool print_key);	\
	static void ucl_emit_ ## type ## _start_obj (struct ucl_emitter_context *ctx,	\
		const ucl_object_t *obj);	\
	static void ucl_emit_ ## type## _start_array (struct ucl_emitter_context *ctx,	\
		const ucl_object_t *obj);	\
	static void ucl_emit_ ##type## _end_object (struct ucl_emitter_context *ctx,	\
		const ucl_object_t *obj);	\
	static void ucl_emit_ ##type## _end_array (struct ucl_emitter_context *ctx,	\
		const ucl_object_t *obj)

/*
 * JSON format operations
 */
UCL_EMIT_TYPE_OPS(json);
UCL_EMIT_TYPE_OPS(json_compact);
UCL_EMIT_TYPE_OPS(config);
UCL_EMIT_TYPE_OPS(yaml);

#define UCL_EMIT_TYPE_CONTENT(type) {	\
	.ucl_emitter_write_elt = ucl_emit_ ## type ## _elt,	\
	.ucl_emitter_start_object = ucl_emit_ ## type ##_start_obj,	\
	.ucl_emitter_start_array = ucl_emit_ ## type ##_start_array,	\
	.ucl_emitter_end_object = ucl_emit_ ## type ##_end_object,	\
	.ucl_emitter_end_array = ucl_emit_ ## type ##_end_array	\
}


static const struct ucl_emitter_operations ucl_standartd_emitter_ops[] = {
	[UCL_EMIT_JSON] = UCL_EMIT_TYPE_CONTENT(json),
	[UCL_EMIT_JSON_COMPACT] = UCL_EMIT_TYPE_CONTENT(json_compact),
	[UCL_EMIT_CONFIG] = UCL_EMIT_TYPE_CONTENT(config),
	[UCL_EMIT_YAML] = UCL_EMIT_TYPE_CONTENT(yaml)
};

static const struct ucl_emitter_context ucl_standard_emitters[] = {
	[UCL_EMIT_JSON] = {
		.name = "json",
		.id = UCL_EMIT_JSON,
		.func = NULL,
		.ops = &ucl_standartd_emitter_ops[UCL_EMIT_JSON]
	},
	[UCL_EMIT_JSON_COMPACT] = {
		.name = "json_compact",
		.id = UCL_EMIT_JSON_COMPACT,
		.func = NULL,
		.ops = &ucl_standartd_emitter_ops[UCL_EMIT_JSON_COMPACT]
	},
	[UCL_EMIT_CONFIG] = {
		.name = "config",
		.id = UCL_EMIT_CONFIG,
		.func = NULL,
		.ops = &ucl_standartd_emitter_ops[UCL_EMIT_CONFIG]
	},
	[UCL_EMIT_YAML] = {
		.name = "yaml",
		.id = UCL_EMIT_YAML,
		.func = NULL,
		.ops = &ucl_standartd_emitter_ops[UCL_EMIT_YAML]
	}
};

/*
 * Utility to check whether we need a top object
 */
#define UCL_EMIT_IDENT_TOP_OBJ(ctx, obj) ((ctx)->top != (obj) || \
		((ctx)->id == UCL_EMIT_JSON_COMPACT || (ctx)->id == UCL_EMIT_JSON))

/**
 * Add tabulation to the output buffer
 * @param buf target buffer
 * @param tabs number of tabs to add
 */
static inline void
ucl_add_tabs (const struct ucl_emitter_functions *func, unsigned int tabs,
		bool compact)
{
	if (!compact && tabs > 0) {
		func->ucl_emitter_append_character (' ', tabs * 4, func->ud);
	}
}

/**
 * Serialise string
 * @param str string to emit
 * @param buf target buffer
 */
static void
ucl_elt_string_write_json (const char *str, size_t size,
		struct ucl_emitter_context *ctx)
{
	const char *p = str, *c = str;
	size_t len = 0;
	const struct ucl_emitter_functions *func = ctx->func;

	if (ctx->id != UCL_EMIT_YAML) {
		func->ucl_emitter_append_character ('"', 1, func->ud);
	}

	while (size) {
		if (ucl_test_character (*p, UCL_CHARACTER_JSON_UNSAFE)) {
			if (len > 0) {
				func->ucl_emitter_append_len (c, len, func->ud);
			}
			switch (*p) {
			case '\n':
				func->ucl_emitter_append_len ("\\n", 2, func->ud);
				break;
			case '\r':
				func->ucl_emitter_append_len ("\\r", 2, func->ud);
				break;
			case '\b':
				func->ucl_emitter_append_len ("\\b", 2, func->ud);
				break;
			case '\t':
				func->ucl_emitter_append_len ("\\t", 2, func->ud);
				break;
			case '\f':
				func->ucl_emitter_append_len ("\\f", 2, func->ud);
				break;
			case '\\':
				func->ucl_emitter_append_len ("\\\\", 2, func->ud);
				break;
			case '"':
				func->ucl_emitter_append_len ("\\\"", 2, func->ud);
				break;
			}
			len = 0;
			c = ++p;
		}
		else {
			p ++;
			len ++;
		}
		size --;
	}
	if (len > 0) {
		func->ucl_emitter_append_len (c, len, func->ud);
	}
	if (ctx->id != UCL_EMIT_YAML) {
		func->ucl_emitter_append_character ('"', 1, func->ud);
	}
}

/**
 * End standard ucl object
 * @param ctx emitter context
 * @param compact compact flag
 */
static void
ucl_emitter_common_end_object (struct ucl_emitter_context *ctx,
		const ucl_object_t *obj, bool compact)
{
	const struct ucl_emitter_functions *func = ctx->func;

	if (UCL_EMIT_IDENT_TOP_OBJ(ctx, obj)) {
		ctx->ident --;
		if (compact) {
			func->ucl_emitter_append_character ('}', 1, func->ud);
		}
		else {
			if (ctx->id != UCL_EMIT_CONFIG) {
				/* newline is already added for this format */
				func->ucl_emitter_append_character ('\n', 1, func->ud);
			}
			ucl_add_tabs (func, ctx->ident, compact);
			func->ucl_emitter_append_character ('}', 1, func->ud);
		}
	}
}

/**
 * End standard ucl array
 * @param ctx emitter context
 * @param compact compact flag
 */
static void
ucl_emitter_common_end_array (struct ucl_emitter_context *ctx,
		const ucl_object_t *obj, bool compact)
{
	const struct ucl_emitter_functions *func = ctx->func;

	ctx->ident --;
	if (compact) {
		func->ucl_emitter_append_character (']', 1, func->ud);
	}
	else {
		if (ctx->id != UCL_EMIT_CONFIG) {
			/* newline is already added for this format */
			func->ucl_emitter_append_character ('\n', 1, func->ud);
		}
		ucl_add_tabs (func, ctx->ident, compact);
		func->ucl_emitter_append_character (']', 1, func->ud);
	}
}

/**
 * Start emit standard UCL array
 * @param ctx emitter context
 * @param obj object to write
 * @param compact compact flag
 */
static void
ucl_emitter_common_start_array (struct ucl_emitter_context *ctx,
		const ucl_object_t *obj, bool compact)
{
	const ucl_object_t *cur = obj;
	const struct ucl_emitter_functions *func = ctx->func;
	bool first = true;

	if (compact) {
		func->ucl_emitter_append_character ('[', 1, func->ud);
	}
	else {
		func->ucl_emitter_append_len ("[\n", 2, func->ud);
	}

	ctx->ident ++;

	while (cur) {
		ucl_emitter_common_elt (ctx, cur, first, false, compact);
		first = false;
		cur = cur->next;
	}
}

/**
 * Start emit standard UCL object
 * @param ctx emitter context
 * @param obj object to write
 * @param compact compact flag
 */
static void
ucl_emitter_common_start_object (struct ucl_emitter_context *ctx,
		const ucl_object_t *obj, bool compact)
{
	ucl_hash_iter_t it = NULL;
	const ucl_object_t *cur, *elt;
	const struct ucl_emitter_functions *func = ctx->func;
	bool first = true;

	/*
	 * Print <ident_level>{
	 * <ident_level + 1><object content>
	 */
	if (UCL_EMIT_IDENT_TOP_OBJ(ctx, obj)) {
		if (compact) {
			func->ucl_emitter_append_character ('{', 1, func->ud);
		}
		else {
			func->ucl_emitter_append_len ("{\n", 2, func->ud);
		}
		ctx->ident ++;
	}

	while ((cur = ucl_hash_iterate (obj->value.ov, &it))) {

		if (ctx->id == UCL_EMIT_CONFIG) {
			LL_FOREACH (cur, elt) {
				ucl_emitter_common_elt (ctx, elt, first, true, compact);
			}
		}
		else {
			/* Expand implicit arrays */
			if (cur->next != NULL) {
				if (!first) {
					if (compact) {
						func->ucl_emitter_append_character (',', 1, func->ud);
					}
					else {
						func->ucl_emitter_append_len (",\n", 2, func->ud);
					}
				}
				ucl_add_tabs (func, ctx->ident, compact);
				if (cur->keylen > 0) {
					ucl_elt_string_write_json (cur->key, cur->keylen, ctx);
				}
				else {
					func->ucl_emitter_append_len ("null", 4, func->ud);
				}

				if (compact) {
					func->ucl_emitter_append_character (':', 1, func->ud);
				}
				else {
					func->ucl_emitter_append_len (": ", 2, func->ud);
				}
				ucl_emitter_common_start_array (ctx, cur, compact);
				ucl_emitter_common_end_array (ctx, cur, compact);
			}
			else {
				ucl_emitter_common_elt (ctx, cur, first, true, compact);
			}
		}

		first = false;
	}
}

/**
 * Common choice of object emitting
 * @param ctx emitter context
 * @param obj object to print
 * @param first flag to mark the first element
 * @param print_key print key of an object
 * @param compact compact output
 */
static void
ucl_emitter_common_elt (struct ucl_emitter_context *ctx,
		const ucl_object_t *obj, bool first, bool print_key, bool compact)
{
	const struct ucl_emitter_functions *func = ctx->func;
	bool flag;

	if (ctx->id != UCL_EMIT_CONFIG && !first) {
		if (compact) {
			func->ucl_emitter_append_character (',', 1, func->ud);
		}
		else {
			func->ucl_emitter_append_len (",\n", 2, func->ud);
		}
	}

	ucl_add_tabs (func, ctx->ident, compact);

	if (print_key) {
		if (ctx->id == UCL_EMIT_CONFIG) {
			if (obj->flags & UCL_OBJECT_NEED_KEY_ESCAPE) {
				ucl_elt_string_write_json (obj->key, obj->keylen, ctx);
			}
			else {
				func->ucl_emitter_append_len (obj->key, obj->keylen, func->ud);
			}

			if (obj->type != UCL_OBJECT && obj->type != UCL_ARRAY) {
				func->ucl_emitter_append_len (" = ", 3, func->ud);
			}
			else {
				func->ucl_emitter_append_character (' ', 1, func->ud);
			}
		}
		else {
			if (obj->keylen > 0) {
				ucl_elt_string_write_json (obj->key, obj->keylen, ctx);
			}
			else {
				func->ucl_emitter_append_len ("null", 4, func->ud);
			}

			if (compact) {
				func->ucl_emitter_append_character (':', 1, func->ud);
			}
			else {
				func->ucl_emitter_append_len (": ", 2, func->ud);
			}
		}
	}

	switch (obj->type) {
	case UCL_INT:
		func->ucl_emitter_append_int (ucl_object_toint (obj), func->ud);
		break;
	case UCL_FLOAT:
	case UCL_TIME:
		func->ucl_emitter_append_double (ucl_object_todouble (obj), func->ud);
		break;
	case UCL_BOOLEAN:
		flag = ucl_object_toboolean (obj);
		if (flag) {
			func->ucl_emitter_append_len ("true", 4, func->ud);
		}
		else {
			func->ucl_emitter_append_len ("false", 5, func->ud);
		}
		break;
	case UCL_STRING:
		ucl_elt_string_write_json (obj->value.sv, obj->len, ctx);
		break;
	case UCL_NULL:
		func->ucl_emitter_append_len ("null", 4, func->ud);
		break;
	case UCL_OBJECT:
		ucl_emitter_common_start_object (ctx, obj, compact);
		ucl_emitter_common_end_object (ctx, obj, compact);
		break;
	case UCL_ARRAY:
		ucl_emitter_common_start_array (ctx, obj->value.av, compact);
		ucl_emitter_common_end_array (ctx, obj->value.av, compact);
		break;
	case UCL_USERDATA:
		break;
	}

	if (ctx->id == UCL_EMIT_CONFIG && obj != ctx->top) {
		if (obj->type != UCL_OBJECT && obj->type != UCL_ARRAY) {
			if (print_key) {
				/* Objects are split by ';' */
				func->ucl_emitter_append_len (";\n", 2, func->ud);
			}
			else {
				/* Use commas for arrays */
				func->ucl_emitter_append_len (",\n", 2, func->ud);
			}
		}
		else {
			func->ucl_emitter_append_character ('\n', 1, func->ud);
		}
	}
}

/*
 * Specific standard implementations of the emitter functions
 */
#define UCL_EMIT_TYPE_IMPL(type, compact)		\
	static void ucl_emit_ ## type ## _elt (struct ucl_emitter_context *ctx,	\
		const ucl_object_t *obj, bool first, bool print_key) {	\
		ucl_emitter_common_elt (ctx, obj, first, print_key, (compact));	\
	}	\
	static void ucl_emit_ ## type ## _start_obj (struct ucl_emitter_context *ctx,	\
		const ucl_object_t *obj) {	\
		ucl_emitter_common_start_object (ctx, obj, (compact));	\
	}	\
	static void ucl_emit_ ## type## _start_array (struct ucl_emitter_context *ctx,	\
		const ucl_object_t *obj) {	\
		ucl_emitter_common_start_array (ctx, obj, (compact));	\
	}	\
	static void ucl_emit_ ##type## _end_object (struct ucl_emitter_context *ctx,	\
		const ucl_object_t *obj) {	\
		ucl_emitter_common_end_object (ctx, obj, (compact));	\
	}	\
	static void ucl_emit_ ##type## _end_array (struct ucl_emitter_context *ctx,	\
		const ucl_object_t *obj) {	\
		ucl_emitter_common_end_array (ctx, obj, (compact));	\
	}

UCL_EMIT_TYPE_IMPL(json, false);
UCL_EMIT_TYPE_IMPL(json_compact, true);
UCL_EMIT_TYPE_IMPL(config, false);
UCL_EMIT_TYPE_IMPL(yaml, false);

/*
 * Generic utstring output
 */
static int
ucl_utstring_append_character (unsigned char c, size_t len, void *ud)
{
	UT_string *buf = ud;

	if (len == 1) {
		utstring_append_c (buf, c);
	}
	else {
		utstring_reserve (buf, len);
		memset (&buf->d[buf->i], c, len);
		buf->i += len;
		buf->d[buf->i] = '\0';
	}

	return 0;
}

static int
ucl_utstring_append_len (const unsigned char *str, size_t len, void *ud)
{
	UT_string *buf = ud;

	utstring_append_len (buf, str, len);

	return 0;
}

static int
ucl_utstring_append_int (int64_t val, void *ud)
{
	UT_string *buf = ud;

	utstring_printf (buf, "%jd", (intmax_t)val);
	return 0;
}

static int
ucl_utstring_append_double (double val, void *ud)
{
	UT_string *buf = ud;
	const double delta = 0.0000001;

	if (val == (double)(int)val) {
		utstring_printf (buf, "%.1lf", val);
	}
	else if (fabs (val - (double)(int)val) < delta) {
		/* Write at maximum precision */
		utstring_printf (buf, "%.*lg", DBL_DIG, val);
	}
	else {
		utstring_printf (buf, "%lf", val);
	}

	return 0;
}


unsigned char *
ucl_object_emit (const ucl_object_t *obj, enum ucl_emitter emit_type)
{
	UT_string *buf = NULL;
	unsigned char *res = NULL;
	struct ucl_emitter_functions func = {
		.ucl_emitter_append_character = ucl_utstring_append_character,
		.ucl_emitter_append_len = ucl_utstring_append_len,
		.ucl_emitter_append_int = ucl_utstring_append_int,
		.ucl_emitter_append_double = ucl_utstring_append_double
	};

	if (obj == NULL) {
		return NULL;
	}

	utstring_new (buf);
	func.ud = buf;

	if (buf != NULL) {
		if (ucl_object_emit_full (obj, emit_type, &func)) {
			res = utstring_body (buf);
		}
		else {
			utstring_done (buf);
		}
		free (buf);
	}

	return res;
}

bool
ucl_object_emit_full (const ucl_object_t *obj, enum ucl_emitter emit_type,
		struct ucl_emitter_functions *emitter)
{
	struct ucl_emitter_context ctx;
	bool res = false;

	if (emit_type >= UCL_EMIT_JSON && emit_type <= UCL_EMIT_YAML) {
		memcpy (&ctx, &ucl_standard_emitters[emit_type], sizeof (ctx));
		ctx.func = emitter;
		ctx.ident = 0;
		ctx.top = obj;

		ctx.ops->ucl_emitter_write_elt (&ctx, obj, true, false);
		res = true;
	}

	return res;
}


unsigned char *
ucl_object_emit_single_json (const ucl_object_t *obj)
{
	UT_string *buf = NULL;
	unsigned char *res = NULL;

	if (obj == NULL) {
		return NULL;
	}

	utstring_new (buf);

	if (buf != NULL) {
		switch (obj->type) {
		case UCL_OBJECT:
			ucl_utstring_append_len ("object", 6, buf);
			break;
		case UCL_ARRAY:
			ucl_utstring_append_len ("array", 5, buf);
			break;
		case UCL_INT:
			ucl_utstring_append_int (obj->value.iv, buf);
			break;
		case UCL_FLOAT:
		case UCL_TIME:
			ucl_utstring_append_double (obj->value.dv, buf);
			break;
		case UCL_NULL:
			ucl_utstring_append_len ("null", 4, buf);
			break;
		case UCL_BOOLEAN:
			if (obj->value.iv) {
				ucl_utstring_append_len ("true", 4, buf);
			}
			else {
				ucl_utstring_append_len ("false", 5, buf);
			}
			break;
		case UCL_STRING:
			ucl_utstring_append_len (obj->value.sv, obj->len, buf);
			break;
		case UCL_USERDATA:
			ucl_utstring_append_len ("userdata", 8, buf);
			break;
		}
		res = utstring_body (buf);
		free (buf);
	}

	return res;
}
