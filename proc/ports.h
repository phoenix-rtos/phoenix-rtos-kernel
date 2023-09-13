/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Ports
 *
 * Copyright 2017, 2018 Phoenix Systems
 * Author: Jakub Sejdak, Pawel Pisarczyk, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PROC_PORT_H_
#define _PROC_PORT_H_

#include "../hal/hal.h"
#include "../lib/lib.h"
#include "msg.h"
#include "process.h"
#include "threads.h"


typedef struct _port_t {
	rbnode_t linkage;
	struct _port_t *next;
	struct _port_t *prev;

	idtree_t rid;

	u32 id;
	u32 lmaxgap;
	u32 rmaxgap;
	kmsg_t *kmessages;
	process_t *owner;
	int refs, closed;

	spinlock_t spinlock;
	lock_t lock;
	thread_t *threads;
	msg_t *current;
} port_t;


int proc_portCreate(u32 *port);


void proc_portDestroy(u32 port);


void proc_portsDestroy(process_t *proc);


port_t *proc_portGet(u32 id);


void port_put(port_t *p, int destroy);


int proc_portRidAlloc(port_t *p, kmsg_t *kmsg);


kmsg_t *proc_portRidGet(port_t *p, unsigned int rid);


void _port_init(void);


#endif
