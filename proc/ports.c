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
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "ports.h"
#include "lib/lib.h"


static struct {
	idtree_t tree;
	spinlock_t spinlock;
} port_common;


port_t *proc_portGet(u32 id)
{
	port_t *port;
	spinlock_ctx_t sc;

	if (id > MAX_ID) {
		return NULL;
	}

	hal_spinlockSet(&port_common.spinlock, &sc);
	port = lib_idtreeof(port_t, linkage, lib_idtreeFind(&port_common.tree, (int)id));
	if (port != NULL) {
		atomic_fetch_add_explicit(&port->refs, 1, memory_order_acq_rel);
	}
	hal_spinlockClear(&port_common.spinlock, &sc);

	return port;
}


void port_put(port_t *p, int destroy)
{
	spinlock_ctx_t sc;
	int refs = atomic_fetch_sub_explicit(&p->refs, 1, memory_order_release);

	(void)refs;
	LIB_ASSERT(refs > 0, "port_put on refs=0");

	if (destroy != 0) {
		atomic_store_explicit(&p->closed, 1, memory_order_relaxed);
	}

	if (refs > 1) {
		if (destroy != 0) {
			/* Wake callers up */
			(void)proc_threadBroadcastPrio(&p->queue);
		}
		return;
	}

	LIB_ASSERT(p->threads == NULL, "receivers should already be popped from the port");

	hal_spinlockSet(&port_common.spinlock, &sc);
	lib_idtreeRemove(&port_common.tree, &p->linkage);
	hal_spinlockClear(&port_common.spinlock, &sc);

	if (p->next != NULL) {
		proc_lockSet(&p->owner->lock);
		LIST_REMOVE(&p->owner->ports, p);
		proc_lockClear(&p->owner->lock);
	}

	/* Now safe to free */
	vm_kfree(p);
}


int proc_portCreate(u32 *id)
{
	port_t *port;
	thread_t *curr = proc_current();
	process_t *proc = (curr == NULL) ? NULL : curr->process;
	spinlock_ctx_t sc;

	port = vm_kmalloc(sizeof(port_t));
	if (port == NULL) {
		return -ENOMEM;
	}

	hal_spinlockCreate(&port->spinlock, "port.spinlock");

	port->threads = NULL;
	port->refs = 1;
	port->closed = 0;
	port->pulse = 0;
	port->owner = proc;
	proc_threadPrioQueueInit(&port->queue);

	hal_spinlockSet(&port_common.spinlock, &sc);
	if (lib_idtreeAlloc(&port_common.tree, &port->linkage, 0) < 0) {
		hal_spinlockClear(&port_common.spinlock, &sc);
		vm_kfree(port);
		return -ENOMEM;
	}
	*id = (u32)port->linkage.id;
	hal_spinlockClear(&port_common.spinlock, &sc);

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

	if (p == NULL) {
		return;
	}

	/* REVISIT */
	int closed = atomic_load_explicit(&p->closed, memory_order_acquire);

	if ((closed != 0) || ((proc != NULL) && (p->owner != proc))) {
		port_put(p, 0);
	}
	else {
		port_put(p, 0);
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
	lib_idtreeInit(&port_common.tree);
	hal_spinlockCreate(&port_common.spinlock, "ports.spinlock");
}
