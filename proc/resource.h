/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Process resources
 *
 * Copyright 2017, 2018, 2023 Phoenix Systems
 * Author: Pawel Pisarczyk, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PROC_RESOURCE_H_
#define _PROC_RESOURCE_H_

#include "hal/hal.h"
#include "threads.h"
#include "lib/idtree.h"


struct _mutex_t;
struct _cond_t;
struct _usrintr_t;


typedef struct _resource_t {
	idnode_t linkage;
	unsigned int refs;
	/* clang-format off */
	enum { rtLock = 0, rtCond, rtInth } type;
	/* clang-format on */

	union {
		struct _cond_t *cond;
		struct _mutex_t *mutex;
		struct _userintr_t *userintr;
	} payload;
} resource_t;


extern int resource_alloc(process_t *process, resource_t *r);


extern resource_t *resource_get(process_t *process, int id);


extern unsigned int resource_put(process_t *process, resource_t *r);


extern int proc_resourceDestroy(process_t *process, int id);


extern void proc_resourcesDestroy(process_t *process);


extern int proc_resourcesCopy(process_t *source);


extern void _resource_init(process_t *process);

#endif
