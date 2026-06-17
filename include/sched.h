/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Scheduler definitions
 *
 * Copyright 2026 Phoenix Systems
 * Authors: Adam Greloch
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_SCHED_H_
#define _PH_SCHED_H_

#include "types.h"

#define SCHED_FIFO  0
#define SCHED_RR    1
#define SCHED_OTHER 2


typedef struct {
	time_t interval;
	int minPriority;
	int maxPriority;
	union {
		char reserved[32]; /* reserved for policy-specific values */
	} policy;
} sched_info_t;


#endif
