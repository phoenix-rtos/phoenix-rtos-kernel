/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Futex
 *
 * Copyright 2025 Phoenix Systems
 * Author: Kamil Kowalczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PROC_FUTEX_H_
#define _PROC_FUTEX_H_

#include "include/types.h"


/* Implementation inspired by: https://github.com/openbsd/src/blob/master/sys/kern/sys_futex.c */


#define FUTEX_SLEEPQUEUES_BITS 6
#define FUTEX_SLEEPQUEUES_SIZE (1U << FUTEX_SLEEPQUEUES_BITS)
#define FUTEX_SLEEPQUEUES_MASK (FUTEX_SLEEPQUEUES_SIZE - 1)
#define FUTEX_WAKEUP_ALL       ((u32) - 1)


typedef struct _futex_waitctx_t {
	struct _futex_waitctx_t *prev, *next;
	_Atomic(struct _thread_t *) thread;
} futex_waitctx_t;


typedef struct {
	struct _thread_t *threads;
	spinlock_t spinlock;
	addr_t address;
	futex_waitctx_t *waitctxs;
} futex_sleepqueue_t;


int proc_futexWait(_Atomic(u32) *address, u32 value, time_t timeout, int clockType);


int proc_futexWakeup(struct _process_t *process, _Atomic(u32) *address, u32 wakeCount);


#endif
