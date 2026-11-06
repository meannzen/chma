#ifndef STORE_H
#define STORE_H

#include <stddef.h>

struct entry {
	char *key;
	size_t key_len;
	char *val;
	size_t val_len;
	long long expire_at_ms;
	struct entry *next;
};

struct store {
	struct entry **buckets;
	size_t nbuckets;
	size_t count;
};

struct store *store_create(void);
void store_destroy(struct store *s);

long long store_now_ms(void);

int store_set(struct store *s, const char *key, size_t klen, const char *val,
	      size_t vlen, long long expire_at_ms);

const struct entry *store_get(struct store *s, const char *key, size_t klen);

int store_del(struct store *s, const char *key, size_t klen);

#endif
