/*
 * Copyright (c) 2015, Vsevolod Stakhov
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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ucl.h"
#include "ucl_internal.h"

#ifdef HAVE_ENDIAN_H
#include <endian.h>
#elif defined(HAVE_SYS_ENDIAN_H)
#include <sys/endian.h>
#elif defined(HAVE_MACHINE_ENDIAN_H)
#include <machine/endian.h>
#else
#define __LITTLE_ENDIAN__ 4321
#define __BYTE_ORDER__ __LITTLE_ENDIAN__
#endif

#define SWAP_LE_BE16(val)	((uint16_t) ( 		\
		(uint16_t) ((uint16_t) (val) >> 8) |	\
		(uint16_t) ((uint16_t) (val) << 8)))

#if defined(__clang__) || (defined(__GNUC__) && __GNUC__ >= 4 && defined (__GNUC_MINOR__) && __GNUC_MINOR__ >= 3)
#	define SWAP_LE_BE32(val) ((uint32_t)__builtin_bswap32 ((uint32_t)(val)))
#	define SWAP_LE_BE64(val) ((uint64_t)__builtin_bswap64 ((uint64_t)(val)))
#else
	#define SWAP_LE_BE32(val)	((uint32_t)( \
		(((uint32_t)(val) & (uint32_t)0x000000ffU) << 24) | \
		(((uint32_t)(val) & (uint32_t)0x0000ff00U) <<  8) | \
		(((uint32_t)(val) & (uint32_t)0x00ff0000U) >>  8) | \
		(((uint32_t)(val) & (uint32_t)0xff000000U) >> 24)))

	#define SWAP_LE_BE64(val)	((uint64_t)( 			\
		  (((uint64_t)(val) &							\
		(uint64_t)(0x00000000000000ffULL)) << 56) |		\
		  (((uint64_t)(val) &							\
		(uint64_t)(0x000000000000ff00ULL)) << 40) |		\
		  (((uint64_t)(val) &							\
		(uint64_t)(0x0000000000ff0000ULL)) << 24) |		\
		  (((uint64_t)(val) &							\
		(uint64_t) (0x00000000ff000000ULL)) <<  8) |	\
		  (((uint64_t)(val) &							\
		(uint64_t)(0x000000ff00000000ULL)) >>  8) |		\
		  (((uint64_t)(val) &							\
		(uint64_t)(0x0000ff0000000000ULL)) >> 24) |		\
		  (((uint64_t)(val) &							\
		(uint64_t)(0x00ff000000000000ULL)) >> 40) |		\
		  (((uint64_t)(val) &							\
		(uint64_t)(0xff00000000000000ULL)) >> 56)))
#endif

#if __BYTE_ORDER__ == __LITTLE_ENDIAN__
#define TO_BE16 SWAP_LE_BE16
#define TO_BE32 SWAP_LE_BE32
#define TO_BE64 SWAP_LE_BE64
#else
#define TO_BE16(val) (uint16_t)(val)
#define TO_BE32(val) (uint32_t)(val)
#define TO_BE64(val) (uint64_t)(val)
#endif

void
ucl_emitter_print_int_msgpack (struct ucl_emitter_context *ctx, int64_t val)
{
	const struct ucl_emitter_functions *func = ctx->func;


}

void
ucl_emitter_print_double_msgpack (struct ucl_emitter_context *ctx, double val)
{
	const struct ucl_emitter_functions *func = ctx->func;


}

void
ucl_emitter_print_bool_msgpack (struct ucl_emitter_context *ctx, bool val)
{
	const struct ucl_emitter_functions *func = ctx->func;


}

void
ucl_emitter_print_string_msgpack (struct ucl_emitter_context *ctx,
		const char *s, size_t len)
{
	const struct ucl_emitter_functions *func = ctx->func;


}

void
ucl_emitter_print_null_msgpack (struct ucl_emitter_context *ctx)
{
	const struct ucl_emitter_functions *func = ctx->func;


}

void
ucl_emitter_print_key_msgpack (bool print_key, struct ucl_emitter_context *ctx,
		const ucl_object_t *obj)
{

}
