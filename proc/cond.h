/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Conditionals
 *
 * Copyright 2017 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PROC_COND_H_
#define _PROC_COND_H_

#include "threads.h"
#include "resource.h"


typedef struct _cond_t {
	resource_t resource;
	thread_t *queue;
	struct condAttr attr;
} cond_t;


extern void cond_put(cond_t *cond);


extern cond_t *cond_get(int c);


extern int proc_condCreate(const struct condAttr *attr);


extern int proc_condWait(int c, int m, time_t timeout);


extern int proc_condSignal(int c);


extern int proc_condBroadcast(int c);


#endif
