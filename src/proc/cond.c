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

#include HAL
#include "../../include/errno.h"
#include "threads.h"
#include "cond.h"
#include "mutex.h"
#include "resource.h"


int cond_put(cond_t *cond)
{
	int rem;

	if (!(rem = resource_put(&cond->resource)))
		vm_kfree(cond);

	return rem;
}


cond_t *cond_get(unsigned int c)
{
	return resourceof(cond_t, resource, resource_get(proc_current()->process, rtCond, c));
}


int proc_condCreate()
{
	process_t *process;
	cond_t *cond;
	int id;

	process = proc_current()->process;

	if ((cond = vm_kmalloc(sizeof(cond_t))) == NULL)
		return -ENOMEM;

	if (!(id = resource_alloc(process, &cond->resource, rtCond))) {
		vm_kfree(cond);
		id = -ENOMEM;
	}
	else {
		cond->queue = NULL;
		resource_put(&cond->resource);
	}

	return id;
}


int proc_condWait(unsigned int c, unsigned int m, time_t timeout)
{
	cond_t *cond;
	mutex_t *mutex;
	int err = -EINVAL;

	if ((cond = cond_get(c)) == NULL)
		return err;

	if ((mutex = mutex_get(m)) != NULL) {
		err = proc_lockWait(&cond->queue, &mutex->lock, timeout);

		if (!mutex_put(mutex))
			err = -EINVAL;
	}

	if (!cond_put(cond))
		err = -EINVAL;

	return err;
}


int proc_condSignal(unsigned int c)
{
	cond_t *cond;
	int err = EOK;

	if ((cond = cond_get(c)) == NULL)
		return -EINVAL;

	proc_threadWakeupYield(&cond->queue);

	if (!cond_put(cond))
		err = -EINVAL;

	return err;
}


int proc_condBroadcast(unsigned int c)
{
	cond_t *cond;
	int err = EOK;

	if ((cond = cond_get(c)) == NULL)
		return -EINVAL;

	proc_threadBroadcastYield(&cond->queue);

	if (!cond_put(cond))
		err = -EINVAL;

	return err;
}
