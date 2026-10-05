/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX per-process timers
 *
 * Copyright 2026 Phoenix Systems
 * Author: Adam Greloch
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "lib/assert.h"
#include "include/errno.h"
#include "include/signal.h"
#include "include/time.h"
#include "threads.h"
#include "timer.h"
#include "process.h"
#include "resource.h"


ptimer_t *timer_get(int id)
{
	thread_t *t = proc_current();
	resource_t *r = resource_get(t->process, id);

	if ((r != NULL) && (r->type != rtTimer)) {
		(void)resource_put(t->process, r);
		return NULL;
	}

	return (r != NULL) ? r->payload.timer : NULL;
}


void timer_put(ptimer_t *timer)
{
	thread_t *t = proc_current();
	int rem;

	LIB_ASSERT(timer != NULL, "process: %s, pid: %d, tid: %d, timer == NULL",
			t->process->path, process_getPid(t->process), proc_getTid(t));

	rem = resource_put(t->process, &timer->resource);
	LIB_ASSERT(rem >= 0, "process: %s, pid: %d, tid: %d, refcnt below zero",
			t->process->path, process_getPid(t->process), proc_getTid(t));
	if (rem == 0) {
		/* Takes the spinlock, locking out a sweep that already holds this timer off its tree */
		proc_ktimerDisarm(&timer->ktimer);
		vm_kfree(timer);
	}
}


/* POSIX fixes a notification's overrun count at the moment the process takes it */
static void timer_latch(ptimer_t *timer)
{
	if (timer->notified != 0) {
		timer->overrun = timer->overrunPending;
		timer->overrunPending = 0;
		timer->notified = 0;
	}
}


static void timer_overrunAdd(ptimer_t *timer, int n)
{
	if ((__INT_MAX__ - timer->overrunPending) < n) {
		timer->overrunPending = __INT_MAX__;
	}
	else {
		timer->overrunPending += n;
	}
}


/* Note: called from the timer interrupt with the scheduler spinlock set */
static time_t timer_fire(ktimer_t *ktimer, time_t now)
{
	ptimer_t *timer = lib_treeof(ptimer_t, ktimer, ktimer);
	process_t *process = ktimer->targetProcess;
	time_t next = 0, missed = 0;
	int expiries;
	u32 sigbit;

	if ((timer->interval != 0) && (now >= ktimer->expiry)) {
		/*
		 * Skip the periods that have already gone by rather than fire back to back to catch
		 * up with them. They still happened, and are reported as overrun.
		 */
		missed = (now - ktimer->expiry) / timer->interval;

		if ((missed + 1) > ((TIME_T_MAX - ktimer->expiry) / timer->interval)) {
			next = TIME_T_MAX;
		}
		else {
			next = ktimer->expiry + ((missed + 1) * timer->interval);
		}
	}

	expiries = (missed > (time_t)(__INT_MAX__ - 1)) ? __INT_MAX__ : ((int)missed + 1);

	if (timer->notify == PH_TIMER_NOTIFY_SIGNAL) {
		sigbit = (u32)1U << (unsigned int)timer->signo;

		if ((process->sigpend & sigbit) != 0U) {
			/* Signals do not queue, so these fold into the notification still outstanding */
			timer_overrunAdd(timer, expiries);
		}
		else {
			timer_latch(timer);

			/* This expiry is the notification itself, so only the skipped ones are overrun */
			timer_overrunAdd(timer, expiries - 1);
			timer->notified = 1;

			(void)threads_sigpostLocked(process, NULL, timer->signo);
		}
	}
	else {
		/* Nothing is delivered, so there is no notification to hold the count back for */
		timer->overrun = expiries - 1;
		timer->overrunPending = 0;
	}

	return next;
}


static int timer_target(int clock, int pid, int tid, process_t **rprocess, thread_t **rthread)
{
	thread_t *current = proc_current();
	process_t *process = current->process;
	thread_t *t;
	int err = EOK;

	*rprocess = process;
	*rthread = NULL;

	if (clock == PH_CLOCK_THREAD_CPUTIME) {
		if ((tid == 0) || (tid == proc_getTid(current))) {
			*rthread = current;
		}
		else {
			t = threads_findThread(tid);
			if (t == NULL) {
				err = -ESRCH;
			}
			else if (t->process != process) {
				/* Only the owning process' clocks, so that no cross-process reference is needed */
				threads_put(t);
				err = -EPERM;
			}
			else {
				/*
				 * No reference is kept: a process destroys its timers before freeing the
				 * threads they point at, so the pointer outlives the timer either way.
				 */
				threads_put(t);
				*rthread = t;
			}
		}
	}
	else if (clock == PH_CLOCK_PROCESS_CPUTIME) {
		if ((pid != 0) && (pid != process_getPid(process))) {
			err = -EPERM;
		}
	}
	else {
		/* Timeline clocks measure nothing in particular */
	}

	return err;
}


int proc_timerCreate(int clock, int pid, int tid, int notify, int signo)
{
	process_t *p = proc_current()->process;
	process_t *target;
	thread_t *targetThread;
	ptimer_t *timer;
	int id, err;

	switch (clock) {
		case PH_CLOCK_RELATIVE:
		case PH_CLOCK_MONOTONIC:
		case PH_CLOCK_REALTIME:
		case PH_CLOCK_PROCESS_CPUTIME:
		case PH_CLOCK_THREAD_CPUTIME:
			break;

		default:
			return -EINVAL;
	}

	if (notify == PH_TIMER_NOTIFY_SIGNAL) {
		/* NSIG and not NSIG_TOTAL, as SIGCANCEL is Phoenix's own and not for timers to raise */
		if ((signo <= 0) || (signo >= NSIG)) {
			return -EINVAL;
		}
	}
	else if (notify != PH_TIMER_NOTIFY_NONE) {
		return -EINVAL;
	}
	else {
		signo = 0;
	}

	err = timer_target(clock, pid, tid, &target, &targetThread);
	if (err < 0) {
		return err;
	}

	timer = vm_kmalloc(sizeof(*timer));
	if (timer == NULL) {
		return -ENOMEM;
	}

	hal_memset(timer, 0, sizeof(*timer));

	timer->resource.payload.timer = timer;
	timer->resource.type = rtTimer;

	timer->notify = notify;
	timer->signo = signo;

	timer->ktimer.fire = timer_fire;
	timer->ktimer.clock = (unsigned int)((clock == PH_CLOCK_RELATIVE) ? PH_CLOCK_MONOTONIC : clock);
	timer->ktimer.targetProcess = target;
	timer->ktimer.targetThread = targetThread;

	id = resource_alloc(p, &timer->resource);
	if (id < 0) {
		vm_kfree(timer);
		return -ENOMEM;
	}

	(void)resource_put(p, &timer->resource);

	return id;
}


int proc_timerDelete(int id)
{
	ptimer_t *timer = timer_get(id);

	if (timer == NULL) {
		return -EINVAL;
	}

	/* Drops what timer_get() took above; destroying the resource drops the process' own */
	timer_put(timer);

	return proc_resourceDestroy(proc_current()->process, id);
}


static void timer_report(const ptimer_t *timer, timerspec_t *value)
{
	value->interval = timer->interval;
	(void)proc_ktimerRemaining(&timer->ktimer, &value->value);
}


int proc_timerSettime(int id, unsigned int flags, const timerspec_t *value, timerspec_t *old)
{
	ptimer_t *timer = timer_get(id);
	time_t expiry, now;

	if (timer == NULL) {
		return -EINVAL;
	}

	if ((value->value < 0) || (value->interval < 0)) {
		timer_put(timer);
		return -EINVAL;
	}

	if (old != NULL) {
		timer_report(timer, old);
	}

	/* Kept even while disarmed, so that timer_gettime() still reports what was asked for */
	timer->interval = value->interval;

	if (value->value == 0) {
		/* POSIX: a zero it_value disarms, whatever the interval says */
		proc_ktimerDisarm(&timer->ktimer);
	}
	else {
		if ((flags & TIMER_ABSTIME) != 0U) {
			expiry = value->value;
		}
		else {
			now = proc_ktimerNow(&timer->ktimer);
			expiry = ((TIME_T_MAX - now) < value->value) ? TIME_T_MAX : (now + value->value);
		}

		proc_ktimerArm(&timer->ktimer, expiry);
	}

	timer_put(timer);

	return EOK;
}


int proc_timerGettime(int id, timerspec_t *value)
{
	ptimer_t *timer = timer_get(id);

	if (timer == NULL) {
		return -EINVAL;
	}

	timer_report(timer, value);

	timer_put(timer);

	return EOK;
}


int proc_timerGetoverrun(int id)
{
	ptimer_t *timer = timer_get(id);
	int overrun;

	if (timer == NULL) {
		return -EINVAL;
	}

	/*
	 * The signal no longer being pending means the process has taken the notification, which is
	 * the moment POSIX fixes its overrun at. Nothing else in the kernel observes that moment.
	 */
	if ((timer->notify == PH_TIMER_NOTIFY_SIGNAL) && (timer->notified != 0) &&
			(threads_sigpending(proc_current()->process, timer->signo) == 0)) {
		timer_latch(timer);
	}

	overrun = timer->overrun;

	timer_put(timer);

	return overrun;
}
