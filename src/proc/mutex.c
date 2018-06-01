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

#include HAL
#include "../../include/errno.h"
#include "mutex.h"
#include "threads.h"
#include "resource.h"


int proc_mutexCreate(unsigned int *h)
{
	process_t *process;
	resource_t *r;

	process = proc_current()->process;

	if ((r = resource_alloc(process, h)) == NULL)
		return -ENOMEM;

	proc_lockInit(&r->lock);
	r->type = rtLock;
	resource_put(process, r);

	return EOK;
}


int proc_mutexLock(unsigned int h)
{
	process_t *process;
	resource_t *r;
	int err;

	process = proc_current()->process;

	if ((r = resource_get(process, h)) == NULL)
		return -EINVAL;

	proc_threadUnprotect();
	err = proc_lockSet(&r->lock);
	proc_threadProtect();
	resource_put(process, r);

	return err;
}


int proc_mutexTry(unsigned int h)
{
	process_t *process;
	resource_t *r;
	int err;

	process = proc_current()->process;

	if ((r = resource_get(process, h)) == NULL)
		return -EINVAL;

	proc_threadUnprotect();
	err = proc_lockTry(&r->lock);
	proc_threadProtect();
	resource_put(process, r);

	return err;
}


int proc_mutexUnlock(unsigned int h)
{
	process_t *process;
	resource_t *r;

	process = proc_current()->process;

	if ((r = resource_get(process, h)) == NULL)
		return -EINVAL;

	proc_lockClear(&r->lock);
	resource_put(process, r);

	return EOK;
}
