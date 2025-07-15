#include "hal/hal.h"
#include "lib/lib.h"
#include "include/errno.h"
#include "process.h"
#include "threads.h"
#include "futex.h"

static inline thread_t **futex_getSleepQueue(process_t *process, addr_t address)
{
	u32 key;
	thread_t **list;
	spinlock_ctx_t ctx;

	hal_spinlockSet(&process->futex_sleepqueues.spinlock, &ctx);
	key = address >> 3;
	key ^= key >> FUTEX_SLEEPQUEUES_BITS;
	list = &process->futex_sleepqueues.items[key & FUTEX_SLEEPQUEUES_MASK];
	hal_spinlockClear(&process->futex_sleepqueues.spinlock, &ctx);
	return list;
}

int proc_futexWait(u32 *address, u32 value, time_t timeout)
{
	thread_t *current_thread = proc_current();
	process_t *current_process = current_thread->process;
	addr_t key = (addr_t)address;
	spinlock_ctx_t ctx;
	thread_t **sleep_queue;
	int err;
	time_t now;

	if (*address != value) {
		return -EAGAIN;
	}
	sleep_queue = futex_getSleepQueue(current_process, key);

	proc_gettime(&now, NULL);

	hal_spinlockSet(&current_process->futex_sleepqueues.spinlock, &ctx);
	err = proc_threadWaitInterruptible(sleep_queue, &current_process->futex_sleepqueues.spinlock, now + timeout, &ctx);
	hal_spinlockClear(&current_process->futex_sleepqueues.spinlock, &ctx);
	return err;
}

int proc_futexWakeup(u32 *address, u32 n_threads)
{
	thread_t *current_thread = proc_current();
	process_t *current_process = current_thread->process;
	addr_t key = (addr_t)address;
	int err;
	thread_t **sleep_queue;

	if (n_threads == 0) {
		return 0;
	}

	sleep_queue = futex_getSleepQueue(current_process, key);

	for (int i = 0; i < n_threads; i++) {
		err = proc_threadWakeup(sleep_queue);
		if (err < 0) {
			return err;
		}
	}
	return n_threads;
}
