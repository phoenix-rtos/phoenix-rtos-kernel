/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Virtual memory manager - page allocator
 *
 * Copyright 2026 Phoenix Systems
 * Author: Jakub Klimek
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_VM_PHMAP_H_
#define _PH_VM_PHMAP_H_

#include "hal/hal.h"
#include "include/sysinfo.h"
#include "types.h"

#define PHADDR_INVALID ((addr_t) - 1)

typedef u8 page_flags_t;  // TODO: should it exist? separate commit?


/* allocates continous fragment of size *size or less if unavailable, updates *size accordingly*/
addr_t vm_phAlloc(size_t *size, page_flags_t page_flags, vm_flags_t vm_flags);


int vm_phFree(addr_t addr, size_t size);


// void _phmap_dump(rbnode_t *node);
void phmap_dumpAll(void);


int vm_mappages(pmap_t *pmap, void *vaddr, addr_t pa, size_t size, vm_attr_t attr);


int _page_sbrk(pmap_t *kpmap, void **start, void **end);


void vm_phGetStats(size_t *freesz);


void vm_phinfo(meminfo_t *info);


void _vm_phmap_init(pmap_t *kpmap, void **bss, void **top);


#endif
