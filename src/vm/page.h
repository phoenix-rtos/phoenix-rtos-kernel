/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Virtual memory manager - page allocator
 *
 * Copyright 2012, 2016 Phoenix Systems
 * Copyright 2001, 2005 Pawel Pisarczyk
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _VM_PAGE_H_
#define _VM_PAGE_H_

#include HAL
#include "../../include/sysinfo.h"


//extern page_t *_page_alloc(size_t size, u8 flags);


extern page_t *vm_pageAlloc(size_t size, u8 flags);


extern void _page_free(page_t *lh);


extern void vm_pageFree(page_t *lh);


extern page_t *_page_get(addr_t addr);


extern void vm_pageFreeAt(pmap_t *pmap, void *vaddr);


extern void _page_showPages(void);


extern int page_map(pmap_t *pmap, void *vaddr, addr_t pa, int attr);


extern int _page_sbrk(pmap_t *pmap, void **bss, void **top);


extern void vm_pageGetStats(size_t *freesz);


extern void vm_pageinfo(meminfo_t *info);


extern void _page_init(pmap_t *pmap, void **bss, void **top);


#endif
