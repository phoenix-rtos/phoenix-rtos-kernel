/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Lock definition
 *
 * Copyright 2017 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PROC_LOCK_H_
#define _PROC_LOCK_H_

#include "hal/hal.h"
#include "include/threads.h"


typedef struct _lock_t {
	spinlock_t spinlock;         /* Spinlock */
	struct _thread_t *owner;     /* Owner thread */
	struct _thread_t *queue;     /* Waiting threads */
	struct _lock_t *prev, *next; /* Doubly linked list */
	const char *name;
	struct lockAttr attr;
	unsigned int depth; /* Used with recursive locks */
} lock_t;


extern const struct lockAttr proc_lockAttrDefault;


extern int proc_lockSet(lock_t *lock);


extern int proc_lockSet2(lock_t *lock1, lock_t *lock2);


extern int proc_lockTry(lock_t *lock);


/* `timeout` - in microseconds, absolute time relative to monotonic clock */
extern int proc_lockWait(struct _thread_t **queue, lock_t *lock, time_t timeout);


extern int proc_lockClear(lock_t *lock);


extern int proc_lockSetInterruptible(lock_t *lock);


extern int proc_lockInit(lock_t *lock, const struct lockAttr *attr, const char *name);


extern int proc_lockDone(lock_t *lock);


#endif
