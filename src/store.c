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

static void list_free(struct list *l)
{
	if (!l)
		return;
	for (size_t i = 0; i < l->len; i++)
		free(l->items[i]);
	free(l->items);
	free(l->lens);
	free(l);
}

static int list_reserve(struct list *l, size_t need)
{
	size_t cap = l->cap ? l->cap : 4;
	char **items;
	size_t *lens;

	if (l->cap >= need)
		return 0;
	while (cap < need)
		cap *= 2;

	items = realloc(l->items, cap * sizeof(*items));
	if (!items)
		return -1;
	l->items = items;

	lens = realloc(l->lens, cap * sizeof(*lens));
	if (!lens)
		return -1;
	l->lens = lens;

	l->cap = cap;
	return 0;
}

static void free_entry(struct entry *e)
{
	list_free(e->list);
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
		list_free(e->list);
		e->list = NULL;
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

static int list_push(struct list *l, const char *const *vals,
		     const size_t *lens, size_t n, int front)
{
	char **copies;

	if (n == 0)
		return 0;
	if (list_reserve(l, l->len + n) < 0)
		return -1;

	copies = calloc(n, sizeof(*copies));
	if (!copies)
		return -1;

	for (size_t i = 0; i < n; i++) {
		copies[i] = dup_bytes(vals[i], lens[i]);
		if (!copies[i]) {
			while (i > 0)
				free(copies[--i]);
			free(copies);
			return -1;
		}
	}

	if (front) {
		memmove(l->items + n, l->items, l->len * sizeof(*l->items));
		memmove(l->lens + n, l->lens, l->len * sizeof(*l->lens));
		for (size_t i = 0; i < n; i++) {
			l->items[i] = copies[n - 1 - i];
			l->lens[i] = lens[n - 1 - i];
		}
	} else {
		for (size_t i = 0; i < n; i++) {
			l->items[l->len + i] = copies[i];
			l->lens[l->len + i] = lens[i];
		}
	}
	l->len += n;
	free(copies);
	return 0;
}

static int store_push(struct store *s, const char *key, size_t klen,
		      const char *const *vals, const size_t *lens, size_t n,
		      size_t *out_len, int front)
{
	struct entry *e;
	int created = 0;

	store_get(s, key, klen);
	e = *find_link(s, key, klen);

	if (e && !e->list)
		return STORE_WRONGTYPE;

	if (!e) {
		struct entry **link;

		if (s->count + 1 > s->nbuckets * 3 / 4)
			grow(s);
		link = find_link(s, key, klen);

		e = calloc(1, sizeof(*e));
		if (!e)
			return -1;
		e->key = dup_bytes(key, klen);
		e->list = calloc(1, sizeof(*e->list));
		if (!e->key || !e->list) {
			free_entry(e);
			return -1;
		}
		e->key_len = klen;

		*link = e;
		s->count++;
		created = 1;
	}

	if (list_push(e->list, vals, lens, n, front) < 0) {
		if (created)
			store_del(s, key, klen);
		return -1;
	}

	*out_len = e->list->len;
	return 0;
}

int store_rpush(struct store *s, const char *key, size_t klen,
		const char *const *vals, const size_t *lens, size_t n,
		size_t *out_len)
{
	return store_push(s, key, klen, vals, lens, n, out_len, 0);
}

int store_lpush(struct store *s, const char *key, size_t klen,
		const char *const *vals, const size_t *lens, size_t n,
		size_t *out_len)
{
	return store_push(s, key, klen, vals, lens, n, out_len, 1);
}
