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
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _VM_KMALLOC_H_
#define _VM_KMALLOC_H_

#include "hal/hal.h"


void *vm_kmalloc(size_t size);


void vm_kfree(void *p);


void vm_kmallocGetStats(size_t *allocsz);


void vm_kmallocDump(void);


int _kmalloc_init(void);


#endif
