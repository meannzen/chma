#include "store.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NUM_ALLOC 16

static uint64_t hash_bytes(const char *p, size_t n)
{
	uint64_t h = 1469598103934665603ULL;

	for (size_t i = 0; i < n; i++) {
		h ^= (unsigned char)p[i];
		h *= 1099511628211ULL;
	}
	return h;
}

static char *dup_bytes(const char *p, size_t n)
{
	char *d = malloc(n + 1);

	if (!d)
		return NULL;
	if (n)
		memcpy(d, p, n);
	d[n] = '\0';
	return d;
}

static void free_entry(struct entry *e)
{
	free(e->key);
	free(e->val);
	free(e);
}

static struct entry **find_link(struct store *s, const char *key, size_t klen)
{
	struct entry **link = &s->buckets[hash_bytes(key, klen) % s->nbuckets];

	while (*link) {
		struct entry *e = *link;

		if (e->key_len == klen && memcmp(e->key, key, klen) == 0)
			return link;
		link = &e->next;
	}
	return link;
}

static void grow(struct store *s)
{
	size_t new_n = s->nbuckets * 2;
	struct entry **nb = calloc(new_n, sizeof(*nb));

	if (!nb)
		return;

	for (size_t i = 0; i < s->nbuckets; i++) {
		struct entry *e = s->buckets[i];

		while (e) {
			struct entry *next = e->next;
			size_t idx = hash_bytes(e->key, e->key_len) % new_n;

			e->next = nb[idx];
			nb[idx] = e;
			e = next;
		}
	}

	free(s->buckets);
	s->buckets = nb;
	s->nbuckets = new_n;
}

long long store_now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

struct store *store_create(void)
{
	struct store *s = calloc(1, sizeof(*s));
	if (s == NULL)
		return NULL;

	s->nbuckets = NUM_ALLOC;
	s->buckets = calloc(s->nbuckets, sizeof(*s->buckets));
	if (s->buckets == NULL) {
		free(s);
		return NULL;
	}

	return s;
}

void store_destroy(struct store *s)
{
	if (!s)
		return;

	for (size_t i = 0; i < s->nbuckets; i++) {
		struct entry *e = s->buckets[i];
		while (e) {
			struct entry *next = e->next;
			free_entry(e);
			e = next;
		}
	}

	free(s->buckets);
	free(s);
}

int store_set(struct store *s, const char *key, size_t klen, const char *val,
	      size_t vlen, long long expire_at_ms)
{
	struct entry **link = find_link(s, key, klen);
	struct entry *e = *link;

	if (e) {
		char *nv = dup_bytes(val, vlen);

		if (!nv)
			return -1;
		free(e->val);
		e->val = nv;
		e->val_len = vlen;
		e->expire_at_ms = expire_at_ms;
		return 0;
	}

	if (s->count + 1 > s->nbuckets * 3 / 4) {
		grow(s);
		link = find_link(s, key, klen);
	}

	e = calloc(1, sizeof(*e));
	if (!e)
		return -1;

	e->key = dup_bytes(key, klen);
	e->val = dup_bytes(val, vlen);
	if (!e->key || !e->val) {
		free_entry(e);
		return -1;
	}
	e->key_len = klen;
	e->val_len = vlen;
	e->expire_at_ms = expire_at_ms;

	*link = e;
	s->count++;
	return 0;
}

const struct entry *store_get(struct store *s, const char *key, size_t klen)
{
	struct entry **link = find_link(s, key, klen);
	struct entry *e = *link;

	if (!e)
		return NULL;

	if (e->expire_at_ms != 0 && e->expire_at_ms <= store_now_ms()) {
		*link = e->next;
		free_entry(e);
		s->count--;
		return NULL;
	}
	return e;
}

int store_del(struct store *s, const char *key, size_t klen)
{
	struct entry **link = find_link(s, key, klen);
	struct entry *e = *link;

	if (!e)
		return 0;

	*link = e->next;
	free_entry(e);
	s->count--;
	return 1;
}
