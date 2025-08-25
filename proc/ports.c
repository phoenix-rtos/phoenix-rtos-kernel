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
	lock_t port_lock;
} port_common;


msg_rid_t proc_portRidAlloc(port_t *p, kmsg_t *kmsg)
{
	msg_rid_t ret;

	/* MISRAC2012-RULE_17_7-a */
	(void)proc_lockSet(&p->lock);
	ret = lib_idtreeAlloc(&p->rid, &kmsg->idlinkage, 0);
	/* MISRAC2012-RULE_17_7-a */
	(void)proc_lockClear(&p->lock);

	return ret;
}


kmsg_t *proc_portRidGet(port_t *p, msg_rid_t rid)
{
	kmsg_t *kmsg;

	/* MISRAC2012-RULE_17_7-a */
	(void)proc_lockSet(&p->lock);

	kmsg = lib_idtreeof(kmsg_t, idlinkage, lib_idtreeFind(&p->rid, rid));
	if (kmsg != NULL) {
		lib_idtreeRemove(&p->rid, &kmsg->idlinkage);
	}

	/* MISRAC2012-RULE_17_7-a */
	(void)proc_lockClear(&p->lock);

	return kmsg;
}


port_t *proc_portGet(u32 id)
{
	port_t *port;
	spinlock_ctx_t sc;

	if (id > MAX_ID) {  // TBD_ Julia Czy MAX_ID rzutować na unsigned? Wcześniej był problem i rzutowany był na int
		return NULL;
	}

	/* MISRAC2012-RULE_17_7-a */
	(void)proc_lockSet(&port_common.port_lock);
	port = lib_idtreeof(port_t, linkage, lib_idtreeFind(&port_common.tree, (int)id));
	if (port != NULL) {
		hal_spinlockSet(&port->spinlock, &sc);
		port->refs++;
		hal_spinlockClear(&port->spinlock, &sc);
	}
	/* MISRAC2012-RULE_17_7-a */
	(void)proc_lockClear(&port_common.port_lock);

	return port;
}


void port_put(port_t *p, int destroy)
{
	spinlock_ctx_t sc;

	/* MISRAC2012-RULE_17_7-a */
	(void)proc_lockSet(&port_common.port_lock);
	hal_spinlockSet(&p->spinlock, &sc);
	p->refs--;

	if (destroy != 0) {
		p->closed = 1;
	}

	if (p->refs != 0) {
		if (destroy != 0) {
			/* Wake receivers up */
			/* MISRAC2012-RULE_17_7-a */
			(void)proc_threadBroadcast(&p->threads);
		}

		hal_spinlockClear(&p->spinlock, &sc);
		/* MISRAC2012-RULE_17_7-a */
		(void)proc_lockClear(&port_common.port_lock);
		return;
	}

	hal_spinlockClear(&p->spinlock, &sc);
	lib_idtreeRemove(&port_common.tree, &p->linkage);
	/* MISRAC2012-RULE_17_7-a */
	(void)proc_lockClear(&port_common.port_lock);

	/* MISRAC2012-RULE_17_7-a */
	(void)proc_lockSet(&p->owner->lock);
	if (p->next != NULL) {
		LIST_REMOVE(&p->owner->ports, p);
	}
	/* MISRAC2012-RULE_17_7-a */
	(void)proc_lockClear(&p->owner->lock);

	(void)proc_lockDone(&p->lock);
	hal_spinlockDestroy(&p->spinlock);
	vm_kfree(p);
}


int proc_portCreate(u32 *id)
{
	port_t *port;
	thread_t *curr = proc_current();
	process_t *proc = (curr == NULL) ? NULL : curr->process;

	port = vm_kmalloc(sizeof(port_t));
	if (port == NULL) {
		return -ENOMEM;
	}

	/* MISRAC2012-RULE_17_7-a */
	(void)proc_lockSet(&port_common.port_lock);
	if (lib_idtreeAlloc(&port_common.tree, &port->linkage, 0) < 0) {
		/* MISRAC2012-RULE_17_7-a */
		(void)proc_lockClear(&port_common.port_lock);
		vm_kfree(port);
		return -ENOMEM;
	}

	port->kmessages = NULL;
	hal_spinlockCreate(&port->spinlock, "port.spinlock");

	lib_idtreeInit(&port->rid);
	/* MISRAC2012-RULE_17_7-a */
	(void)proc_lockInit(&port->lock, &proc_lockAttrDefault, "port.rid");

	port->threads = NULL;
	port->current = NULL;
	port->refs = 1;
	port->closed = 0;

	*id = (u32)port->linkage.id;
	port->owner = proc;
	/* MISRAC2012-RULE_17_7-a */
	(void)proc_lockClear(&port_common.port_lock);

	if (proc != NULL) {
		/* MISRAC2012-RULE_17_7-a */
		(void)proc_lockSet(&proc->lock);
		LIST_ADD((&proc->ports), port);
		/* MISRAC2012-RULE_17_7-a */
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
		/* MISRAC2012-RULE_17_7-a */
		(void)proc_lockSet(&proc->lock);
		p = proc->ports;
		if (p == NULL) {
			/* MISRAC2012-RULE_17_7-a */
			(void)proc_lockClear(&proc->lock);
			break;
		}
		LIST_REMOVE(&proc->ports, p);
		/* MISRAC2012-RULE_17_7-a */
		(void)proc_lockClear(&proc->lock);
		port_put(p, 1);
	}
}


void _port_init(void)
{
	lib_idtreeInit(&port_common.tree);
	/* MISRAC2012-RULE_17_7-a */
	(void)proc_lockInit(&port_common.port_lock, &proc_lockAttrDefault, "port.common");
}
