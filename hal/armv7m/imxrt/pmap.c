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
#include "syspage.h"
#include "spinlock.h"
#include "string.h"
#include "console.h"

#include "../../../../include/errno.h"

struct {
	spinlock_t spinlock;
} pmap_common;


extern void *_init_vectors;


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


addr_t pmap_getMinVAdrr(void)
{
	return (addr_t)syspage->kernel.bss;
}


addr_t pmap_getMaxVAdrr(void)
{
	int i;

	/* Find kernel map end adress */
	for (i = 0; i < syspage->mapssz; ++i) {
		if ((addr_t)syspage->kernel.bss < syspage->maps[i].end && (addr_t)syspage->kernel.bss >= syspage->maps[i].start)
			return syspage->maps[i].end;
	}

	return 0;
}


/* Function creates empty page table */
int pmap_create(pmap_t *pmap, pmap_t *kpmap, page_t *p, void *vaddr)
{
	return EOK;
}


int pmap_getMapParameters(u8 id, void **start, void **end)
{
	int i;

	/* Stop reading parameters */
	if (id >= syspage->mapssz)
		return EOK;

	*start = (void *)syspage->maps[id].start;
	*end = (void *)syspage->maps[id].end;

	if (*end <= *start || ((u32)*start & (SIZE_PAGE - 1)) || ((u32)*end & (SIZE_PAGE - 1)))
		return -EINVAL;

	/* Check if new map overlap with existing one */
	for (i = 0; i < syspage->mapssz; ++i) {
		if (i == id)
			continue;

		if ((*start < (void *)syspage->maps[i].end) && (*end > (void *)syspage->maps[i].start))
			return -EINVAL;
	}

	/* Continue reading map parameters */
	return 1;
}


static void pmap_getMinOverlappedRange(void *memStart, void *memStop, void **segStart, void **segStop, void **minSegStart, void **minSegStop)
{
	if ((memStart < *segStop) && (memStop > *segStart)) {
		if (memStart > *segStart)
			*segStart = memStart;

		if (memStop < *segStop)
			*segStop = memStop;

		if (*segStart < *minSegStart) {
			*minSegStart = *segStart;
			*minSegStop = *segStop;
		}
	}
}


void pmap_getAllocatedSegment(void *memStart, void *memStop, void **estart, void **estop)
{
	int i;
	void *segStart = 0, *segStop = 0;
	void *minSegStart, *minSegStop;

	minSegStart = (void *)-1;
	minSegStop = 0;

	/* Check syspage segment */
	segStart = syspage;
	segStop = ((void *)syspage + syspage->syspagesz);
	pmap_getMinOverlappedRange(memStart, memStop, &segStart, &segStop, &minSegStart, &minSegStop);

	/* Check kernel's .text segment */
	segStart = (void *)(syspage->kernel.text);
	segStop = ((void *)syspage->kernel.text + syspage->kernel.textsz);
	pmap_getMinOverlappedRange(memStart, memStop, &segStart, &segStop, &minSegStart, &minSegStop);

	/* Check kernel's .data segment */
	segStart = (void *)syspage->kernel.data;
	segStop = ((void *)syspage->kernel.data + syspage->kernel.datasz);
	pmap_getMinOverlappedRange(memStart, memStop, &segStart, &segStop, &minSegStart, &minSegStop);

	/* Check programs' segments */
	for (i = 0; i < syspage->progssz; ++i) {
		segStop = (void *)syspage->progs[i].end;
		segStart = (void *)syspage->progs[i].start;
		pmap_getMinOverlappedRange(memStart, memStop, &segStart, &segStop, &minSegStart, &minSegStop);
	}

	if (minSegStart != (void *)-1) {
		*estart = minSegStart;
		*estop = minSegStop;
	}
}


int pmap_getMapsCnt(void)
{
	return syspage->mapssz;
}


void _pmap_init(pmap_t *pmap, void **vstart, void **vend)
{
	(*vstart) = (void *)(((u32)(syspage->kernel.bss + syspage->kernel.bsssz + 1024 + 256) + 7) & ~7);
	(*vend) = (*((char **)vstart)) + SIZE_PAGE;

	pmap->start = (void *)syspage->kernel.bss;
	/* Initial size of kernel map */
	pmap->end = (void *)(syspage->kernel.bss + 32 * 1024);

	hal_spinlockCreate(&pmap_common.spinlock, "pmap_common.spinlock");

	return;
}
