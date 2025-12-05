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

#ifndef _PROC_PORT_H_
#define _PROC_PORT_H_

#include "hal/hal.h"
#include "lib/lib.h"
#include "msg.h"
#include "process.h"
#include "threads.h"


typedef struct _port_t {
	idnode_t linkage;
	struct _port_t *next;
	struct _port_t *prev;

	idtree_t rid;

	kmsg_t *kmessages;
	process_t *owner;
	int refs, closed;

	spinlock_t spinlock;
	lock_t lock;
	thread_t *threads;
	thread_t *fpThreads;
	msg_t *current;

	/* to be merged with threads once old impl is ditched */
	thread_t *queue;
} port_t;

/* FIXME - use int for port handle.
 * Or even better, use dedicated type. */

int proc_portCreate(u32 *port);


void proc_portDestroy(u32 port);


void proc_portsDestroy(process_t *proc);


port_t *proc_portGet(u32 id);


void port_put(port_t *p, int destroy);


msg_rid_t proc_portRidAlloc(port_t *p, kmsg_t *kmsg);


kmsg_t *proc_portRidGet(port_t *p, msg_rid_t rid);


msg_rid_t proc_portRidAlloc_fp(port_t *p, fmsg_t *fmsg);


fmsg_t *proc_portRidGet_fp(port_t *p, msg_rid_t rid);


void _port_init(void);


#endif
