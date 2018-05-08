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


extern int proc_mutexLock(unsigned int h);


extern int proc_mutexUnlock(unsigned int h);


extern int proc_mutexCreate(unsigned int *h);


extern int proc_mutexDestroy(unsigned int h);


#endif
