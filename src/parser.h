#ifndef PARSER_H
#define PARSER_H

#include <stddef.h>

#define PARSER_MAX_ARGS 16
#define PARSER_MAX_BULK_LEN (64 * 1024)

enum parse_status {
	PARSE_OK,
	PARSE_INCOMPLETE,
	PARSE_ERROR
};

struct command {
	int argc;
	const char *argv[PARSER_MAX_ARGS];
	size_t argl[PARSER_MAX_ARGS];
};

enum parse_status parse_command(const char *buf, size_t len,
				struct command *out, size_t *consumed);

#endif
