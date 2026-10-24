#include <slub.h>
#include <pmm.h>
#include <list.h>
#include <stdio.h>
#include <string.h>

/* Simplified SLUB: in-object freelists, size caches and partial/full lists.
 * No per-CPU state, locking, constructors or Linux ABI compatibility.
 * Small objects use one backing page; large requests use whole-page runs.
 * Ownership is tracked here, without changing Page or PMM-private flags.
 */
#define ALIGNMENT 16
#define NCACHE 8
struct cache {
    size_t size;
    list_entry_t partial, full;
};
struct slab {
    list_entry_t link;
    struct cache *cache;
    struct Page *page;
    size_t npages, capacity, used;
    void *freelist;
    uint64_t allocated[4];
};
#define HEADER ROUNDUP(sizeof(struct slab), ALIGNMENT)
static struct cache caches[NCACHE];
static list_entry_t large_objects;
static bool initialized;
/* Test-only fault injection at the backing-page allocation boundary. */
static bool fail_next_page;

/* pmm.h's KADDR example is commented out in this lab. Convert only pages
 * returned by the PMM, using the existing kernel direct-map offset.
 */
static struct slab *slab_address(struct Page *p) {
    assert(p >= pages && p < pages + (npage - nbase));
    return (struct slab *)(uintptr_t)(page2pa(p) + va_pa_offset);
}

static struct Page *get_pages(size_t n) {
    if (fail_next_page) { fail_next_page = 0; return NULL; }
    return alloc_pages(n);
}

void slub_init(void) {
    assert(!initialized);
    assert(HEADER < PGSIZE && (PGSIZE - HEADER) / ALIGNMENT <= 256);
    for (unsigned i = 0; i < NCACHE; i++) {
        caches[i].size = (size_t)ALIGNMENT << i;
        list_init(&caches[i].partial);
        list_init(&caches[i].full);
    }
    list_init(&large_objects);
    initialized = 1;
}

static struct slab *new_slab(struct cache *c) {
    struct Page *p = get_pages(1);
    if (p == NULL) return NULL;
    struct slab *s = slab_address(p);
    memset(s, 0, HEADER);
    s->cache = c;
    s->page = p;
    s->npages = 1;
    s->capacity = (PGSIZE - HEADER) / c->size;
    /* A free object stores its successor in its own first pointer-sized bytes. */
    for (size_t i = s->capacity; i > 0; i--) {
        void *obj = (char *)s + HEADER + (i - 1) * c->size;
        *(void **)obj = s->freelist;
        s->freelist = obj;
    }
    list_add(&c->partial, &s->link);
    return s;
}

void *kmalloc(size_t size) {
    assert(initialized);
    if (size == 0) return NULL;
    /* Bound before addition: also rejects overflow and impossible requests. */
    if (size > ((size_t)1 << 30) - HEADER) return NULL;
    unsigned i;
    for (i = 0; i < NCACHE && caches[i].size < size; i++);
    if (i == NCACHE) {
        size_t n = (size + HEADER + PGSIZE - 1) / PGSIZE;
        struct Page *p = get_pages(n);
        if (!p) return NULL;
        struct slab *s = slab_address(p);
        memset(s, 0, HEADER);
        s->page = p;
        s->npages = n; /* Keep original request for Buddy's free contract. */
        s->used = size;
        list_add(&large_objects, &s->link);
        return (char *)s + HEADER;
    }
    struct cache *c = &caches[i];
    struct slab *s = list_empty(&c->partial) ? new_slab(c) :
        to_struct(list_next(&c->partial), struct slab, link);
    if (!s) return NULL;
    void *obj = s->freelist;
    assert(obj != NULL);
    s->freelist = *(void **)obj;
    size_t index = ((char *)obj - (char *)s - HEADER) / c->size;
    assert(!(s->allocated[index / 64] & (1ULL << (index % 64))));
    s->allocated[index / 64] |= 1ULL << (index % 64);
    if (++s->used == s->capacity) {
        list_del(&s->link);
        list_add(&c->full, &s->link);
    }
    return obj;
}

/* Search owned slabs BEFORE dereferencing the supplied pointer. Invalid and
 * duplicate frees fail without modifying state. Stale pointers after address
 * reuse cannot be detected by this pointer-only API (normal allocator limit).
 */
static bool release_object(void *obj) {
    if (obj == NULL) return 1;
    uintptr_t address = (uintptr_t)obj;
    for (unsigned i = 0; i < NCACHE; i++) {
        struct cache *c = &caches[i];
        list_entry_t *heads[2] = {&c->partial, &c->full};
        for (unsigned h = 0; h < 2; h++) {
            list_entry_t *le = heads[h];
            while ((le = list_next(le)) != heads[h]) {
                struct slab *s = to_struct(le, struct slab, link);
                uintptr_t start = (uintptr_t)s + HEADER;
                if (address < start || address >= start + s->capacity * c->size)
                    continue;
                size_t offset = address - start;
                if (offset % c->size) return 0;
                size_t index = offset / c->size;
                uint64_t mask = 1ULL << (index % 64);
                if (!(s->allocated[index / 64] & mask)) return 0;
                s->allocated[index / 64] &= ~mask;
                if (s->used == s->capacity) {
                    list_del(&s->link);
                    list_add(&c->partial, &s->link);
                }
                *(void **)obj = s->freelist;
                s->freelist = obj;
                if (--s->used == 0) {
                    struct Page *page = s->page;
                    list_del(&s->link);
                    free_pages(page, 1); /* Never touch s after returning its page. */
                }
                return 1;
            }
        }
    }
    list_entry_t *le = &large_objects;
    while ((le = list_next(le)) != &large_objects) {
        struct slab *s = to_struct(le, struct slab, link);
        if (address == (uintptr_t)s + HEADER) {
            struct Page *page = s->page;
            size_t n = s->npages;
            list_del(&s->link);
            free_pages(page, n);
            return 1;
        }
    }
    return 0;
}

void kfree(void *object) {
    assert(initialized);
    assert(release_object(object));
}

static void *test_objects[260];
void slub_check(void) {
    assert(initialized);
    /* Run at boot before other clients use these caches. */
    for (unsigned i = 0; i < NCACHE; i++) {
        assert(list_empty(&caches[i].partial) && list_empty(&caches[i].full));
    }
    assert(list_empty(&large_objects));
    size_t before = nr_free_pages();
    assert(kmalloc(0) == NULL && kmalloc((size_t)-1) == NULL);
    kfree(NULL);
    fail_next_page = 1;
    assert(kmalloc(2048) == NULL && nr_free_pages() == before);
    fail_next_page = 1;
    assert(kmalloc(4096) == NULL && nr_free_pages() == before);
    const size_t sizes[] = {1, 15, 16, 17, 31, 32, 33, 63, 64, 65,
        127, 128, 129, 255, 256, 257, 511, 512, 513, 1023, 1024,
        1025, 2048, 2049, 4096, 9000};
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        unsigned char *p = kmalloc(sizes[i]);
        assert(p && ((uintptr_t)p & (ALIGNMENT - 1)) == 0);
        memset(p, 0xa5, sizes[i]);
        for (size_t j = 0; j < sizes[i]; j++) assert(p[j] == 0xa5);
        assert(!release_object(p + 1));
        kfree(p);
        assert(!release_object(p));
        assert(nr_free_pages() == before);
    }
    /* Mixed size classes and large allocations coexist without aliasing. */
    size_t nsize = sizeof(sizes) / sizeof(sizes[0]);
    for (size_t i = 0; i < nsize; i++) {
        test_objects[i] = kmalloc(sizes[i]);
        assert(test_objects[i]);
        memset(test_objects[i], (int)(i + 1), sizes[i]);
    }
    for (size_t i = 0; i < nsize; i++)
        for (size_t j = 0; j < sizes[i]; j++)
            assert(((unsigned char *)test_objects[i])[j] == i + 1);
    for (size_t i = nsize; i > 0; i--) kfree(test_objects[i - 1]);
    assert(nr_free_pages() == before);
    /* More than one slab, simultaneous contents, full->partial and reuse. */
    size_t cap = (PGSIZE - HEADER) / ALIGNMENT;
    for (size_t i = 0; i < cap + 1; i++) {
        test_objects[i] = kmalloc(16);
        assert(test_objects[i]);
        for (size_t j = 0; j < i; j++) assert(test_objects[i] != test_objects[j]);
        memset(test_objects[i], (int)(i & 255), 16);
    }
    for (size_t i = 0; i < cap + 1; i++)
        for (unsigned j = 0; j < 16; j++)
            assert(((unsigned char *)test_objects[i])[j] == (i & 255));
    void *old = test_objects[0];
    kfree(old);
    assert(!release_object(old));
    test_objects[0] = kmalloc(16);
    assert(test_objects[0] == old);
    for (size_t i = cap + 1; i > 0; i--) kfree(test_objects[i - 1]);
    assert(nr_free_pages() == before);
    assert(!release_object((void *)1));
    for (unsigned i = 0; i < NCACHE; i++)
        assert(list_empty(&caches[i].partial) && list_empty(&caches[i].full));
    assert(list_empty(&large_objects));
    cprintf("slub_check() succeeded: sizes, alignment, contents, reuse, invalid free, OOM, restore\n");
}
