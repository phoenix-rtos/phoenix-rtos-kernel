/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Semaphore imlementation
 *
 * Copyright 2012, 2017 Phoenix Systems
 * Copyright 2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PROC_SEMAPHORE_H_
#define _PROC_SEMAPHORE_H_

#include HAL
#include "threads.h"


typedef struct _semaphore_t {
	spinlock_t spinlock;
	volatile unsigned int v;
	thread_t *queue;
} semaphore_t;


#endif
