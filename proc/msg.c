/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Message passing
 *
 * Copyright 2017, 2018, 2026 Phoenix Systems
 * Author: Pawel Pisarczyk, Jakub Sejdak, Aleksander Kaminski, Adam Greloch
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "threads.h"
#include "msg.h"
#include "ports.h"
#include "xfer.h"
#include "perf/trace-events.h"


#define IPC_PULSED    1
#define IPC_DELIVERED 2


static struct {
	/* TODO: deduplicate kmap pointers across the kernel _common structs */
	vm_map_t *kmap;
	ptr_t ridCookie;
} msg_common;


typedef struct {
	const void *idata;
	void *odata;
	size_t isize;
	size_t osize;
	oid_t oid;
	int type;
	unsigned char raw[MSG_RAW_SIZE];
} msgHdr_t;


static void _msgHdrRead(const msg_t *msg, msgHdr_t *hdr)
{
	hal_memcpy(hdr->raw, msg->i.raw, MSG_RAW_SIZE);
	hal_memcpy(&hdr->oid, &msg->oid, sizeof(hdr->oid));

	hdr->idata = msg->i.data;
	hdr->isize = msg->i.size;
	hdr->odata = msg->o.data;
	hdr->osize = msg->o.size;
	hdr->type = msg->type;
}


static void _setReplyChain(thread_t *from, thread_t *to)
{
	LIB_ASSERT_ALWAYS(from->blockedKind == WAIT_NOTHING, "tid %d already waits, kind %u",
			proc_getTid(from), from->blockedKind);

	_thread_setBlocked(from, WAIT_CALL, to);
	LIST_ADD_EX(&to->reply, from, replyNext, replyPrev);
}


/* Undoes _setReplyChain() */
void _proc_clearReplyChain(thread_t *from)
{
	thread_t *called = _thread_called(from);

	LIB_ASSERT_ALWAYS(called != NULL, "tid %d waits for nobody, kind %u",
			proc_getTid(from), from->blockedKind);
	LIB_ASSERT(LIST_BELONGS_EX(&called->reply, from, replyNext, replyPrev) != 0,
			"caller is not on its receiver's list");

	LIST_REMOVE_EX(&called->reply, from, replyNext, replyPrev);
	_thread_clearBlocked(from, WAIT_CALL);
}


static thread_t *_ridToReply(msg_rid_t rid, thread_t *current)
{
	void *reply = (void *)((ptr_t)rid ^ msg_common.ridCookie);
	thread_t *t;

	if (reply == NULL) {
		return NULL;
	}

#ifndef NOMMU
	if (pmap_belongs(&msg_common.kmap->pmap, reply) == 0) {
		return NULL;
	}
#else
	if (((ptr_t)reply < (ptr_t)msg_common.kmap->start) ||
			((ptr_t)reply > (ptr_t)msg_common.kmap->stop - sizeof(thread_t))) {
		return NULL;
	}
#endif

	t = (thread_t *)reply;

	return (_thread_called(t) == current) ? t : NULL;
}


/* WARN: Assumes aspace of recv */
static void _msgDeliver(thread_t *caller, thread_t *recv, const msgHdr_t *hdr, void *idata, void *odata)
{
	msg_t *out = recv->ipc.msgPtr;

	hal_memcpy(out->i.raw, hdr->raw, MSG_RAW_SIZE);
	hal_memcpy(&out->oid, &hdr->oid, sizeof(out->oid));

	out->i.size = hdr->isize;
	out->i.data = idata;
	out->o.size = hdr->osize;
	out->o.data = odata;
	out->type = hdr->type;
	out->pid = (caller->process != NULL) ? process_getPid(caller->process) : 0;
	out->priority = caller->priority;

	*recv->ipc.ridPtr = (msg_rid_t)((ptr_t)caller ^ msg_common.ridCookie);
}


static int proc_send_ex(u32 port, msg_t *msg, int returnable)
{
	thread_t *caller = proc_current();
	void *idata, *odata;
	spinlock_ctx_t sc, tsc;
	cpu_context_t *ctx;
	thread_t *recv;
	msgHdr_t hdr;
	port_t *p;
	int err;

	for (;;) {
		err = port_claim(port, caller, ((returnable != 0) || (caller->process == NULL)) ? 1 : 0, &p, &recv);
		if (err < 0) {
			return err;
		}

		_msgHdrRead(msg, &hdr);

		err = xfer_begin(caller, recv, hdr.type, hdr.raw, hdr.idata, hdr.isize, hdr.odata, hdr.osize, &idata, &odata);
		if (err < 0) {
			hal_spinlockSet(&threads_spinlock, &tsc);
			_portReturnReceiver(p, recv);
			hal_spinlockClear(&threads_spinlock, &tsc);
			port_put(p, 0);
			return err;
		}

		port_put(p, 0);

		hal_spinlockSet(&threads_spinlock, &sc);

		if (recv->exit == 0U) {
			break;
		}

		/* killed since port_claim(), find another receiver */
		_portReleaseDying(recv);
		hal_spinlockClear(&threads_spinlock, &sc);
		xfer_abort(caller);
	}

	_setReplyChain(caller, recv);
	_sc_donate(caller, recv, caller->scActive);

	caller->ipc.msgPtr = msg;
	caller->state = BLOCKED_ON_REPLY;
	caller->callReturnable = (returnable != 0) ? 1U : 0U;
	recv->state = READY;

	ctx = _threads_switchTo(recv);

	hal_spinlockClear(&threads_spinlock, &sc);

	_msgDeliver(caller, recv, &hdr, idata, odata);

	hal_spinlockSet(&threads_spinlock, &sc);

	(void)__atomic_fetch_or(&recv->flags, IPC_DELIVERED, __ATOMIC_RELEASE);
	hal_cpuSetReturnValue(ctx, EOK);
	recv->fastpathExitCtx = ctx;

	LIB_ASSERT(_proc_current() == recv, "we should be recv here");

	trace_eventSyscallExit(recv->respondAndRecv ? syscall_msgRespondAndRecv : syscall_msgRecv, proc_getTid(recv));

	if ((recv->process == NULL) || (returnable != 0)) {
		/*
		 * The scheduler exits the receiver through fastpathExitCtx, so the
		 * kernel context here belongs to the caller instead (_threads_schedule).
		 */
		recv->saveCtxIn = caller;

		err = hal_cpuReschedule(&threads_spinlock, &sc);
		if (err < 0) {
			/* answered by a dying receiver */
			xfer_abort(caller);
		}

		/* recv must not be touched here, it may have died serving us and be freed already */
		LIB_ASSERT(caller->callReturnable == 0, "callReturnable not cleared?");
		LIB_ASSERT(_proc_current() == caller, "we should be a caller here");

		return err;
	}

	hal_spinlockClear(&threads_spinlock, &sc);

	return EOK;
}


int proc_send(u32 port, msg_t *msg)
{
	return proc_send_ex(port, msg, 0);
}


int proc_send_returnable(u32 port, msg_t *msg)
{
	return proc_send_ex(port, msg, 1);
}


msg_t *proc_kmsg(void)
{
	thread_t *t = proc_current();

	if (t->ipc.kmsg == NULL) {
		t->ipc.kmsg = vm_kmalloc(sizeof(msg_t));
		if (t->ipc.kmsg == NULL) {
			return NULL;
		}
	}

	hal_memset(t->ipc.kmsg, 0, sizeof(msg_t));

	return t->ipc.kmsg;
}


static void _msgHandOver(thread_t *forward, const msg_t *msg, msg_rid_t rid)
{
	/* same aspace, we can copy directly */
	hal_memcpy(forward->ipc.msgPtr, msg, sizeof(*msg));
	*forward->ipc.ridPtr = rid;

	(void)__atomic_fetch_or(&forward->flags, IPC_DELIVERED, __ATOMIC_RELEASE);
	forward->fastpathExitCtx = _threads_getUserContext(forward);
	hal_cpuSetReturnValue(forward->fastpathExitCtx, EOK);

	trace_eventSyscallExit(forward->respondAndRecv ? syscall_msgRespondAndRecv : syscall_msgRecv, proc_getTid(forward));
}


int proc_forward(u32 port, msg_t *msg, msg_rid_t rid)
{
	thread_t *recv = proc_current();
	thread_t *caller, *forward;
	sched_context_t *donated;
	spinlock_ctx_t tsc;
	port_t *p;
	int err;

	err = port_claim(port, recv, 0, &p, &forward);
	if (err < 0) {
		return err;
	}

	hal_spinlockSet(&threads_spinlock, &tsc);

	caller = (forward->process == recv->process) ? _ridToReply(rid, recv) : NULL;
	if (caller == NULL) {
		_portReturnReceiver(p, forward);
	}

	hal_spinlockClear(&threads_spinlock, &tsc);

	if (caller == NULL) {
		port_put(p, 0);
		return -EINVAL;
	}

	err = xfer_forward(caller, recv, forward, msg);
	if (err < 0) {
		hal_spinlockSet(&threads_spinlock, &tsc);
		_portReturnReceiver(p, forward);
		hal_spinlockClear(&threads_spinlock, &tsc);
		port_put(p, 0);
		return err;
	}

	_msgHandOver(forward, msg, rid);

	hal_spinlockSet(&threads_spinlock, &tsc);

	donated = caller->scPushed;

	/* TODO: optimize these */
	_proc_clearReplyChain(caller);
	_sc_return(recv, caller);
	_setReplyChain(caller, forward);
	_sc_donate(caller, forward, donated);

	caller->state = BLOCKED_ON_REPLY;
	forward->state = READY;

	/* could have changed as part of _sc_return */
	_threads_setCurrent(recv);

	if (forward->onReady == 0U) {
		_threads_readyAdd(forward);
	}

	if (forward->priority < recv->priority) {
		hal_cpuReschedule(&threads_spinlock, &tsc);
	}
	else {
		hal_spinlockClear(&threads_spinlock, &tsc);
	}

	port_put(p, 0);

	return EOK;
}


/*
 * Waits for a message. Assumes p->spinlock is set with sc and consumes the
 * port reference, because this frame is abandoned on a fastpath delivery.
 */
static int _recvWait(port_t *p, thread_t *recv, spinlock_ctx_t *sc)
{
	spinlock_ctx_t tsc;
	int flags, refs;

	/*
	 * Handle dying thread. Syscall return normally handles the exit, but
	 * this frame may never return there.
	 */
	if (recv->exit != 0) {
		hal_spinlockClear(&p->spinlock, sc);
		port_put(p, 0);
		proc_threadEnd();
	}

	hal_spinlockSet(&threads_spinlock, &tsc);

	(void)__atomic_fetch_and(&recv->flags, ~(int)(IPC_PULSED | IPC_DELIVERED), __ATOMIC_RELAXED);

	/* Must run before scActive is cleared */
	_portReturnReceiver(p, recv);

	refs = port_release(p);
	LIB_ASSERT(refs > 0, "last ref?");
	(void)refs;

	_threads_setActiveSc(recv, NULL);

	/* tsc and sc are swapped intentionally, we need to enable interrupts */
	hal_spinlockClear(&p->spinlock, &tsc);
	hal_cpuReschedule(&threads_spinlock, sc);

	/* WARN: unreachable if recv is woken in fastpath proc_send switch */

	flags = __atomic_fetch_and(&recv->flags, ~(int)(IPC_PULSED | IPC_DELIVERED), __ATOMIC_ACQUIRE);

	if ((flags & IPC_PULSED) != 0) {
		recv->ipc.msgPtr->o.pulse = recv->ipc.pulse;
		recv->ipc.msgPtr->o.err = EOK;
		return -EPULSE;
	}

	return ((flags & IPC_DELIVERED) != 0) ? EOK : -EINTR;
}


/* Consumes the port reference. Assumes the aspace of the calling thread. */
static int proc_recv_ex(port_t *p, msg_t *msg, msg_rid_t *rid, int rr)
{
	thread_t *recv = proc_current();
	spinlock_ctx_t sc;

	recv->ipc.ridPtr = rid;
	recv->ipc.msgPtr = msg;

	hal_spinlockSet(&p->spinlock, &sc);

	if (p->closed != 0) {
		hal_spinlockClear(&p->spinlock, &sc);
		port_put(p, 0);
		return -EINVAL;
	}

	if (p->pulsePending != 0U) {
		/* a pulse that arrived while nobody was waiting to take it */
		msg->o.pulse = p->pulse;
		msg->o.err = EOK;
		p->pulsePending = 0;

		hal_spinlockClear(&p->spinlock, &sc);
		port_put(p, 0);

		return -EPULSE;
	}

	recv->respondAndRecv = rr;

	return _recvWait(p, recv, &sc);
}


int proc_recv(u32 port, msg_t *msg, msg_rid_t *rid)
{
	port_t *p = proc_portGet(port);

	return (p == NULL) ? -EINVAL : proc_recv_ex(p, msg, rid, 0);
}


int proc_pulse(u32 port, u8 pulse)
{
	spinlock_ctx_t sc, tsc;
	thread_t *recv;
	port_t *p;

	p = proc_portGet(port);
	if (p == NULL) {
		return -EINVAL;
	}

	hal_spinlockSet(&p->spinlock, &sc);
	hal_spinlockSet(&threads_spinlock, &tsc);

	recv = _portWaitingReceiver(p);
	if (recv != NULL) {
		LIB_ASSERT(recv->state != READY, "how is recv ready while on port queue?");

		_wakeReceiver(recv);
		recv->ipc.pulse = pulse;
		(void)__atomic_fetch_or(&recv->flags, IPC_PULSED, __ATOMIC_RELEASE);
	}
	else {
		/* stick the pulse to the port for late receivers */
		p->pulse = pulse;
		p->pulsePending = 1;
	}

	hal_spinlockClear(&threads_spinlock, &tsc);

	if ((recv != NULL) && (recv->priority < proc_current()->priority)) {
		hal_cpuReschedule(&p->spinlock, &sc);
	}
	else {
		hal_spinlockClear(&p->spinlock, &sc);
	}

	port_put(p, 0);

	return EOK;
}


static int proc_respond_ex(port_t *p, msg_t *msg, msg_rid_t rid)
{
	spinlock_ctx_t sc, tsc;
	thread_t *caller, *recv;
	int err = EOK, dying = 0;

	hal_spinlockSet(&p->spinlock, &sc);
	if (p->closed != 0) {
		hal_spinlockClear(&p->spinlock, &sc);
		return -EINVAL;
	}

	hal_spinlockSet(&threads_spinlock, &tsc);

	recv = _proc_current();
	caller = _ridToReply(rid, recv);
	if (caller == NULL) {
		err = -EINVAL;
	}
	else {
		/*
		 * The SC stays with recv until the caller is ready: returned now, it
		 * would let a thread of middle priority preempt the copy below while the
		 * caller waits. Only recv can answer the rid, so the chain can wait too.
		 */
		dying = (caller->exit != 0U) ? 1 : 0;
	}

	hal_spinlockClear(&threads_spinlock, &tsc);
	hal_spinlockClear(&p->spinlock, &sc);

	if (err < 0) {
		return err;
	}

	/*
	 * Defer the copy to _threads_switchToThread(), which switches aspaces
	 * anyway (doing it here would cost two more pmap switches per respond).
	 *
	 * This makes the thread switch WCET unbounded, however (FIXME).
	 */
	if (dying != 0) {
		xfer_abort(caller);
	}
	else {
		hal_memcpy(caller->ipc.reply.raw, msg->o.raw, MSG_RAW_SIZE);
		caller->ipc.reply.err = msg->o.err;
		caller->ipc.reply.size = xfer_end(caller, msg->o.size);
		caller->ipc.call.replied = 1U;
	}

	hal_spinlockSet(&threads_spinlock, &sc);

	_proc_clearReplyChain(caller);
	_proc_setCallerMsgReturn(recv, caller, (dying != 0) ? -EPIPE : EOK);
	_threads_setCurrent(recv);

	LIB_ASSERT(recv->state == READY, "recv should be ready!");

	if (caller->priority < recv->priority) {
		hal_cpuReschedule(&threads_spinlock, &sc);
	}
	else {
		hal_spinlockClear(&threads_spinlock, &sc);
	}

	return (dying != 0) ? -EPIPE : EOK;
}


int proc_respond(u32 port, msg_t *msg, msg_rid_t rid)
{
	port_t *p = proc_portGet(port);
	int err;

	if (p == NULL) {
		return -EINVAL;
	}

	err = proc_respond_ex(p, msg, rid);
	port_put(p, 0);

	return err;
}


int proc_respondAndRecv(u32 port, msg_t *msg, msg_rid_t *rid)
{
	spinlock_ctx_t sc;
	msg_rid_t saved;
	int err, respond;
	port_t *p;

	p = proc_portGet(port);
	if (p == NULL) {
		return -EINVAL;
	}

	hal_spinlockSet(&threads_spinlock, &sc);
	saved = *rid;
	respond = (_ridToReply(saved, _proc_current()) != NULL) ? 1 : 0;
	hal_spinlockClear(&threads_spinlock, &sc);

	if (respond != 0) {
		err = proc_respond_ex(p, msg, saved);
		if ((err < 0) && (err != -EPIPE)) {
			port_put(p, 0);
			return err;
		}
	}

	return proc_recv_ex(p, msg, rid, 1);
}


void _msg_init(vm_map_t *kmap, vm_object_t *kernel)
{
	cycles_t cycles = 0;

	msg_common.kmap = kmap;

	/* FIXME: trivial to predict, implement good kernel entropy source */
	do {
		hal_cpuGetCycles(&cycles);
		msg_common.ridCookie = (ptr_t)cycles + 1;
	} while (msg_common.ridCookie == 0);
}
