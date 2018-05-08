/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Virtual memory manager - zone allocator
 *
 * Copyright 2014, 2016-2017 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include HAL
#include "../../include/errno.h"
#include "../lib/lib.h"
#include "page.h"
#include "map.h"
#include "zone.h"


struct {
	vm_map_t *kmap;
	vm_object_t *kernel;
} zone_common;


int _vm_zoneCreate(vm_zone_t *zone, size_t blocksz, unsigned int blocks)
{
	unsigned int i;

	if ((blocksz == 0) || (blocks == 0))
		return -EINVAL;

	if (!(blocksz & ~(blocksz - 1)))
		return -EINVAL;

	if ((zone->pages = vm_pageAlloc(blocks * blocksz, PAGE_OWNER_KERNEL | PAGE_KERNEL_HEAP)) == NULL)
 		return -ENOMEM;

	if ((zone->vaddr = vm_mmap(zone_common.kmap, zone_common.kmap->start, zone->pages, 1 << zone->pages->idx, PROT_READ | PROT_WRITE, zone_common.kernel, -1, MAP_NONE)) == NULL) {
		vm_pageFree(zone->pages);
		return -ENOMEM;
	}

	/* Prepare zone for allocations */
	for (i = 0; i < blocks; i++)
		*((void **)(zone->vaddr + i * blocksz)) = zone->vaddr + (i + 1) * blocksz;
	*((void **)(zone->vaddr + (blocks - 1)  * blocksz)) = NULL;

	zone->first = zone->vaddr;
	zone->blocks = blocks;
	zone->blocksz = blocksz;
	zone->used = 0;

	return EOK;
}


int _vm_zoneDestroy(vm_zone_t *zone)
{
	if (zone == NULL)
		return -EINVAL;

	if (zone->used)
		return -EBUSY;

	vm_munmap(zone_common.kmap, zone->vaddr, 1 << zone->pages->idx);
	vm_pageFree(zone->pages);

	zone->vaddr = NULL;
	zone->first = NULL;
	zone->pages = NULL;

	return EOK;
}


void *_vm_zalloc(vm_zone_t *zone, addr_t *addr)
{
	void *block;

	if (zone == NULL)
		return NULL;

	if (zone->used == zone->blocks)
		return NULL;

	block = zone->first;
	zone->first = *((void **)(zone->first));
	zone->used++;

	if (addr != NULL)
		*addr = zone->pages->addr + (block - zone->vaddr);

	return block;
}


void _vm_zfree(vm_zone_t *zone, void *block)
{
	if ((block < zone->vaddr) || (block >= zone->vaddr + zone->blocksz * zone->blocks))
		return;

	if (!((unsigned long)block & ~(zone->blocksz - 1)))
		return;

	*((void **)block) = zone->first;
	zone->first = block;
	zone->used--;

	return;
}


void _zone_init(vm_map_t *map, vm_object_t *kernel, void **bss, void **top)
{
	zone_common.kmap = map;
	zone_common.kernel = kernel;
}
