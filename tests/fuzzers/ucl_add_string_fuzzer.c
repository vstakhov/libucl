#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include "ucl.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
       
	struct ucl_parser *parser;
       	parser = ucl_parser_new(0);

	ucl_parser_add_string(parser, (char *)data, size);
	
	if (ucl_parser_get_error(parser) != NULL) {
		return 0;
	}

	ucl_parser_free (parser);
        return 0;
}
