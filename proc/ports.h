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
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_PROC_PORT_H_
#define _PH_PROC_PORT_H_

#include "hal/hal.h"
#include "lib/lib.h"
#include "msg.h"
#include "process.h"
#include "threads.h"


typedef struct _port_t {
	u32 id;
	struct _port_t *next;
	struct _port_t *prev;

	idtree_t rid;

	kmsg_t *kmessages;
	process_t *owner;
	int refs; /* atomic: proc_portGet() takes a reference without any lock */
	int closed;

	spinlock_t spinlock;
	lock_t lock;
	thread_t *threads;
	msg_t *current;
} port_t;

/* FIXME - use int for port handle.
 * Or even better, use dedicated type. */

int proc_portCreate(u32 *id);


void proc_portDestroy(u32 port);


void proc_portsDestroy(process_t *proc);


port_t *proc_portGet(u32 id);


int port_release(port_t *p);


void port_free(port_t *p);


void port_put(port_t *p, int destroy);


msg_rid_t proc_portRidAlloc(port_t *p, kmsg_t *kmsg);


kmsg_t *proc_portRidGet(port_t *p, msg_rid_t rid);


void _port_init(void);


#endif
