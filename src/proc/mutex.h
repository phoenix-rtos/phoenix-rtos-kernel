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

#include "resource.h"

typedef struct {
	resource_t resource;
	lock_t lock;
} mutex_t;


extern mutex_t *mutex_get(unsigned int h);


extern int mutex_put(mutex_t *mutex);


extern int proc_mutexLock(unsigned int h);


extern int proc_mutexTry(unsigned int h);


extern int proc_mutexUnlock(unsigned int h);


extern int proc_mutexCreate(void);


extern int proc_mutexDestroy(unsigned int h);


#endif
