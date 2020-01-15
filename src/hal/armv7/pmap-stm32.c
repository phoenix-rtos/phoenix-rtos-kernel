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
#include "stm32.h"

#include "../../../include/errno.h"


void pmap_switch(pmap_t *pmap)
{
}


int pmap_remove(pmap_t *pmap, void *vaddr)
{
	return EOK;
}


int pmap_enter(pmap_t *pmap, addr_t pa, void *vaddr, int attr, page_t *alloc)
{
	return EOK;
}


/* Function creates empty page table */
int pmap_create(pmap_t *pmap, pmap_t *kpmap, page_t *p, void *vaddr)
{
	return EOK;
}


extern void *_end;


void _pmap_init(pmap_t *pmap, void **vstart, void **vend)
{
	(*vstart) = *(void **) ((hal_cpuGetPC() < 0x08030000) ? 0x08000000 : 0x08030000);
	(*vend) = (*vstart) + SIZE_PAGE;

	pmap->start = (void *)VADDR_KERNEL;
	pmap->end = (void *)VADDR_MAX;

	return;
}
