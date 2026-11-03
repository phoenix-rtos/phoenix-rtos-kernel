/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Scheduling contexts (foundation for bandwidth inheritance)
 *
 * Copyright 2026 Phoenix Systems
 * Author: Adam Greloch
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_PROC_SC_H_
#define _PH_PROC_SC_H_

#include "hal/hal.h"


/*
 * A scheduling context (SC) is what the scheduler runs: a priority and its CPU
 * accounting. A thread owns one and runs it.
 */


struct _thread_t;


typedef struct _sched_context_t {
	struct _sched_context_t *next;
	struct _sched_context_t *prev;

	struct _thread_t *t;     /* thread holding this SC now */
	struct _thread_t *owner; /* thread it was allocated for, never changes */
	unsigned int priorityBase : 4;
	unsigned int priority : 4;

	time_t readyTime;
	time_t maxWait;

	time_t startTime;
	time_t cpuTime;
	time_t lastTime;
} sched_context_t;

#endif
