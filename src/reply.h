#ifndef REPLY_H
#define REPLY_H

#include <stddef.h>

#include "client.h"

void reply_simple(struct client *c, const char *s);
void reply_error(struct client *c, const char *s);
void reply_bulk(struct client *c, const char *s, size_t len);
void reply_null(struct client *c);

void reply_int(struct client *c , int num);
#endif
