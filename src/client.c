#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "client.h"

void client_init(struct client *c, int fd, struct store *store)
{
	c->fd = fd;
	c->store = store;
	c->in_len = 0;
	c->out_len = 0;
}

void client_consume(struct client *c, size_t n)
{
	memmove(c->in, c->in + n, c->in_len - n);
	c->in_len -= n;
}

int client_flush(struct client *c)
{
	size_t sent = 0;
	while (sent < c->out_len) {
		ssize_t n = send(c->fd, c->out + sent, c->out_len - sent, 0);
		if (n < 0 && errno == EINTR)
			continue;
		if (n <= 0)
			return -1;
		sent += (size_t)n;
	}
	c->out_len = 0;
	return 0;
}
