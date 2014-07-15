/* Copyright (c) 2014, Vsevolod Stakhov
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

extern const struct ucl_emitter_operations ucl_standartd_emitter_ops[];

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

/**
 * Get standard emitter context for a specified emit_type
 * @param emit_type type of emitter
 * @return context or NULL if input is invalid
 */
const struct ucl_emitter_context *
ucl_emit_get_standard_context (enum ucl_emitter emit_type)
{
	if (emit_type >= UCL_EMIT_JSON && emit_type <= UCL_EMIT_YAML) {
		return &ucl_standard_emitters[emit_type];
	}

	return NULL;
}

/**
 * Serialise string
 * @param str string to emit
 * @param buf target buffer
 */
void
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

