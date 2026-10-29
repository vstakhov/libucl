#include "ucl.h"
#include "ucl_internal.h"

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

static struct ucl_variable *
find_variable(struct ucl_parser *parser, const char *name, int *count)
{
	struct ucl_variable *var, *found = NULL;

	*count = 0;
	for (var = parser->variables; var != NULL; var = var->next) {
		if (strcmp(var->var, name) == 0) {
			found = var;
			(*count)++;
		}
	}

	return found;
}

/*
 * While an included file is parsed, its own FILENAME, CURDIR and current file
 * replace those of the including document. A failed include must hand them
 * back just like a successful one: they used to stay detached from the parser
 * and leak, with the included file left as the current one.
 */
static bool
check_failed_include(const char *base, const char *args, const char *included,
					 const char *top_file)
{
	const char *dir = getenv("TEST_OUT_DIR");
	struct ucl_parser *parser = ucl_parser_new(0);
	struct ucl_variable *filename, *curdir, *var;
	char path[1024], config[2048];
	const char *cur_file;
	FILE *out;
	int count;
	bool ok = false;

	if (parser == NULL) {
		return false;
	}

	snprintf(path, sizeof(path), "%s/variables_failed.inc",
			 dir != NULL ? dir : ".");
	out = fopen(path, "w");
	if (out == NULL) {
		fprintf(stderr, "cannot create %s\n", path);
		goto done;
	}
	fputs(included, out);
	if (fclose(out) != 0) {
		fprintf(stderr, "cannot write %s\n", path);
		goto done;
	}

	if (top_file != NULL) {
		ucl_parser_set_filevars(parser, top_file, false);
	}
	filename = find_variable(parser, "FILENAME", &count);
	curdir = find_variable(parser, "CURDIR", &count);
	snprintf(config, sizeof(config), "%s\n.include%s \"%s\"\n", base, args, path);

	if (ucl_parser_add_string(parser, config, 0)) {
		fprintf(stderr, "%s: the include must fail\n", included);
		goto done;
	}

	ok = true;
	cur_file = ucl_parser_get_cur_file(parser);
	if (top_file != NULL ? (cur_file == NULL || strcmp(cur_file, top_file) != 0)
						 : cur_file != NULL) {
		fprintf(stderr, "%s: current file is %s after a failed include\n",
				included, cur_file != NULL ? cur_file : "(null)");
		ok = false;
	}
	var = find_variable(parser, "FILENAME", &count);
	if (var != filename || count != 1) {
		fprintf(stderr, "%s: FILENAME was not restored after a failed include\n",
				included);
		ok = false;
	}
	var = find_variable(parser, "CURDIR", &count);
	if (var != curdir || count != 1) {
		fprintf(stderr, "%s: CURDIR was not restored after a failed include\n",
				included);
		ok = false;
	}

done:
	remove(path);
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
	/* The included file does not parse */
	ok = check_failed_include("", "", "a = {{;", NULL) && ok;
	ok = check_failed_include("", "", "a = {{;", "/nonexistent/top.conf") && ok;
	/* Rejected before parsing: the prefix names a key that is not an object */
	ok = check_failed_include("key = 1;", "(key=\"key\")", "a = 1;",
							  "/nonexistent/top.conf") && ok;
	return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
