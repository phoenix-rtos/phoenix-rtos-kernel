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
#include "resource.h"


int proc_condCreate(unsigned int *h)
{
	process_t *process;
	resource_t *r;

	process = proc_current()->process;

	if ((r = resource_alloc(process, h, rtCond)) == NULL)
		return -ENOMEM;

	r->waitq = NULL;
	resource_put(process, r);

	return EOK;
}


int proc_condWait(unsigned int c, unsigned int m, time_t timeout)
{
	lock_t *lock;
	int err = EOK;
	process_t *process;
	resource_t *rl, *rc;

	process = proc_current()->process;

	if ((rl = resource_get(process, m)) == NULL)
		return -EINVAL;

	if (rl->type != rtLock) {
		resource_put(process, rl);
		return -EINVAL;
	}

	if ((rc = resource_get(process, c)) == NULL) {
		resource_put(process, rl);
		return -EINVAL;
	}

	if (rc->type != rtCond) {
		resource_put(process, rl);
		resource_put(process, rc);
		return -EINVAL;
	}

	lock = rl->lock;
	proc_threadUnprotect();

	err = proc_lockWait(&rc->waitq, lock, timeout);

	proc_threadProtect();

	resource_put(process, rl);
	resource_put(process, rc);

	return err;
}


int proc_condSignal(process_t *process, unsigned int c)
{
	int err = EOK;
	resource_t *rc;

	if ((rc =  resource_get(process, c)) == NULL)
		return -EINVAL;

	if (rc->type != rtCond) {
		resource_put(process, rc);
		return -EINVAL;
	}

	proc_threadWakeupYield(&rc->waitq);

	resource_put(process, rc);

	return err;
}


int proc_condBroadcast(process_t *process, unsigned int c)
{
	int err = EOK;
	resource_t *rc;

	if ((rc =  resource_get(process, c)) == NULL)
		return -EINVAL;

	if (rc->type != rtCond) {
		resource_put(process, rc);
		return -EINVAL;
	}

	proc_threadBroadcastYield(&rc->waitq);

	resource_put(process, rc);

	return err;
}


int proc_condCopy(resource_t *dst, resource_t *src)
{
	dst->waitq = NULL;
	dst->type = rtCond;

	return EOK;
}
