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


struct {
	idtree_t tree;
	spinlock_t spinlock;
} port_common;


msg_rid_t proc_portRidAlloc(port_t *p, kmsg_t *kmsg)
{
	msg_rid_t ret;

	proc_lockSet(&p->lock);
	ret = lib_idtreeAlloc(&p->rid, &kmsg->idlinkage, 0);
	proc_lockClear(&p->lock);

	return ret;
}


kmsg_t *proc_portRidGet(port_t *p, msg_rid_t rid)
{
	kmsg_t *kmsg;

	proc_lockSet(&p->lock);

	kmsg = lib_idtreeof(kmsg_t, idlinkage, lib_idtreeFind(&p->rid, rid));
	if (kmsg != NULL) {
		lib_idtreeRemove(&p->rid, &kmsg->idlinkage);
	}

	proc_lockClear(&p->lock);

	return kmsg;
}


msg_rid_t proc_portRidAlloc_fp(port_t *p, fmsg_t *fmsg)
{
	msg_rid_t ret;

	proc_lockSet(&p->lock);
	ret = lib_idtreeAlloc(&p->rid, &fmsg->idlinkage, 0);
	proc_lockClear(&p->lock);

	return ret;
}


fmsg_t *proc_portRidGet_fp(port_t *p, msg_rid_t rid)
{
	fmsg_t *fmsg;

	proc_lockSet(&p->lock);

	fmsg = lib_idtreeof(fmsg_t, idlinkage, lib_idtreeFind(&p->rid, rid));
	if (fmsg != NULL) {
		lib_idtreeRemove(&p->rid, &fmsg->idlinkage);
	}

	proc_lockClear(&p->lock);

	return fmsg;
}


port_t *proc_portGet(u32 id)
{
	port_t *port;
	spinlock_ctx_t sc, psc;

	if (id > MAX_ID) {
		return NULL;
	}

	hal_spinlockSet(&port_common.spinlock, &psc);
	port = lib_idtreeof(port_t, linkage, lib_idtreeFind(&port_common.tree, (int)id));
	if (port != NULL) {
		hal_spinlockSet(&port->spinlock, &sc);
		port->refs++;
		hal_spinlockClear(&port->spinlock, &sc);
	}
	hal_spinlockClear(&port_common.spinlock, &psc);

	return port;
}


void port_put(port_t *p, int destroy)
{
	spinlock_ctx_t sc, psc;

	hal_spinlockSet(&port_common.spinlock, &psc);
	hal_spinlockSet(&p->spinlock, &sc);
	p->refs--;

	if (destroy != 0) {
		p->closed = 1;
	}

	if (p->refs != 0) {
		if (destroy != 0) {
			/* Wake receivers up */
			proc_threadBroadcast(&p->threads);
			// LIB_ASSERT(p->queue == NULL, "hm: port=%d tid=%d queue=0x%x\n", p->linkage.id, proc_getTid(p->queue), p->queue);
			proc_threadBroadcast(&p->queue);
		}

		hal_spinlockClear(&p->spinlock, &sc);
		hal_spinlockClear(&port_common.spinlock, &psc);
		return;
	}

	hal_spinlockClear(&p->spinlock, &sc);
	lib_idtreeRemove(&port_common.tree, &p->linkage);
	hal_spinlockClear(&port_common.spinlock, &psc);

	proc_lockSet(&p->owner->lock);
	if (p->next != NULL) {
		LIST_REMOVE(&p->owner->ports, p);
	}
	proc_lockClear(&p->owner->lock);

	proc_lockDone(&p->lock);
	hal_spinlockDestroy(&p->spinlock);
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

	hal_spinlockSet(&port_common.spinlock, &sc);
	if (lib_idtreeAlloc(&port_common.tree, &port->linkage, 0) < 0) {
		hal_spinlockClear(&port_common.spinlock, &sc);
		vm_kfree(port);
		return -ENOMEM;
	}

	port->kmessages = NULL;
	hal_spinlockCreate(&port->spinlock, "port.spinlock");

	lib_idtreeInit(&port->rid);
	proc_lockInit(&port->lock, &proc_lockAttrDefault, "port.rid");

	port->threads = NULL;
	port->current = NULL;
	port->refs = 1;
	port->closed = 0;

	port->fpThreads = NULL;
	port->queue = NULL;

	*id = (u32)port->linkage.id;
	port->owner = proc;
	hal_spinlockClear(&port_common.spinlock, &sc);

	if (proc != NULL) {
		proc_lockSet(&proc->lock);
		LIST_ADD((&proc->ports), port);
		proc_lockClear(&proc->lock);
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

	if ((p->closed != 0) || ((proc != NULL) && (p->owner != proc))) {
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
		proc_lockSet(&proc->lock);
		p = proc->ports;
		if (p == NULL) {
			proc_lockClear(&proc->lock);
			break;
		}
		LIST_REMOVE(&proc->ports, p);
		proc_lockClear(&proc->lock);
		port_put(p, 1);
	}
}


void _port_init(void)
{
	lib_idtreeInit(&port_common.tree);
	hal_spinlockCreate(&port_common.spinlock, "ports.spinlock");
}
