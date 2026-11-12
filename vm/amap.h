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
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_VM_AMAP_H_
#define _PH_VM_AMAP_H_

#include "hal/hal.h"
#include "map.h"

struct _vm_map_t;
struct _vm_object_t;


/* Anonymous page, shared by all amaps referencing it. page is immutable, refs is atomic.
 * refs can only be increased by a holder of the lock of an amap referencing the anon (amap_getanons) while
 * that amap's slot keeps a reference - so a holder of the only reference (refs == 1) can't race with increments */
typedef struct _anon_t {
	int refs;
	page_t *page;
} anon_t;


typedef struct _amap_t {
	lock_t lock;
	size_t size;
	int refs;
	anon_t *anons[];
} amap_t;


int amap_page(struct _vm_map_t *map, amap_t *amap, struct _vm_object_t *o, void *vaddr, size_t aoffs, u64 offs, vm_prot_t prot, page_t **page);


void amap_clear(amap_t *amap, size_t offset, size_t size);


void amap_putanons(amap_t *amap, size_t offset, size_t size);


void amap_getanons(amap_t *amap, size_t offset, size_t size);


amap_t *amap_create(amap_t *amap, size_t *offset, size_t size);


/* Creates an amap with a single anon at offset 0 that takes ownership of page p.
 * On failure p is not consumed. */
amap_t *amap_createPage(page_t *p);


void amap_put(amap_t *amap);


amap_t *amap_ref(amap_t *amap);


void _amap_init(struct _vm_map_t *kmap, struct _vm_object_t *kernel);


#endif
