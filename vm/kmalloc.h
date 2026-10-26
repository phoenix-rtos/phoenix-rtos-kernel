/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Fine-grained memory allocator
 *
 * Copyright 2012, 2017 Phoenix Systems
 * Copyright 2001, 2005 Pawel Pisarczyk
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_VM_KMALLOC_H_
#define _PH_VM_KMALLOC_H_

#include "hal/hal.h"


void *vm_kmalloc(size_t size);


static inline void *vm_kmallocObjects(size_t nObjs, size_t objSize, size_t extra)
{
	size_t bytes;
	if (__builtin_mul_overflow(nObjs, objSize, &bytes)) {
		return NULL;
	}

	if (__builtin_add_overflow(bytes, extra, &bytes)) {
		return NULL;
	}

	return vm_kmalloc(bytes);
}


void vm_kfree(void *p);


void vm_kmallocGetStats(size_t *allocsz);


void vm_kmallocDump(void);


int _kmalloc_init(void);


#endif
