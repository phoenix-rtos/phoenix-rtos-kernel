/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Virtual memory manager - amap abstraction
 *
 * Copyright 2017 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_VM_AMAP_H_
#define _PH_VM_AMAP_H_

#include "hal/hal.h"
#include "map.h"

struct _vm_map_t;
struct _vm_object_t;
struct _partition_t;


typedef struct _anon_t {
	lock_t lock;
	int refs;
	page_t *page;
} anon_t;


typedef struct _amap_t {
	lock_t lock;
	struct _partition_t *partition;
	size_t size;
	int refs;
	anon_t *anons[];
} amap_t;


page_t *amap_page(struct _vm_map_t *map, amap_t *amap, struct _vm_object_t *o, void *vaddr, size_t aoffs, u64 offs, vm_prot_t prot);


void amap_clear(amap_t *amap, size_t offset, size_t size);


void amap_putanons(amap_t *amap, size_t offset, size_t size);


void amap_getanons(amap_t *amap, size_t offset, size_t size);


amap_t *amap_create(amap_t *amap, size_t *offset, size_t size, struct _partition_t *part);


void amap_put(amap_t *amap);


amap_t *amap_ref(amap_t *amap);


void _amap_init(struct _vm_map_t *kmap, struct _vm_object_t *kernel);


#endif
