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
	futex_t *f = NULL;
	thread_t *t = proc_current();
	resource_t *r = resource_get(t->process, h);

	if (r != NULL && r->type == rtFutex) {
		f = r->payload.futex;
	}

	return f;
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
	int ret = EOK;
	process_t *p = proc_current()->process;
	futex_t *futex;

	if ((h == NULL) || (uaddr == NULL) || (((u64)uaddr % _Alignof(u32)) != 0U)) {
		ret = -EINVAL;
	}

	if ((ret == EOK) && (vm_mapBelongs(p, h, sizeof(*h)) < 0)) {
		ret = -EFAULT;
	}

	if ((ret == EOK) && (vm_mapBelongs(p, uaddr, sizeof(*uaddr)) < 0)) {
		ret = -EFAULT;
	}

	if (ret == EOK) {
		futex = vm_kmalloc(sizeof(*futex));
		if (futex == NULL) {
			ret = -ENOMEM;
		}
	}

	if (ret == EOK) {
		futex->resource.payload.futex = futex;
		futex->resource.type = rtFutex;
		futex->uaddr = uaddr;

		*h = resource_alloc(p, &futex->resource);
		if (*h < 0) {
			vm_kfree(futex);
			ret = -ENOMEM;
		}
	}

	if (ret == EOK) {
		hal_spinlockCreate(&futex->spinlock, "futex");
		futex->queue = NULL;

		(void)resource_put(p, &futex->resource);
	}

	return EOK;
}


int proc_futexWake(handle_t h, int n)
{
	futex_t *futex;
	spinlock_ctx_t sc;
	int ret = EOK;

	futex = futex_get(h);
	if (futex == NULL) {
		ret = -EINVAL;
	}

	if (ret != EOK) {
		hal_spinlockSet(&futex->spinlock, &sc);
		ret = proc_threadRequeue(&futex->queue, NULL, n);
		hal_spinlockClear(&futex->spinlock, &sc);
		futex_put(futex);
	}

	if (ret > 0) {
		hal_cpuReschedule(NULL, NULL);
	}

	return ret;
}


int proc_futexWait(int h, u32 val, time_t timeout)
{
	futex_t *futex;
	spinlock_ctx_t sc;
	int ret = EOK;

	futex = futex_get(h);
	if (futex == NULL) {
		ret = -EINVAL;
	}

	if (ret == EOK) {
		hal_spinlockSet(&futex->spinlock, &sc);

		if (val == *futex->uaddr) {
			ret = proc_threadWaitInterruptible(&futex->queue, &futex->spinlock, timeout, &sc);
		}
		else {
			ret = -EAGAIN;
		}

		hal_spinlockClear(&futex->spinlock, &sc);
		futex_put(futex);
	}

	return ret;
}


int proc_futexRequeue(handle_t h1, handle_t h2, u32 val, int n)
{
	futex_t *f1, *f2;
	spinlock_ctx_t sc1, sc2;
	int ret = EOK;

	f1 = futex_get(h1);
	if (f1 == NULL) {
		ret = -EINVAL;
	}

	if (ret == EOK) {
		f2 = futex_get(h2);
		if (f2 == NULL) {
			futex_put(f1);
			ret = -EINVAL;
		}
	}

	if (ret == EOK) {
		hal_spinlockSet(&futex_common.rqlock, &sc1);
		hal_spinlockSet(&f1->spinlock, &sc2);
		hal_spinlockSet(&f2->spinlock, &sc2);

		if (*f1->uaddr == val) {
			ret = proc_threadRequeue(&f1->queue, &f2->queue, n);
		}
		else {
			ret = -EAGAIN;
		}

		hal_spinlockClear(&f2->spinlock, &sc2);
		hal_spinlockClear(&f1->spinlock, &sc2);
		hal_spinlockClear(&futex_common.rqlock, &sc1);

		futex_put(f1);
		futex_put(f2);
	}

	if (ret > 0) {
		hal_cpuReschedule(NULL, NULL);
	}

	return ret;
}


void _futex_init(void)
{
	hal_spinlockCreate(&futex_common.rqlock, "futex.rqlock");
}
