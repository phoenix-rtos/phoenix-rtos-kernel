/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * pmap - machine dependent part of VM subsystem (ARMv7 with MPU)
 *
 * Copyright 2017, 2020-2021 Phoenix Systems
 * Author: Pawel Pisarczyk, Aleksander Kaminski, Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "pmap.h"

/* Linker symbols */
extern unsigned int _end;
extern unsigned int __bss_start;

extern void *_init_vectors;


void pmap_switch(pmap_t *pmap)
{
	return;
}


int pmap_remove(pmap_t *pmap, void *vaddr)
{
	return 0;
}


int pmap_enter(pmap_t *pmap, addr_t pa, void *vaddr, int attr, page_t *alloc)
{
	return 0;
}


int pmap_segment(unsigned int i, void **vaddr, size_t *size, int *prot, void **top)
{
	if (i)
		return -1;

	/* Returns region above basic kernel's .bss section */
	*vaddr = (void *)&_end;
	*size = (((size_t)(*top) + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1)) - (size_t)&_end;

	return 0;
}


/* Function creates empty page table */
int pmap_create(pmap_t *pmap, pmap_t *kpmap, page_t *p, void *vaddr)
{
	return 0;
}


void _pmap_init(pmap_t *pmap, void **vstart, void **vend)
{
	(*vstart) = (void *)(((ptr_t)_init_vectors + 7) & ~7);
	(*vend) = (*((char **)vstart)) + SIZE_PAGE;

	pmap->start = (void *)&__bss_start;

	/* Initial size of kernel map */
	pmap->end = (void *)((addr_t)&__bss_start + 32 * 1024);
}
