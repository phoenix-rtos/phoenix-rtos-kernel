/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Virtual memory manager
 *
 * Copyright 2012 Phoenix Systems
 * Copyright 2001, 2005 Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _VM_VM_H_
#define _VM_VM_H_

#include "../hal/hal.h"
#include "../include/sysinfo.h"
#include "page.h"
#include "map.h"
#include "zone.h"
#include "kmalloc.h"
#include "object.h"
#include "amap.h"


extern void vm_meminfo(meminfo_t *info);


/* Function initializes virtual memory manager */
extern void _vm_init(vm_map_t *kmap, vm_object_t *kernel);


#endif
