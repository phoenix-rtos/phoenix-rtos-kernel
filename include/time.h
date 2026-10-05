/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Time
 *
 * Copyright 2024, 2026 Phoenix Systems
 * Author: Lukasz Leczkowski, Adam Greloch
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_TIME_H_
#define _PH_TIME_H_

#include "types.h"


/* PH_CLOCK_RELATIVE is no timeline of its own and resolves against PH_CLOCK_MONOTONIC */
#define PH_CLOCK_RELATIVE  0
#define PH_CLOCK_REALTIME  1
#define PH_CLOCK_MONOTONIC 2

/*
 * These advance only while the thread runs, so they have no global timeline - see proc/timer.c
 * for how timers on them are tracked.
 */
#define PH_CLOCK_PROCESS_CPUTIME 3
#define PH_CLOCK_THREAD_CPUTIME  4

#define PH_CLK_TCK 100

#define TIMER_ABSTIME 1U

/* Mirror the POSIX sigevent notification types */
#define PH_TIMER_NOTIFY_NONE   0
#define PH_TIMER_NOTIFY_SIGNAL 1


/* Both fields are in microseconds. A zero value disarms the timer. */
typedef struct {
	time_t value;    /* an absolute expiry when TIMER_ABSTIME is given */
	time_t interval; /* reload period, 0 for a one-shot timer */
} timerspec_t;


#endif
