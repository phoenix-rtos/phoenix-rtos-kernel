/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Messages (no MMU)
 *
 * Copyright 2017, 2018 Phoenix Systems
 * Author: Jakub Sejdak, Pawel Pisarczyk, Aleksander Kaminski, Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "include/errno.h"
#include "lib/lib.h"
#include "proc.h"
#include "vm/vm.h"


enum { msg_rejected = -1, msg_waiting = 0, msg_received, msg_responded };


struct {
	vm_map_t *kmap;
	vm_object_t *kernel;
} msg_common;


int proc_send(u32 port, msg_t *msg)
{
	port_t *p;
	int err = EOK;
	kmsg_t kmsg;
	thread_t *sender;
	spinlock_ctx_t sc;

	p = proc_portGet(port);
	if (p == NULL) {
		return -EINVAL;
	}

	sender = proc_current();

	kmsg.msg = msg;
	kmsg.src = sender->process;
	kmsg.threads = NULL;
	kmsg.state = msg_waiting;

	kmsg.msg->pid = (sender->process != NULL) ? process_getPid(sender->process) : 0;
	kmsg.msg->priority = sender->priority;

	hal_spinlockSet(&p->spinlock, &sc);

	if (p->closed) {
		err = -EINVAL;
	}
	else {
		LIST_ADD(&p->kmessages, &kmsg);
		proc_threadWakeup(&p->threads);

		while ((kmsg.state != msg_responded) && (kmsg.state != msg_rejected)) {
			err = proc_threadWaitInterruptible(&kmsg.threads, &p->spinlock, 0, &sc);

			if ((err != EOK) && (kmsg.state == msg_waiting)) {
				LIST_REMOVE(&p->kmessages, &kmsg);
				break;
			}
		}

		switch (kmsg.state) {
			case msg_responded:
				err = EOK; /* Don't report EINTR if we got the response already */
				break;
			case msg_rejected:
				err = -EINVAL;
				break;
			default:
				break;
		}
	}

	hal_spinlockClear(&p->spinlock, &sc);

	port_put(p, 0);

	return err;
}


static void proc_msgReject(kmsg_t *kmsg, port_t *p)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&p->spinlock, &sc);
	kmsg->state = msg_rejected;
	proc_threadWakeup(&kmsg->threads);
	hal_spinlockClear(&p->spinlock, &sc);

	port_put(p, 0);
}


int proc_recv(u32 port, msg_t *msg, msg_rid_t *rid)
{
	port_t *p;
	kmsg_t *kmsg;
	spinlock_ctx_t sc;
	int err = 0;
	thread_t *current = proc_current();
	void *idata = NULL;

	p = proc_portGet(port);
	if (p == NULL) {
		return -EINVAL;
	}

	hal_spinlockSet(&p->spinlock, &sc);

	while ((p->kmessages == NULL) && (p->closed == 0) && (err != -EINTR)) {
		err = proc_threadWaitInterruptible(&p->threads, &p->spinlock, 0, &sc);
	}

	kmsg = p->kmessages;
	if (kmsg != NULL) {
		kmsg->state = msg_received;
		LIST_REMOVE(&p->kmessages, kmsg);
	}

	if (p->closed) {
		/* Port is being removed */
		if (kmsg != NULL) {
			kmsg->state = msg_rejected;
			proc_threadWakeup(&kmsg->threads);
		}

		err = -EINVAL;
	}
	hal_spinlockClear(&p->spinlock, &sc);

	if (err < 0) {
		port_put(p, 0);
		return err;
	}

	if (proc_portRidAlloc(p, kmsg) < 0) {
		proc_msgReject(kmsg, p);
		return -ENOMEM;
	}

	*rid = lib_idtreeId(&kmsg->idlinkage);

	hal_memcpy(msg, kmsg->msg, sizeof(*msg));

	kmsg->imapped = NULL;
	kmsg->omapped = NULL;

	if ((kmsg->msg->i.data != NULL) && (kmsg->msg->i.size != 0) && (current->process != NULL) &&
			(pmap_isAllowed(current->process->pmapp, kmsg->msg->i.data, kmsg->msg->i.size) == 0)) {

		idata = vm_mmap(current->process->mapp, NULL, NULL, round_page(kmsg->msg->i.size),
			PROT_READ | PROT_USER, NULL, -1, MAP_ANONYMOUS);
		if (idata == NULL) {
			/* Free RID */
			(void)proc_portRidGet(p, *rid);
			proc_msgReject(kmsg, p);
			return -ENOMEM;
		}
		hal_memcpy(idata, kmsg->msg->i.data, kmsg->msg->i.size);
		kmsg->imapped = idata;
		msg->i.data = idata;
	}

	if ((kmsg->msg->o.data != NULL) && (kmsg->msg->o.size != 0) && (current->process != NULL) &&
			(pmap_isAllowed(current->process->pmapp, kmsg->msg->o.data, kmsg->msg->o.size) == 0)) {

		msg->o.data = vm_mmap(current->process->mapp, NULL, NULL, round_page(kmsg->msg->o.size),
			PROT_READ | PROT_WRITE | PROT_USER, NULL, -1, MAP_ANONYMOUS);
		if (msg->o.data == NULL) {
			if (idata != NULL) {
				vm_munmap(current->process->mapp, idata, round_page(kmsg->msg->i.size));
			}

			/* Free RID */
			(void)proc_portRidGet(p, *rid);
			proc_msgReject(kmsg, p);
			return -ENOMEM;
		}
		kmsg->omapped = msg->o.data;
	}

	port_put(p, 0);

	return EOK;
}


int proc_respond(u32 port, msg_t *msg, msg_rid_t rid)
{
	port_t *p;
	size_t s = 0;
	kmsg_t *kmsg;
	spinlock_ctx_t sc;
	thread_t *current = proc_current();

	p = proc_portGet(port);
	if (p == NULL) {
		return -EINVAL;
	}

	kmsg = proc_portRidGet(p, rid);
	if (kmsg == NULL) {
		return -ENOENT;
	}

	hal_memcpy(kmsg->msg->o.raw, msg->o.raw, sizeof(msg->o.raw));
	kmsg->msg->o.err = msg->o.err;

	if (kmsg->imapped != NULL) {
		vm_munmap(current->process->mapp, kmsg->imapped, round_page(kmsg->msg->i.size));
	}

	if (kmsg->omapped != NULL) {
		hal_memcpy(kmsg->msg->o.data, kmsg->omapped, kmsg->msg->o.size);
		vm_munmap(current->process->mapp, kmsg->omapped, round_page(kmsg->msg->o.size));
	}

	hal_spinlockSet(&p->spinlock, &sc);
	kmsg->state = msg_responded;
	kmsg->src = current->process;
	proc_threadWakeup(&kmsg->threads);
	hal_spinlockClear(&p->spinlock, &sc);
	port_put(p, 0);

	return s;
}


void *proc_configure(void)
{
	void *vaddr;
	thread_t *t;

	t = proc_current();

	if (t->utcb.w != NULL) {
		return t->utcb.w;
	}

	/* TODO: cleanups on exit */

	/* map to current thread space */
	vaddr = vm_mmap(t->process->mapp, NULL, NULL, round_page(sizeof(msg_t)), PROT_WRITE | PROT_READ | PROT_USER, NULL, -1, MAP_ANONYMOUS);
	if (vaddr == NULL) {
		return NULL;
	}
	t->utcb.w = vaddr;
	t->utcb.kw = vaddr;

	return vaddr;
}


void _msg_init(vm_map_t *kmap, vm_object_t *kernel)
{
	msg_common.kmap = kmap;
	msg_common.kernel = kernel;
}
