#include <default_pmm.h>
#include <best_fit_pmm.h>
#include <buddy_pmm.h>
#include <defs.h>
#include <error.h>
#include <memlayout.h>
#include <mmu.h>
#include <pmm.h>
#include <sbi.h>
#include <stdio.h>
#include <string.h>
#include <riscv.h>
#include <dtb.h>

// virtual address of physical page array
struct Page *pages;
// amount of physical memory (in pages)
size_t npage = 0;
// the kernel image is mapped at VA=KERNBASE and PA=info.base
uint64_t va_pa_offset;
// memory starts at 0x80000000 in RISC-V
// DRAM_BASE defined in riscv.h as 0x80000000
const size_t nbase = DRAM_BASE / PGSIZE;

// virtual address of boot-time page directory
uintptr_t *satp_virtual = NULL;
// physical address of boot-time page directory
uintptr_t satp_physical;

// physical memory management
const struct pmm_manager *pmm_manager;


static void check_alloc_page(void);

// init_pmm_manager - initialize a pmm_manager instance
static void init_pmm_manager(void) {
#ifdef PMM_USE_BUDDY
    pmm_manager = &buddy_pmm_manager;
#else
    pmm_manager = &best_fit_pmm_manager;
#endif
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}

// init_memmap - call pmm->init_memmap to build Page struct for free memory
static void init_memmap(struct Page *base, size_t n) {
    pmm_manager->init_memmap(base, n);
}

// alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE
// memory
struct Page *alloc_pages(size_t n) {
    return pmm_manager->alloc_pages(n);
}

// free_pages - call pmm->free_pages to free a continuous n*PAGESIZE memory
void free_pages(struct Page *base, size_t n) {
    pmm_manager->free_pages(base, n);
}

// nr_free_pages - call pmm->nr_free_pages to get the size (nr*PAGESIZE)
// of current free memory
size_t nr_free_pages(void) {
    return pmm_manager->nr_free_pages();
}

//lab2 function1 YOUR CODE
static void page_init(void) {
    extern char end[];
    uint64_t base = get_memory_base();
    uint64_t size = get_memory_size();
    va_pa_offset = PHYSICAL_MEMORY_OFFSET;
    assert(base == DRAM_BASE && size > 0 && size <= 0x40000000ULL);
    uint64_t limit = base + size;
    assert(limit > base);
    npage = PPN(limit);
    size_t count = npage - nbase;
    pages = (struct Page *)ROUNDUP((uintptr_t)end, PGSIZE);
    uintptr_t descriptors_pa = PADDR(pages);
    assert(descriptors_pa < limit);
    assert(count <= (limit - descriptors_pa) / sizeof(struct Page));
    uintptr_t free_start = ROUNDUP(descriptors_pa + count * sizeof(struct Page), PGSIZE);
    uintptr_t free_end = ROUNDDOWN(limit, PGSIZE);
    assert(free_start < free_end);
    for (size_t i = 0; i < count; i++) {
        pages[i].flags = 0;
        pages[i].property = 0;
        set_page_ref(&pages[i], 0);
        SetPageReserved(&pages[i]);
    }
    /* Keep the DTB alive: its physical pages must not enter the allocator. */
    extern uint64_t boot_dtb;
    if (boot_dtb >= base && boot_dtb < limit) {
        const unsigned char *dtb = (const unsigned char *)(boot_dtb + va_pa_offset);
        uint32_t total = ((uint32_t)dtb[4] << 24) | ((uint32_t)dtb[5] << 16) |
                         ((uint32_t)dtb[6] << 8) | dtb[7];
        assert(total >= 40 && total <= limit - boot_dtb);
        uintptr_t dtb_start = ROUNDDOWN(boot_dtb, PGSIZE);
        uintptr_t dtb_end = ROUNDUP(boot_dtb + total, PGSIZE);
        if (dtb_start < free_end && dtb_end > free_start) {
            uintptr_t left_end = dtb_start < free_end ? dtb_start : free_end;
            uintptr_t right_start = dtb_end > free_start ? dtb_end : free_start;
            if (free_start < left_end)
                init_memmap(pa2page(free_start), (left_end - free_start) / PGSIZE);
            if (right_start < free_end)
                init_memmap(pa2page(right_start), (free_end - right_start) / PGSIZE);
        } else {
            init_memmap(pa2page(free_start), (free_end - free_start) / PGSIZE);
        }
    } else {
        init_memmap(pa2page(free_start), (free_end - free_start) / PGSIZE);
    }
    cprintf("physical memory: base=0x%lx size=0x%lx free=%lu pages\n",
            base, size, nr_free_pages());
}

/* pmm_init - initialize the physical memory management */
void pmm_init(void) {
    // We need to alloc/free the physical memory (granularity is 4KB or other size).
    // So a framework of physical memory manager (struct pmm_manager)is defined in pmm.h
    // First we should init a physical memory manager(pmm) based on the framework.
    // Then pmm can alloc/free the physical memory.
    // First Fit, Best Fit and Buddy are implemented; Best Fit is the default.
    init_pmm_manager();

    // detect physical memory space, reserve already used memory,
    // then use pmm->init_memmap to create free page list
    page_init();

    // use pmm->check to verify the correctness of the alloc/free function in a pmm
    check_alloc_page();

    extern char boot_page_table_sv39[];
    satp_virtual = (pte_t*)boot_page_table_sv39;
    satp_physical = PADDR(satp_virtual);
    cprintf("satp virtual address: 0x%016lx\nsatp physical address: 0x%016lx\n", satp_virtual, satp_physical);
}

static void check_alloc_page(void) {
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}
