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
 * SPDX-License-Identifier: BSD-3-Clause
 */

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
			t->process->path, process_getPid(t->process), proc_getTid(t), h);
	return ((r != NULL) && (r->type == rtLock)) ? r->payload.mutex : NULL;
}


void mutex_put(mutex_t *mutex)
{
	thread_t *t = proc_current();
	int rem;

	LIB_ASSERT(mutex != NULL, "process: %s, pid: %d, tid: %d, mutex == NULL",
			t->process->path, process_getPid(t->process), proc_getTid(t));

	rem = resource_put(t->process, &mutex->resource);
	LIB_ASSERT(rem >= 0, "process: %s, pid: %d, tid: %d, refcnt below zero",
			t->process->path, process_getPid(t->process), proc_getTid(t));
	if (rem == 0) {
		(void)proc_lockDone(&mutex->lock);
		vm_kfree(mutex);
	}
}


int proc_mutexCreate(const struct lockAttr *attr)
{
	process_t *p = proc_current()->process;
	mutex_t *mutex;
	int id;

	if ((attr->type != PH_LOCK_NORMAL) && (attr->type != PH_LOCK_RECURSIVE) && (attr->type != PH_LOCK_ERRORCHECK)) {
		return -EINVAL;
	}

	if ((attr->robust != PH_LOCK_STALLED) && (attr->robust != PH_LOCK_ROBUST)) {
		return -EINVAL;
	}

	if ((attr->protocol != PH_LOCK_PROTO_INHERIT) && (attr->protocol != PH_LOCK_PROTO_NOINHERIT) && (attr->protocol != PH_LOCK_PROTO_PRIOCEILING)) {
		return -EINVAL;
	}

	if (attr->prioceiling > MAX_PRIO) {
		return -EINVAL;
	}

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

	(void)proc_lockInit(&mutex->lock, attr, "user.mutex");

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


int proc_mutexConsistent(int h)
{
	mutex_t *mutex;
	int err;

	mutex = mutex_get(h);
	if (mutex == NULL) {
		return -EINVAL;
	}

	err = proc_lockConsistent(&mutex->lock);

	mutex_put(mutex);

	return err;
}


int proc_mutexPrioCeiling(int h, int prioceiling)
{
	mutex_t *mutex;
	int err;

	mutex = mutex_get(h);
	if (mutex == NULL) {
		return -EINVAL;
	}

	err = proc_lockPrioCeiling(&mutex->lock, prioceiling);

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
