/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * pmap interface - machine dependent part of VM subsystem
 *
 * Copyright 2014, 2018 Phoenix Systems
 * Author: Jacek Popko, Pawel Pisarczyk, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_PMAP_H_
#define _PH_HAL_PMAP_H_

#include "vm/types.h"
#include "lib/attrs.h"

#include <arch/pmap.h>
#include "syspage.h"


#ifndef NOMMU

MAYBE_UNUSED static inline int pmap_belongs(pmap_t *pmap, void *addr)
{
	return ((ptr_t)addr >= (ptr_t)pmap->start && (ptr_t)addr < (ptr_t)pmap->end) ? 1 : 0;
}

#else

int pmap_isAllowed(pmap_t *pmap, const void *vaddr, size_t size);

#endif


int pmap_create(pmap_t *pmap, pmap_t *kpmap, page_t *p, const syspage_prog_t *prog, void *vaddr);


addr_t pmap_destroy(pmap_t *pmap, unsigned int *i);


void pmap_switch(pmap_t *pmap);


int pmap_enter(pmap_t *pmap, addr_t paddr, void *vaddr, vm_attr_t attr, page_t *alloc);


/* Function removes mapping in range [vstart, vend) */
int pmap_remove(pmap_t *pmap, void *vstart, void *vend);


addr_t pmap_resolve(pmap_t *pmap, void *vaddr);


int pmap_getPage(page_t *page, addr_t *addr);


/* Function return character marker for page flags */
char pmap_marker(page_t *p);


/* Function allocates page tables for kernel space */
int _pmap_kernelSpaceExpand(pmap_t *pmap, void **start, void *end, page_t *dp);


int pmap_segment(unsigned int i, void **vaddr, size_t *size, vm_prot_t *prot, void **top);


void _pmap_init(pmap_t *pmap, void **vstart, void **vend);

#endif
