#ifndef __KERN_MM_SLUB_H__
#define __KERN_MM_SLUB_H__
#include <defs.h>
/* Single-hart teaching allocator. Initialize once AFTER pmm_init(). */
void slub_init(void);
void *kmalloc(size_t size);
void kfree(void *object);
void slub_check(void);
#endif
