/*
 *
 * Operating system kernel
 *
 * Futex
 *
 * Copyright 2025 Phoenix Systems
 * Author: Kamil Kowalczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <stdatomic.h>
#include <stdbool.h>
#include "hal/hal.h"
#include "lib/lib.h"
#include "include/errno.h"
#include "include/time.h"
#include "process.h"
#include "threads.h"
#include "futex.h"


static u32 _proc_futexTableHash(addr_t address)
{
	u32 key;

	// hash the address
	key = address >> 3;
	key ^= key >> FUTEX_SLEEPQUEUES_BITS;
	return key & FUTEX_SLEEPQUEUES_MASK;
}


static futex_sleepqueue_t *_proc_allocFutexSleepQueue(process_t *process, addr_t address)
{
	u32 idx, i;

	idx = _proc_futexTableHash(address);

	/* Find a free slot using linear probing */
	i = idx;
	do {
		if (process->futexSleepQueues[i].address == 0) {
			process->futexSleepQueues[i].address = address;
			return &process->futexSleepQueues[i];
		}
		i = (i + 1) % FUTEX_SLEEPQUEUES_SIZE;
	} while (i != idx);

	return NULL;
}


futex_sleepqueue_t *_proc_getFutexSleepQueue(process_t *process, addr_t address)
{
	u32 idx, i;

	idx = _proc_futexTableHash(address);

	/* Find a taken slot with the same address using linear probing */
	i = idx;
	do {
		if (process->futexSleepQueues[i].address == address) {
			return &process->futexSleepQueues[i];
		}
		else if (process->futexSleepQueues[i].address == 0) {
			return NULL;
		}
		i = (i + 1) % FUTEX_SLEEPQUEUES_SIZE;
	} while (i != idx);

	return NULL;
}


static bool proc_futexUnwait(futex_sleepqueue_t *sq, futex_waitctx_t *wc)
{
	bool r;
	spinlock_ctx_t sc;

	hal_spinlockSet(&sq->spinlock, &sc);
	r = atomic_load(&wc->thread) != NULL;
	if (r) {
		LIST_REMOVE(&sq->waitctxs, wc);
	}
	hal_spinlockClear(&sq->spinlock, &sc);
	return r;
}


int proc_futexWait(_Atomic(u32) *address, u32 value, time_t timeout, int clockType)
{
	spinlock_ctx_t sc, sqSc;
	futex_sleepqueue_t *sq;
	futex_waitctx_t wc;
	thread_t *current;
	int err = EOK;
	time_t waitTime = 0, offs;


	if (timeout != 0) {
		switch (clockType) {
			case PH_CLOCK_REALTIME:
				proc_gettime(&waitTime, &offs);
				if (waitTime + offs > timeout) {
					return -ETIME;
				}
				waitTime = timeout - offs;
				break;
			case PH_CLOCK_MONOTONIC:
				proc_gettime(&waitTime, NULL);
				if (waitTime > timeout) {
					return -ETIME;
				}
				waitTime = timeout;
				break;
			case PH_CLOCK_RELATIVE:
				proc_gettime(&waitTime, NULL);
				waitTime += timeout;
				break;
			default:
				return -EINVAL;
		}
	}

	current = proc_current();

	hal_spinlockSet(&current->process->futexSqSpinlock, &sqSc);
	sq = _proc_getFutexSleepQueue(current->process, (addr_t)address);
	if (sq == NULL) {
		sq = _proc_allocFutexSleepQueue(current->process, (addr_t)address);
	}
	hal_spinlockClear(&current->process->futexSqSpinlock, &sqSc);
	if (sq == NULL) {
		return -ENOMEM;
	}
	atomic_init(&wc.thread, NULL);

	atomic_store(&wc.thread, current);
	hal_spinlockSet(&sq->spinlock, &sc);
	LIST_ADD(&sq->waitctxs, &wc);
	hal_spinlockClear(&sq->spinlock, &sc);

	if (atomic_load(address) != value) {
		err = -EAGAIN;
		proc_futexUnwait(sq, &wc);
	}
	else {

		if (atomic_load(&wc.thread) != NULL) {
			hal_spinlockSet(&sq->spinlock, &sc);
			err = proc_threadWaitInterruptible(&sq->threads, &sq->spinlock, waitTime, &sc);
			hal_spinlockClear(&sq->spinlock, &sc);
		}

		if (err != EOK || atomic_load(&wc.thread) != NULL) {
			if (proc_futexUnwait(sq, &wc) == 0) {
				err = EOK;
			}
		}
	}
	return err;
}


int proc_futexWakeup(process_t *process, _Atomic(u32) *address, u32 wakeCount)
{
	futex_sleepqueue_t *sq;
	futex_waitctx_t *wc = NULL, *wakeupList = NULL, *tmpwc;
	int i = 0, woken = 0;
	spinlock_ctx_t sc, sqSc;
	thread_t *tmp = NULL;

	if (wakeCount == 0) {
		return 0;
	}

	hal_spinlockSet(&process->futexSqSpinlock, &sqSc);
	sq = _proc_getFutexSleepQueue(process, (addr_t)address);
	hal_spinlockClear(&process->futexSqSpinlock, &sqSc);
	if (sq == NULL) {
		return 0;
	}

	hal_spinlockSet(&sq->spinlock, &sc);
	while ((wc = sq->waitctxs) != NULL) {
		LIST_REMOVE(&sq->waitctxs, wc);
		LIST_ADD(&wakeupList, wc);
		i++;
		if (i == wakeCount && wakeCount != FUTEX_WAKEUP_ALL) {
			break;
		}
	}
	hal_spinlockClear(&sq->spinlock, &sc);

	if (wakeupList != NULL) {
		wc = wakeupList;
		do {
			LIB_ASSERT(wc != NULL, "wc == NULL");
			tmpwc = wc;
			tmp = atomic_load(&tmpwc->thread);
			wc = wc->next;
			atomic_store(&tmpwc->thread, NULL);
			hal_spinlockSet(&sq->spinlock, &sc);
			if (proc_threadWakeupOne(tmp)) {
				woken++;
			}
			hal_spinlockClear(&sq->spinlock, &sc);
		} while (wc != NULL && wc != wakeupList);
	}

	return woken;
}
