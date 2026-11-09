#include "reply.h"
#include "client.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

static int out_append(struct client *c, const char *data, size_t n)
{
	if (CLIENT_OUT_SIZE - c->out_len < n)
		return -1;
	memcpy(c->out + c->out_len, data, n);
	c->out_len += n;
	return 0;
}

static void reply_line(struct client *c, char type, const char *s)
{
	size_t len = strlen(s);

	if (CLIENT_OUT_SIZE - c->out_len < len + 3)
		return;

	out_append(c, &type, 1);
	out_append(c, s, len);
	out_append(c, "\r\n", 2);
}

void reply_simple(struct client *c, const char *s)
{
	reply_line(c, '+', s);
}

void reply_error(struct client *c, const char *s)
{
	reply_line(c, '-', s);
}

void reply_null(struct client *c)
{
	out_append(c, "$-1\r\n", 5);
}

void reply_bulk(struct client *c, const char *s, size_t len)
{
	char hdr[32];
	int hlen = snprintf(hdr, sizeof(hdr), "$%zu\r\n", len);

	if (hlen < 0 || CLIENT_OUT_SIZE - c->out_len < (size_t)hlen + len + 2)
		return;

	out_append(c, hdr, (size_t)hlen);
	out_append(c, s, len);
	out_append(c, "\r\n", 2);
}

void reply_int(struct client *c , int num) {
    char hdr[32];
	int hlen = snprintf(hdr, sizeof(hdr), ":%d\r\n",num);
    out_append(c, hdr, (size_t)hlen);
}
