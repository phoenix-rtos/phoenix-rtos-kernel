/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Virtual memory manager - memory mapper
 *
 * Copyright 2016 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _VM_MAP_H_
#define _VM_MAP_H_

#include "hal/hal.h"
#include "include/sysinfo.h"
#include "include/mman.h"
#include "lib/lib.h"
#include "syspage.h"
#include "object.h"
#include "proc/lock.h"
#include "vm/amap.h"


struct _amap_t;
struct _vm_object_t;


typedef struct _vm_map_t {
	pmap_t pmap;
	void *start;
	void *stop;
	rbtree_t tree;
	lock_t lock;
} vm_map_t;


struct _process_t;


typedef struct _map_entry_t {
#ifndef NOMMU
	union {
		rbnode_t linkage;
		struct _map_entry_t *next;
	};
#else
	rbnode_t linkage;
	struct _map_entry_t *next;
	struct _map_entry_t *prev;
	struct _process_t *process;
#endif

	vm_map_t *map;

	int aoffs;
	struct _amap_t *amap;

	void *vaddr;
	size_t size;
	size_t lmaxgap;
	size_t rmaxgap;

	unsigned short flags;
	unsigned short prot;
	struct _vm_object_t *object;
	offs_t offs;
} map_entry_t;


extern void *vm_mapFind(vm_map_t *map, void *vaddr, size_t size, u8 flags, u8 prot);


extern void *vm_mmap(vm_map_t *map, void *vaddr, page_t *p, size_t size, u8 prot, struct _vm_object_t *o, offs_t offs, u8 flags);


extern void *_vm_mmap(vm_map_t *map, void *vaddr, page_t *p, size_t size, u8 prot, struct _vm_object_t *o, offs_t offs, u8 flags);


extern int vm_mapForce(vm_map_t *map, void *vaddr, int prot);


extern int vm_mapFlags(vm_map_t *map, void *vaddr);


extern int vm_lockVerify(vm_map_t *map, struct _amap_t **amap, struct _vm_object_t *o, void *vaddr, offs_t offs);


extern int vm_munmap(vm_map_t *map, void *vaddr, size_t size);


extern int _vm_munmap(vm_map_t *map, void *vaddr, size_t size);


extern void vm_mapDump(vm_map_t *map);


extern int vm_mapCreate(vm_map_t *map, void *start, void *stop);


extern int vm_mapCopy(struct _process_t *process, vm_map_t *dst, vm_map_t *src);


extern void vm_mapDestroy(struct _process_t *p, vm_map_t *map);


extern void vm_mapGetStats(size_t *allocsz);


extern void vm_mapinfo(meminfo_t *info);


extern int vm_createSharedMap(ptr_t start, ptr_t stop, unsigned int attr, int no);


extern vm_map_t *vm_getSharedMap(int map);


extern int _map_init(vm_map_t *kmap, struct _vm_object_t *kernel, void **start, void **end);


#endif
