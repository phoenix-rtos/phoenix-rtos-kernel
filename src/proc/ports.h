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

#include HAL
#include "msg.h"
#include "process.h"
#include "threads.h"


typedef struct _port_t {
	rbnode_t linkage;
	struct _port_t *next;
	struct _port_t *prev;

	u32 id;
	u32 lmaxgap;
	u32 rmaxgap;
	kmsg_t *kmessages;
	kmsg_t *received;
	process_t *owner;
	int refs, closed;

	spinlock_t spinlock;
	thread_t *threads;
	msg_t *current;
} port_t;


extern int proc_portCreate(u32 *port);


extern void proc_portDestroy(u32 port);


extern void proc_portsDestroy(process_t *proc);


extern port_t *proc_portGet(u32 id);


extern void port_put(port_t *p, int destroy);


extern void _port_init(void);


#endif
