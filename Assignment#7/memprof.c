/*
Assignment#7
Operating Systems
Redon Jashari
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#define DEFAULT_N_BUCKETS 16384

typedef void *(*malloc_t)(size_t);
typedef void *(*calloc_t)(size_t, size_t);
typedef void *(*realloc_t)(void *, size_t);
typedef void  (*free_t)(void *);

static malloc_t  real_malloc  = NULL;
static calloc_t  real_calloc  = NULL;
static realloc_t real_realloc = NULL;
static free_t    real_free    = NULL;

static pthread_once_t init_once = PTHREAD_ONCE_INIT;
static int header_printed = 0;

//hash table for ptr
struct alloc_entry {
    void *ptr;
    size_t size;
    struct alloc_entry *next;
};

static struct alloc_entry **buckets = NULL;
static size_t n_buckets = 0;
static pthread_mutex_t map_lock = PTHREAD_MUTEX_INITIALIZER;

static inline size_t ptr_hash(void *p) {
    uintptr_t x = (uintptr_t)p;
    //keep it even for small pointers
    x = (x >> 3) ^ (x >> 13) ^ (x << 7);
    return (size_t)(x & (n_buckets - 1));
}

static void safe_write(const char *buf, size_t len) {
    ssize_t w = write(2, buf, len);
    (void)w;
}

static void init_real_funcs(void) {
    real_malloc  = (malloc_t)dlsym(RTLD_NEXT, "malloc");
    real_calloc  = (calloc_t)dlsym(RTLD_NEXT, "calloc");
    real_realloc = (realloc_t)dlsym(RTLD_NEXT, "realloc");
    real_free    = (free_t)dlsym(RTLD_NEXT, "free");

    /* set up buckets using real_calloc if available, else fallback to malloc */
    n_buckets = DEFAULT_N_BUCKETS;
    if (real_calloc) {
        buckets = (struct alloc_entry **)real_calloc(n_buckets, sizeof(struct alloc_entry *));
    } else if (real_malloc) {
        buckets = (struct alloc_entry **)real_malloc(n_buckets * sizeof(struct alloc_entry *));
        if (buckets) memset(buckets, 0, n_buckets * sizeof(struct alloc_entry *));
    } else {
        buckets = NULL;
    }

    if (!header_printed) {
        const char *hdr = "function,asize,aptr,rptr\n";
        safe_write(hdr, strlen(hdr));
        header_printed = 1;
    }
}

// mapping ptr -> size. If ptr already present, update size.
static void map_insert(void *ptr, size_t size) {
    if (!ptr || !buckets) return;
    pthread_mutex_lock(&map_lock);
    size_t h = ptr_hash(ptr);
    struct alloc_entry *e = buckets[h];
    while (e) {
        if (e->ptr == ptr) {
            e->size = size;
            pthread_mutex_unlock(&map_lock);
            return;
        }
        e = e->next;
    }
    // allocate new entry using real_malloc to avoid using our malloc wrapper
    if (!real_malloc) {
        pthread_mutex_unlock(&map_lock);
        return;
    }
    e = (struct alloc_entry *)real_malloc(sizeof(struct alloc_entry));
    if (!e) {
        pthread_mutex_unlock(&map_lock);
        return;
    }
    e->ptr = ptr;
    e->size = size;
    e->next = buckets[h];
    buckets[h] = e;
    pthread_mutex_unlock(&map_lock);
}

// Remove mapping for ptr and return its size. If not found, return 0
static size_t map_remove(void *ptr) {
    if (!ptr || !buckets) return 0;
    pthread_mutex_lock(&map_lock);
    size_t h = ptr_hash(ptr);
    struct alloc_entry *e = buckets[h];
    struct alloc_entry *prev = NULL;
    while (e) {
        if (e->ptr == ptr) {
            size_t sz = e->size;
            if (prev) prev->next = e->next;
            else buckets[h] = e->next;
            if (real_free) real_free(e);
            pthread_mutex_unlock(&map_lock);
            return sz;
        }
        prev = e;
        e = e->next;
    }
    pthread_mutex_unlock(&map_lock);
    return 0;
}

// Lookup mapping for ptr without removing; returns 0 if not found.
static size_t map_lookup(void *ptr) {
    if (!ptr || !buckets) return 0;
    pthread_mutex_lock(&map_lock);
    size_t h = ptr_hash(ptr);
    struct alloc_entry *e = buckets[h];
    while (e) {
        if (e->ptr == ptr) {
            size_t sz = e->size;
            pthread_mutex_unlock(&map_lock);
            return sz;
        }
        e = e->next;
    }
    pthread_mutex_unlock(&map_lock);
    return 0;
}

// Wrappers

void *malloc(size_t size) {
    pthread_once(&init_once, init_real_funcs);
    void *r = NULL;
    if (real_malloc) r = real_malloc(size);

    if (r) map_insert(r, size);

    char buf[256];
    int n = snprintf(buf, sizeof(buf), "malloc,%zu,,%p\n", size, r);
    if (n > 0) safe_write(buf, (size_t)n);
    return r;
}

void *calloc(size_t nmemb, size_t size) {
    pthread_once(&init_once, init_real_funcs);
    void *r = NULL;
    if (real_calloc) r = real_calloc(nmemb, size);
    size_t total = nmemb * size;
    if (r) map_insert(r, total);

    char buf[256];
    int n = snprintf(buf, sizeof(buf), "calloc,%zu,,%p\n", total, r);
    if (n > 0) safe_write(buf, (size_t)n);
    return r;
}

void *realloc(void *ptr, size_t size) {
    pthread_once(&init_once, init_real_funcs);
    void *r = NULL;
    if (real_realloc) r = real_realloc(ptr, size);

    if (r) {
        if (ptr == NULL) {
            map_insert(r, size);
        } else if (r == ptr) {
            // same pointer
            map_insert(ptr, size);
        } else {
            // moved to new pointer: remove old mapping (if existed) and insert new one
            map_remove(ptr);
            map_insert(r, size);
        }
    } else {
        //realloc failed: keep old mapping
    }

    char buf[256];
    int n = snprintf(buf, sizeof(buf), "realloc,%zu,%p,%p\n", size, ptr, r);
    if (n > 0) safe_write(buf, (size_t)n);
    return r;
}

void free(void *ptr) {
    pthread_once(&init_once, init_real_funcs);
    // find and remove mapping so we can know size freed 
    size_t known_size = map_remove(ptr);

    if (real_free) real_free(ptr);

    char buf[256];
    int n;
    if (known_size) {
        n = snprintf(buf, sizeof(buf), "free,%zu,%p,\n", known_size, ptr);
    } else {
        n = snprintf(buf, sizeof(buf), "free,,%p,\n", ptr);
    }
    if (n > 0) safe_write(buf, (size_t)n);
}
