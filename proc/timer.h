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

#ifndef _PH_PROC_TIMER_H_
#define _PH_PROC_TIMER_H_

#include "include/time.h"
#include "threads.h"
#include "resource.h"


typedef struct _ptimer_t {
	resource_t resource;
	ktimer_t ktimer;

	time_t interval; /* us, 0 for one-shot */

	int notify; /* PH_TIMER_NOTIFY_* */
	int signo;

	int overrun;        /* of the last notification the process took */
	int overrunPending; /* expiries piled up behind the notification still outstanding */
	int notified;
} ptimer_t;


void timer_put(ptimer_t *timer);


/*
 * pid and tid name what the CPU-time clocks measure, 0 standing for the calling process or
 * thread. Returns the timer's handle.
 */
int proc_timerCreate(int clock, int pid, int tid, int notify, int signo);


int proc_timerDelete(int id);


int proc_timerSettime(int id, unsigned int flags, const timerspec_t *value, timerspec_t *old);


int proc_timerGettime(int id, timerspec_t *value);


int proc_timerGetoverrun(int id);


#endif
