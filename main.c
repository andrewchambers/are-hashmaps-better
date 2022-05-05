#define _GNU_SOURCE
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct hashtablekey {
	uint64_t hash;
	const char *str;
	size_t len;
};

struct hashtable {
	size_t len, cap;
	struct hashtablekey *keys;
	void **vals;
};



uint64_t
fnv64(const char *ptr, size_t len)
{	
	size_t i, h;
	unsigned char *p;

	p = (unsigned char *)ptr;
	h = 0xcbf29ce484222325;
	for (i = 0; i < len; i++){
		h *= 0x100000001b3;
		h ^= (uint64_t)p[i];
	}
	return h;
}

static void
htabkey(struct hashtablekey *k, const char *s, size_t n)
{
	k->str = s;
	k->len = n;
	k->hash = fnv64(s, n);
}

static struct hashtable *
mkhtab(size_t cap)
{
	struct hashtable *h;
	size_t i;

	assert(!(cap & (cap - 1)));
	h = malloc(sizeof(*h));
	h->len = 0;
	h->cap = cap;
	h->keys = reallocarray(NULL, cap, sizeof(h->keys[0]));
	h->vals = reallocarray(NULL, cap, sizeof(h->vals[0]));
	for (i = 0; i < cap; ++i)
		h->keys[i].str = NULL;

	return h;
}

static bool
keyequal(struct hashtablekey *k1, struct hashtablekey *k2)
{
	if (k1->hash != k2->hash || k1->len != k2->len)
		return false;
	return memcmp(k1->str, k2->str, k1->len) == 0;
}

static size_t
keyindex(struct hashtable *h, struct hashtablekey *k)
{
	size_t i;

	i = k->hash & (h->cap - 1);
	while (h->keys[i].str && !keyequal(&h->keys[i], k))
		i = (i + 1) & (h->cap - 1);
	return i;
}

static void **
htabput(struct hashtable *h, struct hashtablekey *k)
{
	struct hashtablekey *oldkeys;
	void **oldvals;
	size_t i, j, oldcap;

	if (h->cap / 2 < h->len) {
		oldkeys = h->keys;
		oldvals = h->vals;
		oldcap = h->cap;
		h->cap *= 2;
		h->keys = reallocarray(NULL, h->cap, sizeof(h->keys[0]));
		h->vals = reallocarray(NULL, h->cap, sizeof(h->vals[0]));
		for (i = 0; i < h->cap; ++i)
			h->keys[i].str = NULL;
		for (i = 0; i < oldcap; ++i) {
			if (oldkeys[i].str) {
				j = keyindex(h, &oldkeys[i]);
				h->keys[j] = oldkeys[i];
				h->vals[j] = oldvals[i];
			}
		}
		free(oldkeys);
		free(oldvals);
	}
	i = keyindex(h, k);
	if (!h->keys[i].str) {
		h->keys[i] = *k;
		h->vals[i] = NULL;
		++h->len;
	}

	return &h->vals[i];
}

static void *
htabget(struct hashtable *h, struct hashtablekey *k)
{
	size_t i;

	i = keyindex(h, k);
	return h->keys[i].str ? h->vals[i] : NULL;
}

static void frob(size_t i) 
{
	// Don't let gcc optimize this away.
	asm("":"=r"(i):"r"(i):"memory");
}

struct dumb_kv {
	char *k;
	size_t v;
};

static int
dkv_cmp(const void *l, const void *r)
{
	return strcmp(((struct dumb_kv*)l)->k, ((struct dumb_kv*)r)->k);
}

#define MAX_KEY_SZ 32

int main(int argc, char *argv[])
{
	struct hashtablekey k;
	struct hashtable *h;
	struct dumb_kv *dkv;
	size_t n, m, i, j;
	clock_t t1, t2;

	if (argc != 3) {
		fprintf(stderr, "usage: %s N M", argv[0]);
		exit(1);
	}

	n = strtoul(argv[1], NULL, 10);
	m = strtoul(argv[2], NULL, 10);
	h = mkhtab(1024);
	dkv = calloc(n, sizeof(struct dumb_kv));

	printf("%zu", n);
	
	for (size_t i = 0; i < n; i++) {
		char *s;
		s = NULL;
		asprintf(&s, "%-8zu", i);

		// Add hash table entry.
		htabkey(&k, s, strlen(s));
		*htabput(h, &k) = (void*)i;

		// Add dumb key value entry.
		dkv[i].k = s;
		dkv[i].v = i;
	}

	// qsort, it makes no difference for random linear lookup but is needed for bsearch.
	qsort(dkv, n, sizeof(struct dumb_kv), dkv_cmp);

	// m hash table lookups.
	t1 = clock();
	for (i = 0; i < m; i++) {
		char kbuf[16];

		snprintf(kbuf, sizeof(kbuf), "%zu", rand()%n);
		
		htabkey(&k, kbuf, strlen(kbuf));
		frob((int)htabget(h, &k));
	}
	t2 = clock();
	printf("\t%f", ((double) (t2 - t1)) / CLOCKS_PER_SEC);

	// m dumb_kv lookups.
	t1 = clock();
	for (i = 0; i < m; i++) {
		char kbuf[MAX_KEY_SZ];

		snprintf(kbuf, sizeof(kbuf), "%-8zu", rand()%n);

		int found = 0;
		for (j = 0; j < n; j++) {
			if (strcmp(kbuf, dkv[j].k) == 0) {
				frob(dkv[j].v);
				found = 1;
				break;
			}
		}
		assert(found);
	}
	t2 = clock();
	printf("\t%f", ((double) (t2 - t1)) / CLOCKS_PER_SEC);


	// m dumb_kv bsearch lookups.
	t1 = clock();
	for (i = 0; i < m; i++) {
		struct dumb_kv k, *found;
		char kbuf[MAX_KEY_SZ];

		snprintf(kbuf, sizeof(kbuf), "%-8zu", rand()%n);
		k.k = kbuf;

		found = bsearch(&k, dkv, n, sizeof(struct dumb_kv), dkv_cmp);
		frob(found->v);
	}
	t2 = clock();
	printf("\t%f", ((double) (t2 - t1)) / CLOCKS_PER_SEC);

	printf("\n");
	return 0;
}