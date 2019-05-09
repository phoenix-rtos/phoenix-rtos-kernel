/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Messages (no MMU)
 *
 * Copyright 2017, 2018 Phoenix Systems
 * Author: Jakub Sejdak, Pawel Pisarczyk, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "../../include/errno.h"
#include "../lib/lib.h"
#include "proc.h"


struct {
	vm_map_t *kmap;
	vm_object_t *kernel;
} msg_common;


int proc_send(u32 port, msg_t *msg)
{
	port_t *p;
	int responded = 0;
	int err = EOK;
	kmsg_t kmsg;
	thread_t *sender;

	if ((p = proc_portGet(port)) == NULL)
		return -EINVAL;

	sender = proc_current();

	kmsg.msg = msg;
	kmsg.src = sender->process;
	kmsg.threads = NULL;
	kmsg.responded = 0;

	kmsg.msg->pid = (sender->process != NULL) ? sender->process->id : 0;
	kmsg.msg->priority = sender->priority;

	hal_spinlockSet(&p->spinlock);

	if (p->closed) {
		err = -EINVAL;
	}
	else {
		LIST_ADD(&p->kmessages, &kmsg);

		proc_threadWakeup(&p->threads);

		while (!(responded = kmsg.responded))
			err = proc_threadWait(&kmsg.threads, &p->spinlock, 0);
	}

	hal_spinlockClear(&p->spinlock);

	port_put(p, 0);

	return responded < 0 ? -EINVAL : err;
}


int proc_recv(u32 port, msg_t *msg, unsigned int *rid)
{
	port_t *p;
	kmsg_t *kmsg;

	if ((p = proc_portGet(port)) == NULL)
		return -EINVAL;

	hal_spinlockSet(&p->spinlock);

	while (p->kmessages == NULL && !p->closed)
		proc_threadWait(&p->threads, &p->spinlock, 0);

	kmsg = p->kmessages;

	if (p->closed) {
		/* Port is being removed */
		if (kmsg != NULL) {
			kmsg->responded = -1;
			LIST_REMOVE(&p->kmessages, kmsg);
			proc_threadWakeup(&kmsg->threads);
		}

		hal_spinlockClear(&p->spinlock);
		port_put(p, 0);
		return -EINVAL;
	}

	LIST_REMOVE(&p->kmessages, kmsg);
	LIST_ADD(&p->received, kmsg);
	hal_spinlockClear(&p->spinlock);

	/* (MOD) */
	(*rid) = (unsigned long)(kmsg);

	hal_memcpy(msg, kmsg->msg, sizeof(*msg));

	port_put(p, 0);
	return EOK;
}


int proc_respond(u32 port, msg_t *msg, unsigned int rid)
{
	port_t *p;
	size_t s = 0;
	kmsg_t *kmsg = (kmsg_t *)(unsigned long)rid;

	if ((p = proc_portGet(port)) == NULL)
		return -EINVAL;

	hal_memcpy(kmsg->msg->o.raw, msg->o.raw, sizeof(msg->o.raw));

	hal_spinlockSet(&p->spinlock);
	kmsg->responded = 1;
	kmsg->src = proc_current()->process;
	LIST_REMOVE(&p->received, kmsg);
	proc_threadWakeup(&kmsg->threads);
	hal_spinlockClear(&p->spinlock);
	port_put(p, 0);

	return s;
}


void _msg_init(vm_map_t *kmap, vm_object_t *kernel)
{
	msg_common.kmap = kmap;
	msg_common.kernel = kernel;
}
