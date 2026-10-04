#include "ucl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct handler_state {
	bool bad_name;
};

static bool
expand_variable(const unsigned char *name, size_t len,
				unsigned char **replacement, size_t *replacement_len,
				bool *need_free, void *ud)
{
	struct handler_state *state = ud;
	const char *value;

	/* Both passes must receive only the name, without braces or a suffix. */
	if (memchr(name, '}', len) != NULL || memchr(name, '$', len) != NULL) {
		state->bad_name = true;
		return false;
	}

	*need_free = false;
	if (len == 1 && name[0] == 'X') {
		value = "AB";
	}
	else if (len == 1 && name[0] == 'Y') {
		value = "C";
	}
	else if (len == 5 && memcmp(name, "EMPTY", len) == 0) {
		value = "";
	}
	else if (len == 4 && memcmp(name, "LONG", len) == 0) {
		value = "a longer replacement";
		*need_free = true;
	}
	else if (len == 6 && memcmp(name, "BINARY", len) == 0) {
		*replacement = (unsigned char *) "A\0B";
		*replacement_len = 3;
		return true;
	}
	else {
		return false;
	}

	*replacement_len = strlen(value);
	if (*need_free) {
		*replacement = malloc(*replacement_len + 1);
		if (*replacement == NULL) {
			return false;
		}
		memcpy(*replacement, value, *replacement_len + 1);
	}
	else {
		*replacement = (unsigned char *) value;
	}
	return true;
}

static bool
check_expansion(const char *input, const char *expected, size_t expected_len)
{
	struct handler_state state = {false};
	struct ucl_parser *parser = ucl_parser_new(0);
	ucl_object_t *obj = NULL;
	const ucl_object_t *value;
	const char *actual;
	size_t actual_len;
	char config[256];
	bool ok = false;

	if (parser == NULL) {
		return false;
	}

	ucl_parser_register_variable(parser, "BUILTIN", "registered");
	ucl_parser_set_variables_handler(parser, expand_variable, &state);
	snprintf(config, sizeof(config), "value = \"%s\";", input);
	if (!ucl_parser_add_string(parser, config, strlen(config))) {
		fprintf(stderr, "%s: parse failed: %s\n", input,
				ucl_parser_get_error(parser));
		goto done;
	}

	obj = ucl_parser_get_object(parser);
	value = ucl_object_lookup(obj, "value");
	actual = ucl_object_tolstring(value, &actual_len);
	if (state.bad_name || actual == NULL || actual_len != expected_len ||
		memcmp(actual, expected, expected_len) != 0) {
		fprintf(stderr, "%s: incorrect expansion or callback name\n", input);
		goto done;
	}
	ok = true;

done:
	ucl_object_unref(obj);
	ucl_parser_free(parser);
	return ok;
}

int
main(void)
{
	static const struct {
		const char *input;
		const char *expected;
		size_t expected_len;
	} cases[] = {
#define CASE(input, expected) {input, expected, sizeof(expected) - 1}
		CASE("${X}", "AB"),
		CASE("pre${X}", "preAB"),
		CASE("${X}post", "ABpost"),
		CASE("pre${X}post", "preABpost"),
		CASE("${X}${Y}", "ABC"),
		CASE("pre${X}mid${X}post", "preABmidABpost"),
		CASE("${BUILTIN}/${X}/${Y}", "registered/AB/C"),
		CASE("$BUILTIN/${X}", "registered/AB"),
		CASE("${MISSING}-${X}-${MISSING}", "${MISSING}-AB-${MISSING}"),
		CASE("$${X}-${Y}-$$", "${X}-C-$"),
		CASE("${X}-${MISSING", "AB-${MISSING"),
		CASE("${X}-${", "AB-${"),
		CASE("${EMPTY}", ""),
		CASE("pre${EMPTY}post", "prepost"),
		CASE("${EMPTY}${X}${EMPTY}${Y}", "ABC"),
		CASE("pre${LONG}post", "prea longer replacementpost"),
		CASE("pre${BINARY}post", "preA\0Bpost"),
#undef CASE
	};
	bool ok = true;

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		if (!check_expansion(cases[i].input, cases[i].expected,
							 cases[i].expected_len)) {
			ok = false;
		}
	}
	return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
