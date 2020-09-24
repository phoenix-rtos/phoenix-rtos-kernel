/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * pmap - machine dependent part of VM subsystem (ARMv7 with MPU)
 *
 * Copyright 2017, 2020 Phoenix Systems
 * Author: Pawel Pisarczyk, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "pmap.h"
#include "spinlock.h"
#include "string.h"
#include "console.h"

#include "../../../../include/errno.h"

struct {
	spinlock_t spinlock;
} pmap_common;


void pmap_switch(pmap_t *pmap)
{
}


int pmap_remove(pmap_t *pmap, void *vaddr)
{
	return EOK;
}


int pmap_enter(pmap_t *pmap, addr_t pa, void *vaddr, int attr, page_t *alloc)
{
	/* TODO */

	return EOK;
}


/* Function creates empty page table */
int pmap_create(pmap_t *pmap, pmap_t *kpmap, page_t *p, void *vaddr)
{
	return EOK;
}


extern void *_init_vectors;


void _pmap_init(pmap_t *pmap, void **vstart, void **vend)
{
	(*vstart) = (void *)(((u32)_init_vectors + 7) & ~7);
	(*vend) = (*((char **)vstart)) + SIZE_PAGE;

	pmap->start = (void *)VADDR_KERNEL;
	pmap->end = (void *)(VADDR_KERNEL + VADDR_KERNELSZ);

	hal_spinlockCreate(&pmap_common.spinlock, "pmap_common.spinlock");

	return;
}
