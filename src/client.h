#ifndef CLIENT_H
#define CLIENT_H


#include <stddef.h>
#define CLIENT_IN_SIZE  (64 * 1024 + 64)
#define CLIENT_OUT_SIZE (64 * 1024)

struct client {
	int fd;
	char in[CLIENT_IN_SIZE];
	size_t in_len;
	char out[CLIENT_OUT_SIZE];
	size_t out_len;
};

void client_init(struct client *c, int fd);
void client_consume(struct client *c, size_t n);
int client_flush(struct client *c);


#endif
