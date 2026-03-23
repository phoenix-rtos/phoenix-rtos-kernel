/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Futexes
 *
 * Copyright 2026 Phoenix Systems
 * Author: Jakub Smolaga
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_PROC_FUTEX_H_
#define _PH_PROC_FUTEX_H_

#include "resource.h"


typedef struct _futex_t {
	resource_t resource;
	volatile u32 *uaddr;
	spinlock_t spinlock;
	thread_t *queue;
} futex_t;


futex_t *futex_get(handle_t h);


void futex_put(futex_t *futex);


int proc_futexCreate(handle_t *h, u32 *uaddr);


int proc_futexWake(handle_t h, int n);


int proc_futexWait(handle_t h, u32 val, time_t timeout);


int proc_futexRequeue(handle_t h1, handle_t h2, u32 val, int n);


void _futex_init(void);


#endif
