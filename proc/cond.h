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

#ifndef _PH_PROC_COND_H_
#define _PH_PROC_COND_H_

#include "threads.h"
#include "resource.h"


typedef struct _cond_t {
	resource_t resource;
	thread_t *queue;
	struct condAttr attr;
} cond_t;


void cond_put(cond_t *cond);


cond_t *cond_get(int c);


int proc_condCreate(const struct condAttr *attr);


int proc_condWait(int c, int m, time_t timeout);


int proc_condSignal(int c);


int proc_condBroadcast(int c);


#endif
