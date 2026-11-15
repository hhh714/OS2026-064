#include <assert.h>
#include <error.h>
#include <pmm.h>
#include <stdio.h>
#include <string.h>

static void release_one_page(list_entry_t *held)
{
    assert(!list_empty(held));
    struct Page *page = le2page(list_next(held), page_link);
    list_del(&page->page_link);
    free_page(page);
}

// Exercise real allocation failures without replacing the physical allocator.
void test_pte_oom(void)
{
    size_t free_before = nr_free_pages();
    struct Page *root_page = alloc_page();
    struct Page *data_page = alloc_page();
    assert(root_page != NULL && data_page != NULL);
    pde_t *root = page2kva(root_page);
    memset(root, 0, PGSIZE);

    list_entry_t held;
    list_init(&held);
    struct Page *page;
    while ((page = alloc_page()) != NULL)
    {
        list_add(&held, &page->page_link);
    }
    assert(nr_free_pages() == 0);

    // No memory for the first intermediate table: change nothing.
    assert(page_insert(root, data_page, 0, PTE_R | PTE_W) == -E_NO_MEM);
    assert(root[0] == 0 && page_ref(data_page) == 0);
    assert(nr_free_pages() == 0);

    // One free page is not enough for two intermediate tables. Roll it back.
    release_one_page(&held);
    assert(nr_free_pages() == 1);
    assert(get_pte(root, 0, 0) == NULL);
    assert(nr_free_pages() == 1);
    for (int i = 0; i < 2; i++)
    {
        assert(page_insert(root, data_page, 0, PTE_R | PTE_W) == -E_NO_MEM);
        assert(root[0] == 0);
        assert(page_ref(data_page) == 0);
        assert(nr_free_pages() == 1);
    }

    // An existing subtree and mapping must survive failure in a new branch.
    release_one_page(&held);
    assert(page_insert(root, data_page, 0, PTE_R | PTE_W) == 0);
    assert(nr_free_pages() == 0 && page_ref(data_page) == 1);
    pde_t old_root = root[0];
    pte_t old_leaf = *get_pte(root, 0, 0);
    assert(page_insert(root, data_page, PTSIZE, PTE_R) == -E_NO_MEM);
    assert(root[0] == old_root && *get_pte(root, 0, 0) == old_leaf);
    assert(get_page(root, 0, NULL) == data_page);
    assert(get_pte(root, PTSIZE, 0) == NULL);
    assert(nr_free_pages() == 0 && page_ref(data_page) == 1);

    // Return every page: the test's private root was never loaded into satp.
    pde_t *middle = KADDR(PDE_ADDR(root[0]));
    struct Page *leaf_page = pde2page(middle[0]);
    struct Page *middle_page = pde2page(root[0]);
    page_remove(root, 0);
    set_page_ref(leaf_page, 0);
    set_page_ref(middle_page, 0);
    free_page(leaf_page);
    free_page(middle_page);
    free_page(root_page);
    while (!list_empty(&held))
    {
        release_one_page(&held);
    }
    assert(nr_free_pages() == free_before);
    cprintf("[Test PMM allocation failure] PASS\n");
}
