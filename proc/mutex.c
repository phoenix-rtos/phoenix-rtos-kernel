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
#include "mutex.h"
#include "threads.h"
#include "resource.h"


mutex_t *mutex_get(unsigned int h)
{
	return resourceof(mutex_t, resource, resource_get(proc_current()->process, rtLock, h));
}


int mutex_put(mutex_t *mutex)
{
	int rem = resource_put(&mutex->resource);

	if (rem == 0) {
		proc_lockDone(&mutex->lock);
		vm_kfree(mutex);
	}

	return rem;
}


int proc_mutexCreate(void)
{
	process_t *process;
	mutex_t *mutex;
	int id;

	process = proc_current()->process;
	mutex = vm_kmalloc(sizeof(mutex_t));
	if (mutex == NULL) {
		return -ENOMEM;
	}

	id = resource_alloc(process, &mutex->resource, rtLock);
	if (id == 0) {
		vm_kfree(mutex);
		id = -ENOMEM;
	}
	else {
		proc_lockInit(&mutex->lock, "user.mutex");
		resource_put(&mutex->resource);
	}

	return id;
}


int proc_mutexLock(unsigned int h)
{
	mutex_t *mutex;
	int err = EOK;

	mutex = mutex_get(h);
	if (mutex == NULL) {
		return -EINVAL;
	}

	err = proc_lockSetInterruptible(&mutex->lock);

	if (mutex_put(mutex) == 0) {
		err = -EINVAL;
	}

	return err;
}


int proc_mutexTry(unsigned int h)
{
	mutex_t *mutex;
	int err = EOK;

	mutex = mutex_get(h);
	if (mutex == NULL) {
		return -EINVAL;
	}

	err = proc_lockTry(&mutex->lock);

	if (mutex_put(mutex) == 0) {
		err = -EINVAL;
	}

	return err;
}


int proc_mutexUnlock(unsigned int h)
{
	mutex_t *mutex;
	int err = EOK;

	mutex = mutex_get(h);
	if (mutex == NULL) {
		return -EINVAL;
	}

	err = proc_lockClear(&mutex->lock);

	if (mutex_put(mutex) == 0) {
		err = -EINVAL;
	}

	return err;
}
