/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Process resources
 *
 * Copyright 2017, 2018 Phoenix Systems
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


#define resourceof(type, node_field, node) ({					\
	long _off = (long) &(((type *) 0)->node_field);				\
	resource_t *tmpnode = (node);					\
	(type *) ((tmpnode == NULL) ? NULL : ((void *)tmpnode - _off));	\
})


typedef struct {
	oid_t oid;
	offs_t offs;
	unsigned mode;
} fd_t;


enum { rtLock = 0, rtCond, rtFile, rtInth };


typedef struct _resource_t {
	rbnode_t linkage;

	unsigned refs;

	unsigned lgap : 1;
	unsigned rgap : 1;
	unsigned type : 2;
	unsigned id : 28;
} resource_t;


extern unsigned resource_alloc(process_t *process, resource_t *r, int type);


extern resource_t *resource_get(process_t *process, int type, unsigned int id);


extern int resource_put(resource_t *r);


extern void resource_unlink(process_t *process, resource_t *r);


extern resource_t *resource_remove(process_t *process, unsigned id);


extern resource_t *resource_removeNext(process_t *process);


extern void _resource_init(process_t *process);


extern int proc_resourceDestroy(process_t *process, unsigned id);


extern void proc_resourcesDestroy(process_t *process);


extern int proc_resourcesCopy(process_t *source);

#endif
