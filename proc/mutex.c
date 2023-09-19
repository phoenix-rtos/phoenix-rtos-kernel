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

#include "hal/hal.h"
#include "include/errno.h"
#include "lib/assert.h"
#include "mutex.h"
#include "threads.h"
#include "resource.h"


mutex_t *mutex_get(int h)
{
	thread_t *t = proc_current();
	resource_t *r = resource_get(t->process, h);
	LIB_ASSERT((r == NULL) || (r->type == rtLock), "process: %s, pid: %d, tid: %d, handle: %d, resource type mismatch",
		t->process->path, t->process->id, t->id, h);
	return ((r != NULL) && (r->type == rtLock)) ? r->payload.mutex : NULL;
}


void mutex_put(mutex_t *mutex)
{
	thread_t *t = proc_current();
	int rem;

	LIB_ASSERT(mutex != NULL, "process: %s, pid: %d, tid: %d, mutex == NULL",
		t->process->path, t->process->id, t->id);

	rem = resource_put(t->process, &mutex->resource);
	LIB_ASSERT(rem >= 0, "process: %s, pid: %d, tid: %d, refcnt below zero",
		t->process->path, t->process->id, t->id);
	if (rem <= 0) {
		proc_lockDone(&mutex->lock);
		vm_kfree(mutex);
	}
}


int proc_mutexCreate(void)
{
	process_t *p = proc_current()->process;
	mutex_t *mutex;
	int id;

	mutex = vm_kmalloc(sizeof(*mutex));
	if (mutex == NULL) {
		return -ENOMEM;
	}

	mutex->resource.payload.mutex = mutex;
	mutex->resource.type = rtLock;

	id = resource_alloc(p, &mutex->resource);
	if (id < 0) {
		vm_kfree(mutex);
		return -ENOMEM;
	}

	proc_lockInit(&mutex->lock, "user.mutex");

	(void)resource_put(p, &mutex->resource);

	return id;
}


int proc_mutexLock(int h)
{
	mutex_t *mutex;
	int err;

	mutex = mutex_get(h);
	if (mutex == NULL) {
		return -EINVAL;
	}

	err = proc_lockSetInterruptible(&mutex->lock);

	mutex_put(mutex);

	return err;
}


int proc_mutexTry(int h)
{
	mutex_t *mutex;
	int err;

	mutex = mutex_get(h);
	if (mutex == NULL) {
		return -EINVAL;
	}

	err = proc_lockTry(&mutex->lock);

	mutex_put(mutex);

	return err;
}


int proc_mutexUnlock(int h)
{
	mutex_t *mutex;
	int err;

	mutex = mutex_get(h);
	if (mutex == NULL) {
		return -EINVAL;
	}

	err = proc_lockClear(&mutex->lock);

	mutex_put(mutex);

	return err;
}
