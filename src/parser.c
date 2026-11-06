#include "parser.h"


static enum parse_status read_int(const char **pp, const char *end, long *out,
				  long max)
{
	const char *p = *pp;
	long v = 0;
	int digits = 0;

	while (p < end && *p >= '0' && *p <= '9') {
		v = v * 10 + (*p - '0');
		if (v > max)
			return PARSE_ERROR;
		digits++;
		p++;
	}

	if (p == end)
		return PARSE_INCOMPLETE;
	if (digits == 0 || *p != '\r')
		return PARSE_ERROR;
	if (end - p < 2)
		return PARSE_INCOMPLETE;
	if (p[1] != '\n')
		return PARSE_ERROR;

	*out = v;
	*pp = p + 2;
	return PARSE_OK;
}

enum parse_status parse_command(const char *buf, size_t len,
				struct command *out, size_t *consumed)
{
	const char *p = buf;
	const char *end = buf + len;
	enum parse_status st;
	long n;

	if (p == end)
		return PARSE_INCOMPLETE;
	if (*p != '*')
		return PARSE_ERROR;
	p++;

	st = read_int(&p, end, &n, PARSER_MAX_ARGS);
	if (st != PARSE_OK)
		return st;
	if (n < 1)
		return PARSE_ERROR;

	for (long i = 0; i < n; i++) {
		long blen;

		if (p == end)
			return PARSE_INCOMPLETE;
		if (*p != '$')
			return PARSE_ERROR;
		p++;

		st = read_int(&p, end, &blen, PARSER_MAX_BULK_LEN);
		if (st != PARSE_OK)
			return st;


		if ((size_t)(end - p) < (size_t)blen + 2)
			return PARSE_INCOMPLETE;
		if (p[blen] != '\r' || p[blen + 1] != '\n')
			return PARSE_ERROR;

		out->argv[i] = p;
		out->argl[i] = (size_t)blen;
		p += blen + 2;
	}

	out->argc = (int)n;
	*consumed = (size_t)(p - buf);
	return PARSE_OK;
}
