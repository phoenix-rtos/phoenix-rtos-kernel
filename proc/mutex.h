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

#include "threads.h"
#include "resource.h"


typedef struct _mutex_t {
	resource_t resource;
	lock_t lock;
} mutex_t;


extern mutex_t *mutex_get(int h);


extern void mutex_put(mutex_t *mutex);


extern int proc_mutexLock(int h);


extern int proc_mutexTry(int h);


extern int proc_mutexUnlock(int h);


extern int proc_mutexCreate(void);


extern int proc_mutexDestroy(int h);


#endif
