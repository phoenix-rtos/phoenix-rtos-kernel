/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Conditionals
 *
 * Copyright 2017, 2023 Phoenix Systems
 * Author: Pawel Pisarczyk, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "lib/assert.h"
#include "include/errno.h"
#include "include/time.h"
#include "threads.h"
#include "cond.h"
#include "mutex.h"
#include "resource.h"


cond_t *cond_get(int c)
{
	thread_t *t = proc_current();
	resource_t *r = resource_get(t->process, c);
	LIB_ASSERT((r == NULL) || (r->type == rtCond), "process: %s, pid: %d, tid: %d, handle: %d, resource type mismatch",
			t->process->path, process_getPid(t->process), proc_getTid(t), c);
	return ((r != NULL) && (r->type == rtCond)) ? r->payload.cond : NULL;
}


void cond_put(cond_t *cond)
{
	thread_t *t = proc_current();
	int rem;

	LIB_ASSERT(cond != NULL, "process: %s, pid: %d, tid: %d, cond == NULL",
			t->process->path, process_getPid(t->process), proc_getTid(t));

	rem = resource_put(t->process, &cond->resource);
	LIB_ASSERT(rem >= 0, "process: %s, pid: %d, tid: %d, refcnt below zero",
			t->process->path, process_getPid(t->process), proc_getTid(t));
	if (rem == 0) {
		proc_threadBroadcastYield(&cond->queue);
		vm_kfree(cond);
	}
}


int proc_condCreate(const struct condAttr *attr)
{
	process_t *p = proc_current()->process;
	cond_t *cond;
	int id;

	if ((attr->clock != PH_CLOCK_RELATIVE) && (attr->clock != PH_CLOCK_REALTIME) && (attr->clock != PH_CLOCK_MONOTONIC)) {
		return -EINVAL;
	}

	cond = vm_kmalloc(sizeof(*cond));
	if (cond == NULL) {
		return -ENOMEM;
	}

	cond->resource.payload.cond = cond;
	cond->resource.type = rtCond;

	id = resource_alloc(p, &cond->resource);
	if (id < 0) {
		vm_kfree(cond);
		return -ENOMEM;
	}

	cond->queue = NULL;

	hal_memcpy(&cond->attr, attr, sizeof(cond->attr));

	(void)resource_put(p, &cond->resource);

	return id;
}


int proc_condWait(int c, int m, time_t timeout)
{
	cond_t *cond;
	mutex_t *mutex;
	int err = 0;
	time_t offs, abstime = 0;

	cond = cond_get(c);
	if (cond == NULL) {
		return -EINVAL;
	}

	mutex = mutex_get(m);
	if (mutex == NULL) {
		cond_put(cond);
		return -EINVAL;
	}

	if (timeout != 0) {
		switch (cond->attr.clock) {
			case PH_CLOCK_REALTIME:
				proc_gettime(&abstime, &offs);
				if (abstime + offs > timeout) {
					err = -ETIME;
					break;
				}

				abstime = timeout - offs;
				break;

			case PH_CLOCK_MONOTONIC:
				proc_gettime(&abstime, NULL);
				if (abstime > timeout) {
					err = -ETIME;
					break;
				}

				abstime = timeout;
				break;

			case PH_CLOCK_RELATIVE:
				proc_gettime(&abstime, NULL);
				abstime += timeout;
				break;

			default:
				/* Wrong clock type, should never happen */
				err = -EINVAL;
				break;
		}
	}

	if (err == 0) {
		err = proc_lockWait(&cond->queue, &mutex->lock, abstime);
	}

	mutex_put(mutex);
	cond_put(cond);

	return err;
}


int proc_condSignal(int c)
{
	cond_t *cond;

	cond = cond_get(c);
	if (cond == NULL) {
		return -EINVAL;
	}

	proc_threadWakeupYield(&cond->queue);

	cond_put(cond);

	return EOK;
}


int proc_condBroadcast(int c)
{
	cond_t *cond;

	cond = cond_get(c);
	if (cond == NULL) {
		return -EINVAL;
	}

	proc_threadBroadcastYield(&cond->queue);

	cond_put(cond);

	return EOK;
}
