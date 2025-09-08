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
 * accounting. A thread owns one and donates it to the thread it blocks on, so
 * that thread runs at the blocked thread's priority.
 *
 * This module tracks who holds which SC and who donated it; only
 * _threads_setActiveSc() (threads.h) decides what a thread runs. Everything
 * here requires threads_spinlock.
 */


#define BWI_MAX_CHAIN_DEPTH 16


struct _thread_t;


typedef struct _sched_context_t {
	struct _sched_context_t *next;
	struct _sched_context_t *prev;

	struct _sched_context_t *dnext;
	struct _sched_context_t *dprev;

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


/* The best SC t can run: its own, or one donated to it. */
sched_context_t *_sc_best(struct _thread_t *t);


/* The thread t is blocked on, or NULL if it is blocked on none. */
struct _thread_t *_sc_blockedOn(struct _thread_t *t);


/* 1 if t is transitively blocked on target, so donating to it would cycle. */
int _sc_reaches(struct _thread_t *t, const struct _thread_t *target);


/* t blocks on recv, a receiver busy serving somebody else. */
void _sc_blockOnRecv(struct _thread_t *t, struct _thread_t *recv);


/* Makes t run its best SC. */
void _sc_recalculate(struct _thread_t *t);


/* from blocks on to, which runs sc until it releases from. */
void _sc_donate(struct _thread_t *from, struct _thread_t *to, sched_context_t *sc);


/* from gives back the SC to pushed to it. The caller sets to's state. */
void _sc_return(struct _thread_t *from, struct _thread_t *to);


/* Takes back the SC t donated, wherever it ended up. t now blocks on nobody. */
void _sc_reclaim(struct _thread_t *t);


/* Repoints SCs pushed by threads on queue from one holder to another. */
void _sc_migrate(struct _thread_t *from, struct _thread_t *queue, struct _thread_t *to);

#endif
