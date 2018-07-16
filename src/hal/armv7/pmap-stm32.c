/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * pmap - machine dependent part of VM subsystem (ARMv7 with MPU)
 *
 * Copyright 2017 Phoenix Systems
 * Author: Pawel Pisarczyk
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
	mpur_t reg;
	int i;
	int free = -1;
	const u32 mask = (SIZE_PAGE << 3) - 1;
	int toPurge = 0;
	int toPurgeRes = 0;
	int toPurgeTmp = 0;
	u8 t;

	attr &= PGHD_EXEC | PGHD_WRITE | PGHD_USER;

/*if ((vaddr < (void *)pmap_common.minaddr) || (vaddr >= (void *)pmap_common.maxaddr))
	return -EINVAL;*/

pa = (addr_t)vaddr;

	hal_spinlockSet(&pmap_common.spinlock);

	for (i = 0; i < 8; ++i) {
		_stm32_mpuReadRegion(i, &reg);

		/* If region is free, save it for later */
		if (!(reg.attr & PGHD_PRESENT)) {
			if (free < 0)
				free = i;
			continue;
		}

		if (reg.size != SIZE_PAGE << 3) {
			/* Region is corrupted; disable it */
			_stm32_mpuEnableRegion(reg.region, 0);
			if (free < 0)
				free = i;
			continue;
		}

		/* Check if base address match */
		if (reg.base != (pa & ~(mask)))
			continue;

		/* Check if access rights match */
		if (attr != (reg.attr & (PGHD_EXEC | PGHD_WRITE | PGHD_USER)))
			continue;

		/* Everything matches; add subregion to the region */
		reg.subregions &= ~(1 << ((pa & mask) / SIZE_PAGE));
		_stm32_mpuUpdateRegion(&reg);

		hal_spinlockClear(&pmap_common.spinlock);
		return EOK;
	}

	if (free >= 0) {
		/* Fill new region */
		reg.region = free;
		reg.base = pa & ~(mask);
		reg.size = SIZE_PAGE << 3;
		reg.attr = attr | PGHD_PRESENT;
		reg.subregions = 0xff & ~(1 << ((pa & mask) / SIZE_PAGE));
		_stm32_mpuUpdateRegion(&reg);

		hal_spinlockClear(&pmap_common.spinlock);
		return EOK;
	}

	/* Could not find suitable region to expand or create. Purge existing one */
	for (i = 0; i < 8; ++i) {
		/* Find region to overwrite */
		_stm32_mpuReadRegion(i, &reg);
		t = reg.subregions;
		toPurgeTmp = 0;
		while (t != 0) {
			toPurgeTmp += t & 1;
			t >>= 1;
		}

		if (toPurgeTmp > toPurgeRes) {
			toPurgeRes = toPurgeTmp;
			toPurge = i;
		}
	}

	reg.region = toPurge;
	reg.base = pa & ~(mask);
	reg.size = SIZE_PAGE << 3;
	reg.attr = attr | PGHD_PRESENT;
	reg.subregions = 0xff & ~(1 << ((pa & mask) / SIZE_PAGE));
	_stm32_mpuUpdateRegion(&reg);

	hal_spinlockClear(&pmap_common.spinlock);
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

	hal_spinlockCreate(&pmap_common.spinlock, "pmap_common.spinlock");

	return;
}
