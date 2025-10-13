/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Virtual memory manager - memory object abstraction
 *
 * Copyright 2016-2017 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _VM_OBJECT_H_
#define _VM_OBJECT_H_

#include "hal/hal.h"
#include "lib/lib.h"
#include "proc/lock.h"
#include "amap.h"


struct _vm_map_t;

typedef struct _vm_object_t {
	rbnode_t linkage;
	oid_t oid;
	unsigned int refs;
	size_t size;
	page_t *pages[];
} vm_object_t;


#define VM_OBJ_PHYSMEM ((vm_object_t *)-1)


extern vm_object_t *vm_objectRef(vm_object_t *o);


extern int vm_objectGet(vm_object_t **o, oid_t oid);


extern int vm_objectPut(vm_object_t *o);


extern page_t *vm_objectPage(struct _vm_map_t *map, amap_t **amap, vm_object_t *o, void *vaddr, off_t offs);


extern vm_object_t *vm_objectContiguous(size_t size);


extern int _object_init(struct _vm_map_t *kmap, vm_object_t *kernel);


#endif
