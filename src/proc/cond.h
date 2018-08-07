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


#include HAL
#include "threads.h"
#include "resource.h"


extern int proc_condCreate(unsigned int *h);


extern int proc_condWait(unsigned int c, unsigned int m, time_t timeout);


extern int proc_condSignal(process_t *process, unsigned int c);


extern int proc_condBroadcast(process_t *process, unsigned int c);


extern int proc_condCopy(resource_t *dst, resource_t *src);

#endif
