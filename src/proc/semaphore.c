/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Semaphores
 *
 * Copyright 2012, 2017 Phoenix Systems
 * Copyright 2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include HAL
#include "../../include/errno.h"
#include "threads.h"
#include "semaphore.h"


static void semaphore_p(semaphore_t *semaphore)
{
	hal_spinlockSet(&semaphore->spinlock);
	for (;;) {

		if (semaphore->v > 0) {
			semaphore->v--;
			break;
		}
		proc_threadWait(&semaphore->queue, &semaphore->spinlock, 0);
	}
	hal_spinlockClear(&semaphore->spinlock);

	return;
}


static void semaphore_v(semaphore_t *semaphore)
{
	hal_spinlockSet(&semaphore->spinlock);	
	semaphore->v++;
	proc_threadWakeup(&semaphore->queue);
	hal_spinlockClear(&semaphore->spinlock);

	return;
}


static int semaphore_init(semaphore_t *semaphore, unsigned int v)
{
	semaphore->v = v;
	semaphore->queue = NULL;
	hal_spinlockCreate(&semaphore->spinlock, "semaphore.spinlock");

	return EOK;
}


static int semaphore_done(semaphore_t *semaphore)
{
	hal_spinlockDestroy(&semaphore->spinlock);
	return EOK;
}


int proc_semaphoreP(unsigned int sh)
{
	semaphore_t *s;

	s = NULL;
	semaphore_p(s);
	return EOK; 
}


int proc_semaphoreV(unsigned int sh)
{
	semaphore_t *s;

	s = NULL;
	semaphore_v(s);
	return EOK;
}


int proc_semaphoreCreate(unsigned int *sh, unsigned int v)
{
	semaphore_t *s;

	s = NULL;

	semaphore_init(s, v);
	return EOK;
}


int proc_semaphoreDestroy(unsigned int *sh)
{
	semaphore_t *s;

	s = NULL;

	semaphore_done(s);
	return EOK;
}
