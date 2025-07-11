#include "hal/hal.h"
#include "lib/lib.h"
#include "include/errno.h"
#include "process.h"
#include "threads.h"
#include "futex.h"

static inline futex_sleepqueue_t *futex_getSleepQueue(process_t *process, futex_t *f)
{
	unsigned int key = f->address >> 3;
	key ^= key >> FUTEX_SLEEPQUEUES_BITS;
	return &process->futex_sleepqueues[key & FUTEX_SLEEPQUEUES_MASK];
}

int futex_wait(unsigned int *address, unsigned int value, time_t timeout)
{
	thread_t *thread = proc_current();
	process_t *process = thread->process;
	futex_t futex;
	futex_sleepqueue_t *sleepqueue = NULL;
	int err;

	hal_memset(&futex, 0, sizeof(futex));

	futex.address = (addr_t)address;

	sleepqueue = futex_getSleepQueue(process, &futex);

	futex.waiting_thread = thread;
	proc_lockSet(&process->lock);
	LIST_ADD(&sleepqueue->futex_list, &futex);
	proc_lockClear(&process->lock);

	/* lib_printf("address = %p, *address = %u, value = %u, timeout = %d\n", address, *address, value, timeout); */
	if (*address != value) {
		proc_lockSet(&process->lock);
		LIST_REMOVE(&sleepqueue->futex_list, &futex);
		proc_lockClear(&process->lock);
		return -EAGAIN;
	}

	err = proc_threadSleep(timeout * 1000 * 1000);
	if (err < 0) {
		proc_lockSet(&process->lock);
		LIST_REMOVE(&sleepqueue->futex_list, &futex);
		proc_lockClear(&process->lock);
	}
	return err;
}

int futex_wakeup(unsigned int *address, unsigned int n_threads)
{
	/* lib_printf("address = %p, *address = %u, n_threads = %u\n", address, *address, n_threads); */
	thread_t *thread = proc_current();
	process_t *process = thread->process;
	futex_t futex;
	futex_sleepqueue_t *sleepqueue = NULL;
	futex_t *f;
	int i, err;

	if (n_threads == 0) {
		return 0;
	}

	hal_memset(&futex, 0, sizeof(futex));
	futex.address = (addr_t)address;

	sleepqueue = futex_getSleepQueue(process, &futex);

	f = sleepqueue->futex_list;
	i = 0;
	while (f != NULL) {
		if (i == n_threads) {
			break;
		}
		/* lib_printf("WAKEUP\n"); */
		err = proc_threadWakeup(&f->waiting_thread);
		if (err < 0) {
			return err;
		}
		f = f->next, i++;
	}
	return i;
}
