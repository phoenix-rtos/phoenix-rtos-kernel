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


/*
 * Senders and receivers wait in one queue: PORT_QUEUE_RECV holds receivers with
 * nothing to serve, PORT_QUEUE_SEND(prio) the senders waiting for one, which
 * keeps senders priority ordered. nonempty says which slots are used, so
 * picking the best is one count-trailing-zeros. A free receiver goes to the
 * best waiting sender at once, so the two kinds of slot are never both in use.
 *
 * A bit may stay set after its slot empties, because interrupts and thread
 * teardown dequeue threads without knowing about ports; readers clear such
 * bits. The reverse would lose a wakeup and never happens.
 */
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

	/* Receivers serving a message from this port - the threads a sender boosts. */
	thread_t *busy;

	__u8 pulse;
	__u8 pulsePending; /* a pulse arrived with no receiver waiting to take it */
} port_t;


/* FIXME - use int for port handle.
 * Or even better, use dedicated type. */

int proc_portCreate(u32 *id);


void proc_portDestroy(u32 port);


void proc_portsDestroy(process_t *proc);


port_t *proc_portGet(u32 id);


/* Drops a reference and returns what is left. Never frees; see port_free(). */
int port_release(port_t *p);


/* Finishes off a port whose last reference port_release() just dropped. */
void port_free(port_t *p);


void port_put(port_t *p, int destroy);


void _port_init(void);


/*
 * Claims a receiver of port id, waiting for a free one if need be, and returns
 * the port reference that goes with it in *pp. kernel is nonzero for a call
 * made by the kernel itself, which must not wait forever on a dead port.
 */
int port_claim(u32 id, struct _thread_t *caller, int kernel, port_t **pp, struct _thread_t **recvp);


/* The rest require threads_spinlock (threads.h) to be held. */


/* Moves a waiting sender to the slot matching its new priority. */
void _portSenderRequeue(thread_t *t, u8 priority);


/* Deregisters t from the port whose message it was serving, if any. */
void _portBusyRemove(thread_t *t);


/* Takes back the SCs donated to recv by senders waiting for it. */
void _portDropBoosts(thread_t *recv);


/* The receiver waiting on p, or NULL if there is none. */
thread_t *_portWaitingReceiver(port_t *p);


/* Makes recv available to senders again. */
void _portReturnReceiver(port_t *p, thread_t *recv);


/* Lets a claimed receiver that is being killed run to its end instead of serving. */
void _portReleaseDying(thread_t *recv);


#endif
