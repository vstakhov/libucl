#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include "ucl.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	char *ss = (char*)malloc(size+1);
	memcpy(ss, data, size);
	ss[size] = '\0';
       
	struct ucl_parser *parser;
       	parser = ucl_parser_new(0);

	ucl_parser_add_string(parser, ss, 0);
	
	free(ss);

	if (ucl_parser_get_error(parser) != NULL) {
		return 0;
	}

	ucl_parser_free (parser);
        return 0;
}
