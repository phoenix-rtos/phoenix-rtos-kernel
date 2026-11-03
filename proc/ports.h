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


#define PORT_QUEUE_RECV       0U
#define PORT_QUEUE_SEND(prio) (1U + (unsigned int)(prio))
#define PORT_QUEUE_SLOTS      (1U + NPRIOS)

typedef struct {
	struct _thread_t *slot[PORT_QUEUE_SLOTS];
	u32 nonempty;
} port_queue_t;


typedef struct _port_t {
	u32 id;
	struct _port_t *next;
	struct _port_t *prev;

	struct _port_t *gnext; /* threads_common.ghostPorts linkage */
	struct _port_t *gprev;

	process_t *owner;
	int refs; /* atomic: proc_portGet() takes a reference without any lock */
	int closed;

	spinlock_t spinlock;

	port_queue_t queue;

	thread_t *busy;

	__u8 pulse;
	__u8 pulsePending; /* a pulse arrived with no receiver waiting to take it */
} port_t;


int proc_portCreate(u32 *id);


void proc_portDestroy(u32 port);


void proc_portsDestroy(process_t *proc);


port_t *proc_portGet(u32 id);


int port_release(port_t *p);


void port_free(port_t *p);


void port_put(port_t *p, int destroy);


void _port_init(void);


int port_claim(u32 id, struct _thread_t *caller, int kernel, port_t **pp, struct _thread_t **recvp);


/* The rest require threads_spinlock (threads.h) to be held. */


void _portSenderRequeue(thread_t *t, u8 priority);


void _portBusyRemove(thread_t *t);


void _portDropBoosts(thread_t *recv);


thread_t *_portWaitingReceiver(port_t *p);


void _portReturnReceiver(port_t *p, thread_t *recv);


void _portReleaseDying(thread_t *recv);


#endif
