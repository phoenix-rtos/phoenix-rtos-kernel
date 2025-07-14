#include "hal/hal.h"
#include "lib/lib.h"
#include "include/errno.h"
#include "process.h"
#include "threads.h"
#include "futex.h"

static inline futex_sleepqueue_t *futex_getSleepQueue(process_t *process, futex_t *f)
{
	u32 key = f->address >> 3;
	key ^= key >> FUTEX_SLEEPQUEUES_BITS;
	return &process->futex_sleepqueues[key & FUTEX_SLEEPQUEUES_MASK];
}

void dump(unsigned int c, futex_t *f)
{
	int i = 0;
	futex_t *begin_f = f;
	while (1) {
		lib_printf("[%u] %d: f=%p, p=%p n=%p a=%p\n", c, i, f, f->prev, f->next, f->address);

		if (f->next == begin_f) {
			/* we've wrapped around, so we've reached the end. break out */
			break;
		}

		f = f->next;
		i++;
	}
}

void dump_threads(thread_t *queue)
{
#if 1
	lib_printf("dump_threads\n");
	thread_t *begin = queue;
	thread_t *head = queue;
	int i = 0;
	while (1) {
		lib_printf("[%d] h=%p, p=%p, n=%p\n", i, head, head->prev, head->next);

		if (begin == head->next) {
			break;
		}
		head = head->next;
		i++;
	}
#endif
}

unsigned int c = 222;

int futex_wait(u32 *address, u32 value, time_t timeout)
{
	thread_t *thread = proc_current();
	process_t *process = thread->process;
	futex_t futex;
	futex_sleepqueue_t *sleepqueue = NULL;
	int err;

	hal_memset(&futex, 0, sizeof(futex));

	futex.address = (addr_t)address;

	proc_lockSet(&process->lock);
	sleepqueue = futex_getSleepQueue(process, &futex);
	proc_lockClear(&process->lock);

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

	dump(c, sleepqueue->futex_list);
	c += 1;

	err = proc_threadSleep2(thread, timeout * 1000 * 1000);
	if (err < 0) {
		proc_lockSet(&process->lock);
		LIST_REMOVE(&sleepqueue->futex_list, &futex);
		proc_lockClear(&process->lock);
	}
	return err;
}

int futex_wakeup(u32 *address, u32 n_threads)
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

	proc_lockSet(&process->lock);
	sleepqueue = futex_getSleepQueue(process, &futex);
	proc_lockClear(&process->lock);

	f = sleepqueue->futex_list;
	i = 0;

	/* We're working on a temp n-element copy, so that we can pass it into proc_threadWakeup() */
	thread_t *wakeup_list = NULL;

	/* save begin ptr */
	futex_t *begin_f = f;
	for (i = 0; i < n_threads; i++) {
		LIST_ADD(&wakeup_list, f->waiting_thread);

		if (f->next == begin_f) {
			/* we've wrapped around, so we've reached the end. break out */
			break;
		}

		f = f->next;
	}

	dump_threads(wakeup_list);

	/* err = 0; */
	err = proc_threadWakeup(&wakeup_list);
	if (err < 0) {
		return err;
	}
	return i;
}
