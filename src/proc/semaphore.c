/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Semaphores
 *
 * Copyright 2012, 2017, 2018 Phoenix Systems
 * Copyright 2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include HAL
#include "../../include/errno.h"
#include "threads.h"
#include "semaphore.h"
#include "resource.h"


int proc_semaphoreP(unsigned int sh, time_t timeout)
{
	process_t *process;
	resource_t *r;
	int err;

	process = proc_current()->process;

	if ((r = resource_get(process, sh)) == NULL)
		return -EINVAL;

	proc_threadUnprotect();
	hal_spinlockSet(&r->semaphore.spinlock);
	for (;;) {

		if (r->semaphore.v > 0) {
			r->semaphore.v--;
			break;
		}

		if ((err = proc_threadWait(&r->semaphore.queue, &r->semaphore.spinlock, timeout)) != EOK)
			break;
	}
	hal_spinlockClear(&r->semaphore.spinlock);
	proc_threadProtect();
	resource_put(process, r);

	return err;
}


int proc_semaphoreV(unsigned int sh)
{
	process_t *process;
	resource_t *r;

	process = proc_current()->process;

	if ((r = resource_get(process, sh)) == NULL)
		return -EINVAL;

	hal_spinlockSet(&r->semaphore.spinlock);
	r->semaphore.v++;
	proc_threadWakeup(&r->semaphore.queue);
	hal_spinlockClear(&r->semaphore.spinlock);

	resource_put(process, r);

	return EOK;
}


int proc_semaphoreCreate(unsigned int *sh, unsigned int v)
{
	process_t *process;
	resource_t *r;;

	process = proc_current()->process;

	if ((r = resource_alloc(process, sh)) == NULL)
		return -ENOMEM;

	r->semaphore.v = v;
	r->semaphore.queue = NULL;
	hal_spinlockCreate(&r->semaphore.spinlock, "semaphore.spinlock");

	r->type = rtSemaphore;
	resource_put(process, r);

	return EOK;
}


int proc_semaphoreDone(semaphore_t *semaphore)
{
	hal_spinlockDestroy(&semaphore->spinlock);
	return EOK;
}
