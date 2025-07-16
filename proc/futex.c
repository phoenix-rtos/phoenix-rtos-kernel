#include "hal/hal.h"
#include "lib/lib.h"
#include "include/errno.h"
#include "process.h"
#include "threads.h"
#include "futex.h"

static futex_sleepqueue_t *futex_getSleepQueue(process_t *process, addr_t address)
{
	u32 key;
	futex_sleepqueue_t *futex_sleepqueue;

	proc_lockSet(&process->lock);
	key = address >> 3;
	key ^= key >> FUTEX_SLEEPQUEUES_BITS;
	futex_sleepqueue = &process->futex_sleepqueues[key & FUTEX_SLEEPQUEUES_MASK];
	proc_lockClear(&process->lock);
	return futex_sleepqueue;
}

int proc_futexWait(u32 *address, u32 value, time_t timeout)
{
	thread_t *current_thread = proc_current();
	process_t *current_process = current_thread->process;
	addr_t key = (addr_t)address;
	spinlock_ctx_t ctx;
	futex_sleepqueue_t *sleepqueue;
	int err;
	time_t now;

	if (*address != value) {
		return -EAGAIN;
	}
	sleepqueue = futex_getSleepQueue(current_process, key);

	proc_gettime(&now, NULL);

	hal_spinlockSet(&sleepqueue->spinlock, &ctx);
	sleepqueue->count += 1;
	err = proc_threadWaitInterruptible(&sleepqueue->threads, &sleepqueue->spinlock, now + timeout, &ctx);
	sleepqueue->count -= 1;
	hal_spinlockClear(&sleepqueue->spinlock, &ctx);
	return err;
}

int proc_futexWakeup(u32 *address, u32 n_threads)
{
	thread_t *current_thread = proc_current();
	process_t *current_process = current_thread->process;
	addr_t key = (addr_t)address;
	int err;
	spinlock_ctx_t ctx;
	futex_sleepqueue_t *sleepqueue;

	if (n_threads == 0) {
		return 0;
	}

	sleepqueue = futex_getSleepQueue(current_process, key);
	hal_spinlockSet(&sleepqueue->spinlock, &ctx);
	if (n_threads == FUTEX_WAKEUP_ALL) {
		n_threads = sleepqueue->count;
	}

	for (int i = 0; i < n_threads && sleepqueue->count > 0; i++, sleepqueue->count -= 1) {
		err = proc_threadWakeup(&sleepqueue->threads);
		if (err < 0) {
			hal_spinlockClear(&sleepqueue->spinlock, &ctx);
			return err;
		}
	}
	hal_spinlockClear(&sleepqueue->spinlock, &ctx);
	return n_threads;
}
