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

#include "../include/errno.h"
#include "../lib/lib.h"
#include "proc.h"


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

	if ((p = proc_portGet(port)) == NULL)
		return -EINVAL;

	sender = proc_current();

	kmsg.msg = msg;
	kmsg.src = sender->process;
	kmsg.threads = NULL;
	kmsg.state = msg_waiting;

	kmsg.msg->pid = (sender->process != NULL) ? sender->process->id : 0;
	kmsg.msg->priority = sender->priority;

	hal_spinlockSet(&p->spinlock, &sc);

	if (p->closed) {
		err = -EINVAL;
	}
	else {
		LIST_ADD(&p->kmessages, &kmsg);

		proc_threadWakeup(&p->threads);

		while (kmsg.state != msg_responded && kmsg.state != msg_rejected)
			err = proc_threadWait(&kmsg.threads, &p->spinlock, 0, &sc);
	}

	hal_spinlockClear(&p->spinlock, &sc);

	port_put(p, 0);

	return kmsg.state == msg_rejected ? -EINVAL : err;
}


int proc_recv(u32 port, msg_t *msg, unsigned long int *rid)
{
	port_t *p;
	kmsg_t *kmsg;
	spinlock_ctx_t sc;

	if ((p = proc_portGet(port)) == NULL)
		return -EINVAL;

	hal_spinlockSet(&p->spinlock, &sc);

	while (p->kmessages == NULL && !p->closed)
		proc_threadWait(&p->threads, &p->spinlock, 0, &sc);

	kmsg = p->kmessages;

	if (p->closed) {
		/* Port is being removed */
		if (kmsg != NULL) {
			kmsg->state = msg_rejected;
			LIST_REMOVE(&p->kmessages, kmsg);
			proc_threadWakeup(&kmsg->threads);
		}

		hal_spinlockClear(&p->spinlock, &sc);
		port_put(p, 0);
		return -EINVAL;
	}

	kmsg->state = msg_received;
	LIST_REMOVE(&p->kmessages, kmsg);
	hal_spinlockClear(&p->spinlock, &sc);

	/* (MOD) */
	(*rid) = (unsigned long)(kmsg);

	hal_memcpy(msg, kmsg->msg, sizeof(*msg));

	port_put(p, 0);
	return EOK;
}


int proc_respond(u32 port, msg_t *msg, unsigned long int rid)
{
	port_t *p;
	size_t s = 0;
	kmsg_t *kmsg = (kmsg_t *)(unsigned long)rid;
	spinlock_ctx_t sc;

	if ((p = proc_portGet(port)) == NULL)
		return -EINVAL;

	hal_memcpy(kmsg->msg->o.raw, msg->o.raw, sizeof(msg->o.raw));

	hal_spinlockSet(&p->spinlock, &sc);
	kmsg->state = msg_responded;
	kmsg->src = proc_current()->process;
	proc_threadWakeup(&kmsg->threads);
	hal_spinlockClear(&p->spinlock, &sc);
	port_put(p, 0);

	return s;
}


void _msg_init(vm_map_t *kmap, vm_object_t *kernel)
{
	msg_common.kmap = kmap;
	msg_common.kernel = kernel;
}
