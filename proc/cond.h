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

#include "../hal/hal.h"
#include "threads.h"
#include "resource.h"


typedef struct {
	resource_t resource;
	thread_t *queue;
} cond_t;


extern int cond_put(cond_t *cond);


extern cond_t *cond_get(unsigned int c);


extern int proc_condCreate(void);


extern int proc_condWait(unsigned int c, unsigned int m, time_t timeout);


extern int proc_condSignal(unsigned int c);


extern int proc_condBroadcast(unsigned int c);


#endif
