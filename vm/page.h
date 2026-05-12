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

#ifndef _PH_VM_PAGE_H_
#define _PH_VM_PAGE_H_

#include "hal/hal.h"
#include "include/sysinfo.h"
#include "types.h"


struct _vm_map_t;
#ifndef NOMMU
struct _ph_map_t;
typedef struct _ph_map_t ph_map_t;
#else
typedef struct _vm_map_t ph_map_t;
#endif

ph_map_t *vm_phMapGet(u8 dmap);


page_t *vm_pageAlloc(ph_map_t **maps, size_t size, vm_flags_t flags, syspage_part_t *part);


void vm_pageFree(page_t *p, syspage_part_t *part);


page_t *page_get(addr_t addr);


void _page_showPages(void);


int page_map(pmap_t *pmap, void *vaddr, addr_t pa, vm_attr_t attr);


int _page_sbrk(pmap_t *pmap, void **start, void **end);


void vm_pageGetStats(size_t *freesz);


void vm_pageinfo(meminfo_t *info);


void _page_init(struct _vm_map_t *kmap, void **bss, void **top);


#endif
