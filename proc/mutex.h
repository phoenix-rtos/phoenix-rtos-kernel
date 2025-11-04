/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Mutexes
 *
 * Copyright 2017 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PROC_MUTEX_H_
#define _PROC_MUTEX_H_

#include "include/threads.h"
#include "resource.h"


typedef struct _mutex_t {
	resource_t resource;
	lock_t lock;
} mutex_t;


mutex_t *mutex_get(int h);


void mutex_put(mutex_t *mutex);


int proc_mutexLock(int h);


int proc_mutexTry(int h);


int proc_mutexUnlock(int h);


int proc_mutexCreate(const struct lockAttr *attr);


#endif
