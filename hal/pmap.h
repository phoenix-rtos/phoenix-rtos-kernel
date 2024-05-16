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

#ifndef _HAL_PMAP_H_
#define _HAL_PMAP_H_

#include <arch/pmap.h>


static inline int pmap_belongs(pmap_t *pmap, void *addr)
{
	return addr >= pmap->start && addr < pmap->end;
}


extern int pmap_create(pmap_t *pmap, pmap_t *kpmap, page_t *p, void *vaddr);


extern addr_t pmap_destroy(pmap_t *pmap, int *i);


/* Available only on NOMMU */
extern int pmap_addMap(pmap_t *pmap, unsigned int map);


extern void pmap_switch(pmap_t *pmap);


extern int pmap_enter(pmap_t *pmap, addr_t addr, void *vaddr, int attrs, page_t *alloc);


/* Function removes mapping for given address */
extern int pmap_remove(pmap_t *pmap, void *vaddr);


extern addr_t pmap_resolve(pmap_t *pmap, void *vaddr);


extern int pmap_getPage(page_t *page, addr_t *addr);


/* Function return character marker for page flags */
extern char pmap_marker(page_t *p);


/* Function allocates page tables for kernel space */
extern int _pmap_kernelSpaceExpand(pmap_t *pmap, void **start, void *end, page_t *dp);


extern int pmap_segment(unsigned int i, void **vaddr, size_t *size, int *prot, void **top);


extern void _pmap_init(pmap_t *pmap, void **start, void **end);

#endif
