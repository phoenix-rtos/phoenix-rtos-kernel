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

#ifndef _VM_AMAP_H_
#define _VM_AMAP_H_

#include HAL
#include "map.h"

struct _vm_map_t;
struct _vm_object_t;


typedef struct _anon_t {
	lock_t lock;
	unsigned int refs;
	page_t *page;
} anon_t;


typedef struct _amap_t {
	lock_t lock;
	unsigned int refs, size;
	anon_t *anons[];
} amap_t;


extern page_t *amap_page(struct _vm_map_t *map, amap_t *amap, struct _vm_object_t *o, void *vaddr, int aoffs, int offs, int prot);


extern void amap_putanons(amap_t *amap, int offs, int size);


extern void amap_getanons(amap_t *amap, int offs, int size);


extern amap_t *amap_create(amap_t *amap, int *offset, size_t size);


extern void amap_put(amap_t *amap);


extern amap_t *amap_ref(amap_t *amap);


extern void _amap_init(struct _vm_map_t *kmap, struct _vm_object_t *kernel);


#endif
