/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Ports
 *
 * Copyright 2017, 2018, 2023 Phoenix Systems
 * Author: Jakub Sejdak, Pawel Pisarczyk, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_PROC_PORT_H_
#define _PH_PROC_PORT_H_

#include <stdatomic.h>

#include "hal/hal.h"
#include "lib/lib.h"
#include "include/errno.h"
#include "process.h"
#include "threads.h"


typedef struct _port_t {
	idnode_t linkage;
	struct _port_t *next;
	struct _port_t *prev;

	process_t *owner;

	_Atomic int refs, closed;

	spinlock_t spinlock;
	thread_t *threads;

	/* to be merged with threads once old impl is ditched */
	prio_queue_t queue;

	__u8 pulse;
} port_t;

/* FIXME - use int for port handle.
 * Or even better, use dedicated type. */

int proc_portCreate(u32 *id);


void proc_portDestroy(u32 port);


void proc_portsDestroy(process_t *proc);


port_t *proc_portGet(u32 id);


void port_put(port_t *p, int destroy);


void _port_init(void);


#endif
