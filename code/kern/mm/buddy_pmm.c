#include <buddy_pmm.h>
#include <stdio.h>

/* 4 KiB * 2^18 = 1 GiB, matching this kernel's bootstrap mapping limit.
 * No public Page layout changes: private bits are used only by this manager.
 * property is a FREE block's page count, or an ALLOCATED head's request size.
 * Single-hart boot-time allocator; callers must serialize concurrent access.
 */
#define MAX_ORDER 18
#define B_ALLOC (1UL << 2)
#define B_HEAD  (1UL << 3)
static free_area_t areas[MAX_ORDER + 1];
static size_t available;

static size_t block_size(unsigned order) { return (size_t)1 << order; }

static unsigned request_order(size_t n) {
    unsigned order = 0;
    while (order < MAX_ORDER && block_size(order) < n) order++;
    return order;
}

static void buddy_init(void) {
    for (unsigned o = 0; o <= MAX_ORDER; o++) {
        list_init(&areas[o].free_list);
        areas[o].nr_free = 0;
    }
    available = 0;
}

static void insert_block(struct Page *p, unsigned o) {
    p->property = block_size(o);
    SetPageProperty(p);
    list_add(&areas[o].free_list, &p->page_link);
    areas[o].nr_free += block_size(o);
}

static void remove_block(struct Page *p, unsigned o) {
    list_del(&p->page_link);
    areas[o].nr_free -= block_size(o);
    ClearPageProperty(p);
    p->property = 0;
}

/* XOR operates on ABSOLUTE physical page numbers, not descriptor indices.
 * Bounds and head/order checks prevent merging across reserved holes (DTB).
 */
static void release_block(struct Page *p, unsigned o) {
    while (o < MAX_ORDER) {
        size_t ppn = page2ppn(p);
        size_t partner = ppn ^ block_size(o);
        if (partner < nbase || partner >= npage ||
            block_size(o) > npage - partner) break;
        struct Page *q = pages + (partner - nbase);
        if (PageReserved(q) || !PageProperty(q) ||
            q->property != block_size(o)) break;
        remove_block(q, o);
        if (q < p) p = q;
        o++;
    }
    insert_block(p, o);
}

static void buddy_init_memmap(struct Page *base, size_t n) {
    assert(n > 0 && base >= pages && base < pages + (npage - nbase));
    assert(n <= (size_t)(pages + (npage - nbase) - base));
    for (size_t i = 0; i < n; i++) {
        assert(PageReserved(base + i));
        base[i].flags = 0;
        base[i].property = 0;
        set_page_ref(base + i, 0);
    }
    available += n;
    /* Decompose arbitrary, non-power-of-two ranges into aligned blocks. */
    while (n) {
        unsigned o = MAX_ORDER;
        size_t ppn = page2ppn(base);
        while (block_size(o) > n || (ppn & (block_size(o) - 1))) o--;
        release_block(base, o);
        base += block_size(o);
        n -= block_size(o);
    }
}

static struct Page *buddy_alloc_pages(size_t n) {
    assert(n > 0);
    if (n > block_size(MAX_ORDER)) return NULL;
    unsigned wanted = request_order(n), o = wanted;
    size_t actual = block_size(wanted);
    if (actual > available) return NULL;
    while (o <= MAX_ORDER && list_empty(&areas[o].free_list)) o++;
    if (o > MAX_ORDER) return NULL;
    struct Page *p = le2page(list_next(&areas[o].free_list), page_link);
    remove_block(p, o);
    while (o > wanted) {
        o--;
        insert_block(p + block_size(o), o);
    }
    for (size_t i = 0; i < actual; i++) {
        p[i].flags = B_ALLOC;
        p[i].property = 0;
        set_page_ref(p + i, 0);
    }
    p->flags |= B_HEAD;
    p->property = n;
    available -= actual;
    return p;
}

static void buddy_free_pages(struct Page *base, size_t n) {
    assert(n > 0 && n <= block_size(MAX_ORDER));
    assert(base >= pages && base < pages + (npage - nbase));
    unsigned o = request_order(n);
    size_t actual = block_size(o);
    assert(actual <= (size_t)(pages + (npage - nbase) - base));
    assert((page2ppn(base) & (actual - 1)) == 0);
    assert((base->flags & B_HEAD) && base->property == n);
    /* Validate the WHOLE allocation before changing any state. */
    for (size_t i = 0; i < actual; i++) {
        assert(!PageReserved(base + i) && !PageProperty(base + i));
        assert(base[i].flags & B_ALLOC);
        assert(i == 0 || !(base[i].flags & B_HEAD));
        assert(page_ref(base + i) == 0);
    }
    for (size_t i = 0; i < actual; i++) {
        base[i].flags = 0;
        base[i].property = 0;
    }
    available += actual;
    release_block(base, o);
}

static size_t buddy_nr_free_pages(void) { return available; }

static void check_invariants(void) {
    size_t total = 0;
    for (unsigned o = 0; o <= MAX_ORDER; o++) {
        list_entry_t *head = &areas[o].free_list, *le = head;
        size_t subtotal = 0;
        while ((le = list_next(le)) != head) {
            assert(list_next(list_prev(le)) == le);
            assert(list_prev(list_next(le)) == le);
            struct Page *p = le2page(le, page_link);
            assert(PageProperty(p) && !PageReserved(p));
            assert(p->property == block_size(o));
            assert((page2ppn(p) & (block_size(o) - 1)) == 0);
            assert(!(p->flags & B_ALLOC));
            subtotal += block_size(o);
            assert(subtotal <= available);
        }
        assert(subtotal == areas[o].nr_free);
        total += subtotal;
    }
    assert(total == available);
}

static void buddy_check(void) {
    check_invariants();
    size_t before = available;
    struct Page *arena = buddy_alloc_pages(16);
    assert(arena != NULL);
    /* Isolate 16 real pages; original lists are untouched until restored.
     * Saved sentinel pointers refer to the original areas[] addresses.
     */
    free_area_t saved[MAX_ORDER + 1];
    for (unsigned o = 0; o <= MAX_ORDER; o++) saved[o] = areas[o];
    size_t saved_available = available;
    buddy_init();
    assert(buddy_alloc_pages(1) == NULL);
    for (size_t i = 0; i < 16; i++) {
        arena[i].flags = 0;
        SetPageReserved(arena + i);
    }
    /* Two disjoint, non-power-of-two ranges with a reserved hole at 7. */
    buddy_init_memmap(arena, 7);
    buddy_init_memmap(arena + 8, 8);
    assert(available == 15 && PageReserved(arena + 7));
    struct Page *held[15];
    for (size_t i = 0; i < 15; i++) {
        held[i] = buddy_alloc_pages(1);
        assert(held[i] != NULL && held[i] != arena + 7);
        for (size_t j = 0; j < i; j++) assert(held[i] != held[j]);
    }
    assert(available == 0 && buddy_alloc_pages(1) == NULL);
    for (size_t i = 15; i > 0; i--) buddy_free_pages(held[i - 1], 1);
    check_invariants();
    assert(available == 15);
    buddy_init_memmap(arena + 7, 1);
    check_invariants();
    assert(available == 16);
    /* Recursive merge must reconstruct the original aligned 16-page block. */
    struct Page *whole = buddy_alloc_pages(16);
    assert(whole == arena && available == 0);
    buddy_free_pages(whole, 16);
    struct Page *a = buddy_alloc_pages(3);
    struct Page *b = buddy_alloc_pages(5);
    assert(a && b && available == 4);
    assert((page2ppn(a) & 3) == 0 && (page2ppn(b) & 7) == 0);
    assert(a + 4 <= b || b + 8 <= a);
    assert(buddy_alloc_pages(5) == NULL && available == 4);
    assert(buddy_alloc_pages(block_size(MAX_ORDER) + 1) == NULL);
    check_invariants();
    buddy_free_pages(a, 3);
    buddy_free_pages(b, 5);
    whole = buddy_alloc_pages(16);
    assert(whole == arena && available == 0);
    for (unsigned o = 0; o <= MAX_ORDER; o++) areas[o] = saved[o];
    available = saved_available;
    buddy_free_pages(whole, 16);
    assert(available == before);
    check_invariants();
    cprintf("buddy_check() succeeded: split, rounding, exhaustion, holes, merge, restore\n");
}

const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};
