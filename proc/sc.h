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


#define BWI_MAX_CHAIN_DEPTH 16


struct _thread_t;


typedef struct _sched_context_t {
	struct _sched_context_t *next;
	struct _sched_context_t *prev;

	struct _sched_context_t *dnext;
	struct _sched_context_t *dprev;

	struct _thread_t *t;
	struct _thread_t *owner;
	unsigned int priorityBase : 4;
	unsigned int priority : 4;

	time_t readyTime;
	time_t maxWait;

	time_t startTime;
	time_t cpuTime;
	time_t lastTime;
} sched_context_t;


sched_context_t *_sc_best(struct _thread_t *t);


struct _thread_t *_sc_blockedOn(struct _thread_t *t);


int _sc_reaches(struct _thread_t *t, const struct _thread_t *target);


void _sc_blockOnRecv(struct _thread_t *t, struct _thread_t *recv);


void _sc_recalculate(struct _thread_t *t);


void _sc_donate(struct _thread_t *from, struct _thread_t *to, sched_context_t *sc);


void _sc_return(struct _thread_t *from, struct _thread_t *to);


void _sc_reclaim(struct _thread_t *t);


void _sc_migrate(struct _thread_t *from, struct _thread_t *queue, struct _thread_t *to);

#endif
