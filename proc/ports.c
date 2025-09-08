/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Ports
 *
 * Copyright 2017, 2018, 2023 Phoenix Systems
 * Author: Jakub Sejdak, Pawel Pisarczyk, Aleksander Kaminski, Jan Sikorski
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "ports.h"
#include "sc.h"
#include "threads.h"
#include "lib/lib.h"


/* How often a kernel thread waiting for a receiver checks whether the port is dead. */
#define PORT_KERNEL_RECHECK_US 10000


static struct {
	table_t table;
	port_t *free;
	spinlock_t spinlock;
} port_common;


/* Takes a reference, unless the port is already dead. */
static int _port_ref(port_t *p)
{
	int refs = __atomic_load_n(&p->refs, __ATOMIC_ACQUIRE);

	while (refs != 0) {
		if (__atomic_compare_exchange_n(&p->refs, &refs, refs + 1, 1, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE) != 0) {
			return 1;
		}
	}

	return 0;
}


int port_release(port_t *p)
{
	int refs = __atomic_sub_fetch(&p->refs, 1, __ATOMIC_ACQ_REL);
	LIB_ASSERT(refs >= 0, "port_release on refs=0");
	return refs;
}


void _portBusyRemove(thread_t *t)
{
	port_t *p = t->busyPort;

	if (p != NULL) {
		LIST_REMOVE_EX(&p->busy, t, bnext, bprev);
		t->busyPort = NULL;
	}
}


/* Blocks t on one of the port's queue slots, until timeout if nonzero. */
static void _portQueueAdd(port_t *p, thread_t *t, unsigned int slot, u8 state, time_t timeout)
{
	int refs;

	p->queue.nonempty |= 1U << slot;
	_proc_threadEnqueueThread(t, &p->queue.slot[slot], state, timeout, 1);

	t->waitPort = p;

	refs = __atomic_fetch_add(&p->refs, 1, __ATOMIC_RELAXED);
	LIB_ASSERT(refs > 0, "queueing on a port nobody holds");
	(void)refs;
}


void _portSenderRequeue(thread_t *t, u8 priority)
{
	port_t *p = t->waitPort;
	unsigned int from, to;

	if (p == NULL) {
		return;
	}

	from = PORT_QUEUE_SEND(t->priority);
	to = PORT_QUEUE_SEND(priority);

	if ((from == to) || (t->wait != &p->queue.slot[from])) {
		/* nothing to do, or waiting as a receiver, which has no priority slot */
		return;
	}

	LIST_REMOVE_EX(&p->queue.slot[from], t, qnext, qprev);
	if (p->queue.slot[from] == NULL) {
		p->queue.nonempty &= ~(1U << from);
	}

	LIST_ADD_EX(&p->queue.slot[to], t, qnext, qprev);
	p->queue.nonempty |= 1U << to;
	t->wait = &p->queue.slot[to];
}


/* The best-priority sender waiting on p, or NULL if there is none. */
static thread_t *_portBestSender(port_t *p)
{
	u32 mask = p->queue.nonempty & ~(1U << PORT_QUEUE_RECV);
	unsigned int slot;
	thread_t *t;

	while (mask != 0U) {
		slot = (unsigned int)__builtin_ctz(mask);
		t = p->queue.slot[slot];

		if (t != NULL) {
			return t;
		}

		/* the slot emptied without its bit being cleared, see ports.h */
		p->queue.nonempty &= ~(1U << slot);
		mask &= ~(1U << slot);
	}

	return NULL;
}


/* The receiver waiting on p, or NULL if there is none. */
thread_t *_portWaitingReceiver(port_t *p)
{
	thread_t *recv = p->queue.slot[PORT_QUEUE_RECV];

	if (recv == NULL) {
		p->queue.nonempty &= ~(1U << PORT_QUEUE_RECV);
	}

	return recv;
}


/*
 * Dequeues recv and marks it busy, so only the sender that claimed it can give
 * it a message. It stays unschedulable until that sender donates its SC.
 */
static void _portClaim(port_t *p, thread_t *recv)
{
	LIB_ASSERT(recv->busyPort == NULL, "receiver already serves a port");

	_proc_threadUnlink(recv);

	/* READY would make _threads_schedule() put it back on the ready queue */
	recv->state = BLOCKED_ON_RECV;
	recv->interruptible = 0;

	/* the busy list is what tells a sender whom to boost while it waits */
	recv->busyPort = p;
	LIST_ADD_EX(&p->busy, recv, bnext, bprev);
}


/*
 * Makes a receiver available again, to the best-priority waiting sender in this
 * same critical section so a later sender cannot barge past a queued one.
 */
void _portReturnReceiver(port_t *p, thread_t *recv)
{
	thread_t *client;

	_portBusyRemove(recv);

	client = _portBestSender(p);
	if (client == NULL) {
		_portQueueAdd(p, recv, PORT_QUEUE_RECV, BLOCKED_ON_RECV, 0);
		return;
	}

	_portClaim(p, recv);
	client->granted = recv;

	/* the client no longer waits for a busy receiver, so it needs its SC back */
	_sc_reclaim(client);
	_proc_threadDequeue(client);
}


/*
 * Undoes the claim of a receiver that is being killed. It was not interruptible
 * while claimed, so it leaves _recvWait() with -EINTR only now, and dies.
 */
void _portReleaseDying(thread_t *recv)
{
	_portBusyRemove(recv);
	_threads_setActiveSc(recv, _sc_best(recv));
	_proc_threadDequeue(recv);
}


/*
 * Takes back the SCs donated by waiting senders, so recv can die without
 * freeing an SC a sender still expects back. The senders keep waiting.
 */
void _portDropBoosts(thread_t *recv)
{
	while (recv->boosters != NULL) {
		/* clears the WAIT_RECV edge, which takes the sender off the list */
		_sc_reclaim(recv->boosters);
	}
}


/* Claims a receiver for a caller not yet queued. NULL if it has to wait. */
static thread_t *_portClaimReceiver(port_t *p, thread_t *caller)
{
	thread_t *recv, *best;

	for (;;) {
		recv = caller->granted;
		if (recv != NULL) {
			/* given to us while we were waiting */
			caller->granted = NULL;
		}
		else {
			best = _portBestSender(p);
			if ((best != NULL) && (best->priority <= caller->priority)) {
				LIB_ASSERT(_portWaitingReceiver(p) == NULL, "receiver and sender are in the queue at the same time");
				return NULL;
			}

			recv = _portWaitingReceiver(p);
			if (recv != NULL) {
				_portClaim(p, recv);
			}
		}

		/* a receiver killed after it was claimed was not woken, it would get the message dying */
		if ((recv == NULL) || (recv->exit == 0U)) {
			return recv;
		}
		_portReleaseDying(recv);
	}
}


/*
 * 1 if no thread can ever receive on this port again (its owner process is
 * being destroyed).
 * WARN: assumes the port is still open, implying that p->owner is still alive.
 */
static int _portOwnerGone(const port_t *p)
{
	const thread_t *t;

	if (p->owner == NULL) {
		/* kernel port: no process to lose */
		return 0;
	}

	t = p->owner->threads;
	if (t == NULL) {
		return 1;
	}

	do {
		if ((t->exit == 0U) && (t->state != GHOST)) {
			return 0;
		}
		t = t->procnext;
	} while (t != p->owner->threads);

	return 1;
}


/* Blocks the caller until a receiver is available, donating its SC to a busy one meanwhile */
static int _portWait(port_t *p, thread_t *caller, int kernel, spinlock_ctx_t *scp)
{
	spinlock_ctx_t tsc;
	thread_t *recv = NULL, *t;
	int err;

	hal_spinlockSet(&threads_spinlock, &tsc);

	if (caller->exit != 0U) {
		/* Waiting in this state can lead to becoming a hanging zombie */
		hal_spinlockClear(&threads_spinlock, &tsc);
		return -EINTR;
	}

	if (_portOwnerGone(p) != 0) {
		hal_spinlockClear(&threads_spinlock, &tsc);
		return -EINVAL;
	}

	/*
	 * Nothing wakes a sender when the owner loses its last receiver later. A user
	 * call gets -EINVAL once the reaper closes the port, but a call made by the
	 * kernel may come from the reaper itself (closing files of a dead process),
	 * so it checks again.
	 */
	_portQueueAdd(p, caller, PORT_QUEUE_SEND(caller->priority), BLOCKED_ON_SEND,
			(kernel != 0) ? (_proc_gettimeRaw() + PORT_KERNEL_RECHECK_US) : 0);
	_threads_enqueued(caller);

	t = p->busy;
	if (t != NULL) {
		/* find lowest-prio busy receiver to boost */
		do {
			if (((recv == NULL) || (t->priority > recv->priority)) && (_sc_reaches(t, caller) == 0)) {
				recv = t;
			}
			t = t->bnext;
		} while (t != p->busy);

		if (recv != NULL) {
			_sc_blockOnRecv(caller, recv);
			_sc_donate(caller, recv, caller->scActive);
		}
	}

	/* tsc and scp are swapped intentionally, we need to enable interrupts */
	hal_spinlockClear(&p->spinlock, &tsc);
	err = hal_cpuReschedule(&threads_spinlock, scp);
	hal_spinlockSet(&p->spinlock, scp);

	return err;
}


/* Waits for a receiver and claims one. */
int port_claim(u32 id, thread_t *caller, int kernel, port_t **pp, thread_t **recvp)
{
	spinlock_ctx_t sc, tsc;
	thread_t *recv;
	port_t *p = proc_portGet(id);
	int err;

	if (p == NULL) {
		return -EINVAL;
	}

	hal_spinlockSet(&p->spinlock, &sc);
	if (p->closed != 0) {
		err = -EINVAL;
	}
	else {
		for (;;) {
			hal_spinlockSet(&threads_spinlock, &tsc);
			recv = _portClaimReceiver(p, caller);
			hal_spinlockClear(&threads_spinlock, &tsc);

			if (recv != NULL) {
				*recvp = recv;
				err = EOK;
				break;
			}

			if (p->closed != 0) {
				err = -EINVAL;
				break;
			}

			err = _portWait(p, caller, kernel, &sc);
			if (err == -ETIME) {
				continue;
			}
			if (err < 0) {
				hal_spinlockSet(&threads_spinlock, &tsc);
				if (caller->granted != NULL) {
					/* got a receiver at the same time as the error, release it */
					_portReturnReceiver(p, caller->granted);
					caller->granted = NULL;
				}
				hal_spinlockClear(&threads_spinlock, &tsc);

				break;
			}
		}
	}
	hal_spinlockClear(&p->spinlock, &sc);

	if (err < 0) {
		port_put(p, 0);
		return err;
	}

	*pp = p;

	return EOK;
}


/* Needs p->spinlock. */
static void _portWakeAll(port_t *p)
{
	spinlock_ctx_t sc;
	unsigned int slot;
	thread_t *t;

	hal_spinlockSet(&threads_spinlock, &sc);

	while ((t = p->queue.slot[PORT_QUEUE_RECV]) != NULL) {
		_wakeReceiver(t);
	}

	for (slot = PORT_QUEUE_SEND(0); slot < PORT_QUEUE_SLOTS; slot++) {
		while ((t = p->queue.slot[slot]) != NULL) {
			_sc_reclaim(t);
			_proc_threadDequeue(t);
		}
	}

	p->queue.nonempty = 0;

	hal_spinlockClear(&threads_spinlock, &sc);
}


port_t *proc_portGet(u32 id)
{
	port_t *port;

	if (id > MAX_ID) {
		return NULL;
	}

	for (;;) {
		port = lib_tableGet(&port_common.table, (int)id);
		if (port == NULL) {
			return NULL;
		}

		if (_port_ref(port) == 0) {
			/* dying */
			return NULL;
		}

		if (lib_tableGet(&port_common.table, (int)id) == port) {
			return port;
		}

		/* recycled under us - it belongs to some other id now */
		port_put(port, 0);
	}
}


void port_free(port_t *p)
{
	spinlock_ctx_t sc;
	thread_t *t;

	hal_spinlockSet(&threads_spinlock, &sc);

	/* anyone still queued would be holding a reference of their own */
	LIB_ASSERT(p->queue.slot[PORT_QUEUE_RECV] == NULL, "receivers should already be popped from the port");
	LIB_ASSERT(_portBestSender(p) == NULL, "senders should already be woken up");

	while ((t = p->busy) != NULL) {
		_portBusyRemove(t);
	}

	hal_spinlockClear(&threads_spinlock, &sc);

	if (p->next != NULL) {
		proc_lockSet(&p->owner->lock);
		LIST_REMOVE(&p->owner->ports, p);
		proc_lockClear(&p->owner->lock);
	}

	hal_spinlockSet(&port_common.spinlock, &sc);
	(void)lib_tableRemove(&port_common.table, (int)p->id);
	p->next = port_common.free;
	port_common.free = p;
	hal_spinlockClear(&port_common.spinlock, &sc);
}


void port_put(port_t *p, int destroy)
{
	spinlock_ctx_t sc;

	if (destroy != 0) {
		hal_spinlockSet(&p->spinlock, &sc);
		p->closed = 1;
		hal_spinlockClear(&p->spinlock, &sc);
	}

	if (port_release(p) != 0) {
		if (destroy != 0) {
			hal_spinlockSet(&p->spinlock, &sc);
			_portWakeAll(p);
			hal_spinlockClear(&p->spinlock, &sc);
		}

		return;
	}

	port_free(p);
}


static port_t *_port_alloc(void)
{
	port_t *port;
	spinlock_ctx_t sc;

	hal_spinlockSet(&port_common.spinlock, &sc);
	port = port_common.free;
	if (port != NULL) {
		port_common.free = port->next;
	}
	hal_spinlockClear(&port_common.spinlock, &sc);

	if (port == NULL) {
		port = vm_kmalloc(sizeof(port_t));
		if (port != NULL) {
			hal_spinlockCreate(&port->spinlock, "port.spinlock");
		}
	}

	return port;
}


int proc_portCreate(u32 *id)
{
	port_t *port;
	thread_t *curr = proc_current();
	process_t *proc = (curr == NULL) ? NULL : curr->process;
	spinlock_ctx_t sc;
	table_block_t *block;
	size_t size;
	int res;

	port = _port_alloc();
	if (port == NULL) {
		return -ENOMEM;
	}

	port->next = NULL;
	port->busy = NULL;
	port->refs = 1;
	port->closed = 0;

	port->pulse = 0;
	port->pulsePending = 0;
	port->owner = proc;

	hal_memset(&port->queue, 0, sizeof(port->queue));

	/* The port is fully initialized before it becomes reachable by its id. */
	for (;;) {
		hal_spinlockSet(&port_common.spinlock, &sc);
		res = lib_tableAdd(&port_common.table, port);
		if (res >= 0) {
			port->id = (u32)res;
		}
		hal_spinlockClear(&port_common.spinlock, &sc);

		if (res >= 0) {
			break;
		}

		/* No free id, grow the table */
		size = lib_tableNextSize(&port_common.table);
		block = (size != 0) ? vm_kmalloc(TABLE_BLOCK_BYTES(size)) : NULL;
		if (block == NULL) {
			hal_spinlockSet(&port_common.spinlock, &sc);
			port->next = port_common.free;
			port_common.free = port;
			hal_spinlockClear(&port_common.spinlock, &sc);
			return -ENOMEM;
		}

		hal_spinlockSet(&port_common.spinlock, &sc);
		res = lib_tableGrow(&port_common.table, block, size);
		hal_spinlockClear(&port_common.spinlock, &sc);

		if (res < 0) {
			/* somebody grew it first */
			vm_kfree(block);
		}
	}

	*id = port->id;

	if (proc != NULL) {
		(void)proc_lockSet(&proc->lock);
		LIST_ADD((&proc->ports), port);
		(void)proc_lockClear(&proc->lock);
	}

	return EOK;
}


void proc_portDestroy(u32 port)
{
	port_t *p = proc_portGet(port);
	thread_t *curr = proc_current();
	process_t *proc = (curr == NULL) ? NULL : curr->process;
	int own;

	if (p == NULL) {
		return;
	}

	own = ((p->closed == 0) && ((proc == NULL) || (p->owner == proc))) ? 1 : 0;

	port_put(p, 0);
	if (own != 0) {
		port_put(p, 1);
	}
}


void proc_portsDestroy(process_t *proc)
{
	port_t *p;

	for (;;) {
		(void)proc_lockSet(&proc->lock);
		p = proc->ports;
		if (p == NULL) {
			(void)proc_lockClear(&proc->lock);
			break;
		}
		LIST_REMOVE(&proc->ports, p);
		(void)proc_lockClear(&proc->lock);
		port_put(p, 1);
	}
}


void _port_init(void)
{
	lib_tableInit(&port_common.table);
	port_common.free = NULL;
	hal_spinlockCreate(&port_common.spinlock, "ports.spinlock");
}
