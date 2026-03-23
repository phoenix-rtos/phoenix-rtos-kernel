/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Futexes
 *
 * Copyright 2026 Phoenix Systems
 * Author: Jakub Smolaga
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <hal/hal.h>
#include "include/errno.h"
#include "threads.h"
#include "futex.h"
#include "resource.h"


static struct {
	/*
	 * Extra spinlock for the requeue operation. This is required because
	 * futexRequeue locks two spinlocks which could deadlock (one thread
	 * holds lock1 and waits for lock2 and another thread holds lock2 and
	 * waits for lock1).
	 */
	spinlock_t rqlock;
} futex_common;


futex_t *futex_get(handle_t h)
{
	thread_t *t = proc_current();
	resource_t *r = resource_get(t->process, h);

	if (r != NULL && r->type == rtFutex) {
		return r->payload.futex;
	}
	else {
		return NULL;
	}
}


void futex_put(futex_t *futex)
{
	thread_t *t = proc_current();
	int rem;

	rem = resource_put(t->process, &futex->resource);
	if (rem == 0) {
		hal_spinlockDestroy(&futex->spinlock);
		vm_kfree(futex);
	}
}


int proc_futexCreate(handle_t *h, u32 *uaddr)
{
	process_t *p = proc_current()->process;
	futex_t *futex;

	if (vm_mapBelongs(p, h, sizeof(h)) < 0) {
		return -EFAULT;
	}

	if (vm_mapBelongs(p, uaddr, sizeof(*uaddr)) < 0) {
		return -EFAULT;
	}

	if (h == NULL || uaddr == NULL) {
		return -EINVAL;
	}

	futex = vm_kmalloc(sizeof(*futex));
	if (futex == NULL) {
		return -ENOMEM;
	}

	futex->resource.payload.futex = futex;
	futex->resource.type = rtFutex;
	futex->uaddr = uaddr;

	*h = resource_alloc(p, &futex->resource);
	if (*h < 0) {
		vm_kfree(futex);
		return -ENOMEM;
	}

	hal_spinlockCreate(&futex->spinlock, "futex");
	futex->queue = NULL;

	(void)resource_put(p, &futex->resource);

	return EOK;
}


int proc_futexWake(handle_t h, int n)
{
	futex_t *futex;
	spinlock_ctx_t sc;
	int ret;

	futex = futex_get(h);
	if (futex == NULL) {
		return -EINVAL;
	}

	hal_spinlockSet(&futex->spinlock, &sc);
	ret = proc_threadRequeueYield(&futex->queue, NULL, n);
	hal_spinlockClear(&futex->spinlock, &sc);

	futex_put(futex);

	return ret;
}


int proc_futexWait(int h, u32 val, time_t timeout)
{
	futex_t *futex;
	int err;
	spinlock_ctx_t sc;

	futex = futex_get(h);
	if (futex == NULL) {
		return -EINVAL;
	}

	hal_spinlockSet(&futex->spinlock, &sc);

	if (val == *futex->uaddr) {
		err = proc_threadWaitInterruptible(&futex->queue, &futex->spinlock, timeout, &sc);
	}
	else {
		err = -EAGAIN;
	}

	hal_spinlockClear(&futex->spinlock, &sc);
	futex_put(futex);

	return err;
}


int proc_futexRequeue(handle_t h1, handle_t h2, u32 val, int n)
{
	futex_t *f1, *f2;
	spinlock_ctx_t sc;
	int ret;

	f1 = futex_get(h1);
	if (f1 == NULL) {
		return -EINVAL;
	}

	f2 = futex_get(h2);
	if (f2 == NULL) {
		futex_put(f1);
		return -EINVAL;
	}

	hal_spinlockSet(&futex_common.rqlock, &sc);
	hal_spinlockSet(&f1->spinlock, NULL);
	hal_spinlockSet(&f2->spinlock, NULL);

	if (*f1->uaddr == val) {
		ret = proc_threadRequeueYield(&f1->queue, &f2->queue, n);
	}
	else {
		ret = -EAGAIN;
	}

	hal_spinlockClear(&f2->spinlock, NULL);
	hal_spinlockClear(&f1->spinlock, NULL);
	hal_spinlockClear(&futex_common.rqlock, &sc);

	futex_put(f1);
	futex_put(f2);

	return ret;
}


void _futex_init(void)
{
	hal_spinlockCreate(&futex_common.rqlock, "futex.rqlock");
}
