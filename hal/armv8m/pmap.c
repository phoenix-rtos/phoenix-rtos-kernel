/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * pmap - machine dependent part of VM subsystem (ARMv8)
 *
 * Copyright 2017, 2020-2022 Phoenix Systems
 * Author: Pawel Pisarczyk, Aleksander Kaminski, Hubert Buczynski, Damian Loewnau
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/pmap.h"
#include <arch/cpu.h>

/* Linker symbols */
extern unsigned int _end;
extern unsigned int __bss_start;

extern void *_init_vectors;


/* Function creates empty page table */
int pmap_create(pmap_t *pmap, pmap_t *kpmap, page_t *p, void *vaddr)
{
	return 0;
}


addr_t pmap_destroy(pmap_t *pmap, int *i)
{
	return 0;
}


int pmap_addMap(pmap_t *pmap, unsigned int map)
{
	return 0;
}


void pmap_switch(pmap_t *pmap)
{
}


int pmap_enter(pmap_t *pmap, addr_t pa, void *vaddr, int attr, page_t *alloc)
{
	return 0;
}


int pmap_remove(pmap_t *pmap, void *vaddr)
{
	return 0;
}


addr_t pmap_resolve(pmap_t *pmap, void *vaddr)
{
	return (addr_t)vaddr;
}


int pmap_isAllowed(pmap_t *pmap, const void *vaddr, size_t size)
{
	return 1;
}


int pmap_getPage(page_t *page, addr_t *addr)
{
	return 0;
}


char pmap_marker(page_t *p)
{
	return 0;
}


int _pmap_kernelSpaceExpand(pmap_t *pmap, void **start, void *end, page_t *dp)
{
	return 0;
}


int pmap_segment(unsigned int i, void **vaddr, size_t *size, int *prot, void **top)
{
	if (i != 0) {
		return -1;
	}

	/* Returns region above basic kernel's .bss section */
	*vaddr = (void *)&_end;
	*size = (((size_t)(*top) + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1)) - (size_t)&_end;

	return 0;
}


void _pmap_init(pmap_t *pmap, void **vstart, void **vend)
{
	(*vstart) = (void *)(((ptr_t)_init_vectors + 7) & ~7u);
	(*vend) = (*((char **)vstart)) + SIZE_PAGE;

	pmap->start = (void *)&__bss_start;

	/* Initial size of kernel map */
	pmap->end = (void *)((addr_t)&__bss_start + 32 * 1024);
}
