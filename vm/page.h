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

#include "hal/hal.h"
#include "include/sysinfo.h"
#include "types.h"


page_t *vm_pageAlloc(size_t size, vm_flags_t flags);


void vm_pageFree(page_t *lh);


page_t *_page_get(addr_t addr);


void _page_showPages(void);


int page_map(pmap_t *pmap, void *vaddr, addr_t pa, vm_attr_t attr);


int _page_sbrk(pmap_t *pmap, void **start, void **end);


void vm_pageGetStats(size_t *freesz);


void vm_pageinfo(meminfo_t *info);


void _page_init(pmap_t *pmap, void **bss, void **top);


#endif
