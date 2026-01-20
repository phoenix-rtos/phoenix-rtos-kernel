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

#ifndef _PH_VM_ZALLOC_H_
#define _PH_VM_ZALLOC_H_

#include "lib/lib.h"


typedef struct _vm_zone_t {
	struct _vm_zone_t *next;
	struct _vm_zone_t *prev;

	rbnode_t linkage;

	size_t blocksz;
	unsigned int blocks;
	unsigned int used;
	void *vaddr;
	void *first;
	addr_t pages;

	/*	u8 padd[1024]; */
} vm_zone_t;


int _vm_zoneCreate(vm_zone_t *zone, size_t blocksz, unsigned int blocks);


int _vm_zoneDestroy(vm_zone_t *zone);


void *_vm_zalloc(vm_zone_t *zone, addr_t *addr);


void _vm_zfree(vm_zone_t *zone, void *block);


void _zone_init(vm_map_t *map, vm_object_t *kernel, void **bss, void **top);


#endif
