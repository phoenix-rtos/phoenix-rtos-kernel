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
#include "lib/lib.h"
#include "syspage.h"


static struct {
	idtree_t tree;
	lock_t port_lock;
} port_common;


msg_rid_t proc_portRidAlloc(port_t *p, kmsg_t *kmsg)
{
	msg_rid_t ret;

	(void)proc_lockSet(&p->lock);
	ret = lib_idtreeAlloc(&p->rid, &kmsg->idlinkage, 0);
	(void)proc_lockClear(&p->lock);

	return ret;
}


kmsg_t *proc_portRidGet(port_t *p, msg_rid_t rid)
{
	kmsg_t *kmsg;

	(void)proc_lockSet(&p->lock);

	kmsg = lib_treeof(kmsg_t, idlinkage, lib_idtreeFind(&p->rid, rid));
	if (kmsg != NULL) {
		lib_idtreeRemove(&p->rid, &kmsg->idlinkage);
	}

	(void)proc_lockClear(&p->lock);

	return kmsg;
}


port_t *proc_portGet(u32 id)
{
	port_t *port;
	spinlock_ctx_t sc;

	if (id > MAX_ID) {
		return NULL;
	}

	(void)proc_lockSet(&port_common.port_lock);
	port = lib_treeof(port_t, linkage, lib_idtreeFind(&port_common.tree, (int)id));
	if (port != NULL) {
		hal_spinlockSet(&port->spinlock, &sc);
		port->refs++;
		hal_spinlockClear(&port->spinlock, &sc);
	}
	(void)proc_lockClear(&port_common.port_lock);

	return port;
}


void port_put(port_t *p, int destroy)
{
	spinlock_ctx_t sc;

	(void)proc_lockSet(&port_common.port_lock);
	hal_spinlockSet(&p->spinlock, &sc);
	p->refs--;

	if (destroy != 0) {
		p->closed = 1;
	}

	if (p->refs != 0) {
		if (destroy != 0) {
			/* Wake receivers up */
			(void)proc_threadBroadcast(&p->threads);
		}

		hal_spinlockClear(&p->spinlock, &sc);
		(void)proc_lockClear(&port_common.port_lock);
		return;
	}

	hal_spinlockClear(&p->spinlock, &sc);
	lib_idtreeRemove(&port_common.tree, &p->linkage);
	(void)proc_lockClear(&port_common.port_lock);

	(void)proc_lockSet(&p->owner->lock);
	if (p->next != NULL) {
		LIST_REMOVE(&p->owner->ports, p);
	}
	(void)proc_lockClear(&p->owner->lock);

	(void)proc_lockDone(&p->lock);
	hal_spinlockDestroy(&p->spinlock);
	vm_kfree(p);
}


static int port_create(process_t *proc, syspage_named_port_t *namedPort, u32 *id)
{
	port_t *port;

	port = vm_kmalloc(sizeof(port_t));
	if (port == NULL) {
		return -ENOMEM;
	}

	(void)proc_lockSet(&port_common.port_lock);
	if (lib_idtreeAlloc(&port_common.tree, &port->linkage, 0) < 0) {
		(void)proc_lockClear(&port_common.port_lock);
		vm_kfree(port);
		return -ENOMEM;
	}

	port->kmessages = NULL;
	hal_spinlockCreate(&port->spinlock, "port.spinlock");

	lib_idtreeInit(&port->rid);
	(void)proc_lockInit(&port->lock, &proc_lockAttrDefault, "port.rid");

	port->threads = NULL;
	port->current = NULL;
	port->refs = 1;
	port->closed = 0;

	*id = (u32)port->linkage.id;
	port->owner = proc;
	port->namedPort = namedPort;
	(void)proc_lockClear(&port_common.port_lock);

	if (proc != NULL) {
		(void)proc_lockSet(&proc->lock);
		LIST_ADD((&proc->ports), port);
		(void)proc_lockClear(&proc->lock);
	}

	return EOK;
}


int proc_portCreate(u32 *id)
{
	thread_t *curr = proc_current();
	process_t *proc = (curr == NULL) ? NULL : curr->process;
	*id = 0;
	return port_create(proc, NULL, id);
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


static int msg_isNamedPortAllowed(unsigned int allowMask, process_t *process)
{
	if ((process == NULL) || (process->partition == NULL) ||
			((allowMask & (1UL << process->partition->config->id)) != 0U)) {
		return 1;
	}
	return 0;
}


static int msg_isOwnerAllowed(process_t *owner, process_t *process)
{
	if ((owner == NULL) || (process == NULL) ||
			(process->partition == NULL) || (owner->partition == NULL) ||
			((owner->partition == process->partition))) {
		return 1;
	}
	return 0;
}


int proc_isPortAllowed(port_t *port, process_t *process, int isRecv)
{
	unsigned int allowMask;
	if (port->namedPort != NULL) {
		allowMask = isRecv != 0 ? port->namedPort->recvMask : port->namedPort->sendMask;
		return msg_isNamedPortAllowed(allowMask, process) != 0 ? 1 : 0;
	}
	return msg_isOwnerAllowed(port->owner, process) != 0 ? 1 : 0;
}


void _port_init(void)
{
	syspage_named_port_t *port;
	u32 id;

	lib_idtreeInit(&port_common.tree);
	(void)proc_lockInit(&port_common.port_lock, &proc_lockAttrDefault, "port.common");

	port = syspage_namedPortsList();
	if (port != NULL) {
		do {
			if (port_create(NULL, port, &id) == 0) {
				port->portId = id;
			}
			else {
				port->portId = (unsigned int)-1;
			}

			port = port->next;
		} while (port != syspage_namedPortsList());
	}
}
