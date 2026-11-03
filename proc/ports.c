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
			/* Wake receivers up */
			hal_spinlockSet(&p->spinlock, &sc);
			(void)proc_threadBroadcast(&p->threads);
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
			(void)proc_lockInit(&port->lock, &proc_lockAttrDefault, "port.rid");
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
	port->kmessages = NULL;
	lib_idtreeInit(&port->rid);
	port->threads = NULL;
	port->current = NULL;
	port->refs = 1;
	port->closed = 0;
	port->owner = proc;

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
