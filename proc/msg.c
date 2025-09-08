/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Messages
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

#include "perf/trace-events.h"
#include "syscalls.h"


#define FLOOR(x) ((x) & ~(SIZE_PAGE - 1))
#define CEIL(x)  (((x) + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1))


/* clang-format off */
enum { msg_rejected = -1, msg_waiting = 0, msg_received, msg_responded };
/* clang-format on */


struct {
	vm_map_t *kmap;
	vm_object_t *kernel;
} msg_common;


static void *msg_map(int dir, kmsg_t *kmsg, void *data, size_t size, process_t *from, process_t *to)
{
	void *w = NULL, *vaddr;
	u64 boffs, eoffs;
	unsigned int n = 0, i, attr, prot;
	page_t *nep = NULL, *nbp = NULL;
	vm_map_t *srcmap, *dstmap;
	struct _kmsg_layout_t *ml = dir ? &kmsg->o : &kmsg->i;
	int flags;
	addr_t bpa, pa, epa;

	if ((size == 0) || (data == NULL)) {
		return NULL;
	}

	attr = PGHD_READ | PGHD_PRESENT;
	prot = PROT_READ;

	if (dir != 0) {
		attr |= PGHD_WRITE;
		prot |= PROT_WRITE;
	}

	if (to != NULL) {
		attr |= PGHD_USER;
		prot |= PROT_USER;
	}

	boffs = (ptr_t)data & (SIZE_PAGE - 1);

	if (FLOOR((ptr_t)data + size) > CEIL((ptr_t)data)) {
		n = (FLOOR((ptr_t)data + size) - CEIL((ptr_t)data)) / SIZE_PAGE;
	}

	if ((boffs != 0) && (FLOOR((ptr_t)data) == FLOOR((ptr_t)data + size))) {
		/* Data is on one page only and will be copied by boffs handler */
		eoffs = 0;
	}
	else {
		eoffs = ((ptr_t)data + size) & (SIZE_PAGE - 1);
	}

	srcmap = (from == NULL) ? msg_common.kmap : from->mapp;
	dstmap = (to == NULL) ? msg_common.kmap : to->mapp;

	if ((srcmap == dstmap) && (pmap_belongs(&dstmap->pmap, data) != 0)) {
		return data;
	}

	w = vm_mapFind(dstmap, NULL, (((boffs != 0) ? 1 : 0) + ((eoffs != 0) ? 1 : 0) + n) * SIZE_PAGE, MAP_NOINHERIT, prot);
	ml->w = w;
	if (w == NULL) {
		return NULL;
	}

	if (pmap_belongs(&srcmap->pmap, data) != 0) {
		flags = vm_mapFlags(srcmap, data);
	}
	else {
		flags = vm_mapFlags(msg_common.kmap, data);
	}

	if (flags < 0) {
		return NULL;
	}

	attr |= vm_flagsToAttr(flags);

	if (boffs > 0) {
		ml->boffs = boffs;
		bpa = pmap_resolve(&srcmap->pmap, data) & ~(SIZE_PAGE - 1);

		nbp = vm_pageAlloc(SIZE_PAGE, PAGE_OWNER_APP);
		ml->bp = nbp;
		if (nbp == NULL) {
			return NULL;
		}

		vaddr = vm_mmap(msg_common.kmap, NULL, NULL, SIZE_PAGE, PROT_READ | PROT_WRITE, VM_OBJ_PHYSMEM, bpa, flags);
		ml->bvaddr = vaddr;
		if (vaddr == NULL) {
			return NULL;
		}

		/* Map new page into destination address space */
		if (page_map(&dstmap->pmap, w, nbp->addr, (attr | PGHD_WRITE) & ~PGHD_USER) < 0) {
			return NULL;
		}

		hal_memcpy(w + boffs, vaddr + boffs, min(size, SIZE_PAGE - boffs));

		if (page_map(&dstmap->pmap, w, nbp->addr, attr) < 0) {
			return NULL;
		}
	}

	/* Map pages */
	vaddr = (void *)CEIL((ptr_t)data);

	for (i = 0; i < n; i++, vaddr += SIZE_PAGE) {
		pa = pmap_resolve(&srcmap->pmap, vaddr) & ~(SIZE_PAGE - 1);
		if (page_map(&dstmap->pmap, w + (i + ((boffs != 0) ? 1 : 0)) * SIZE_PAGE, pa, attr) < 0) {
			return NULL;
		}
	}

	if (eoffs) {
		ml->eoffs = eoffs;
		vaddr = (void *)FLOOR((ptr_t)data + size);
		epa = pmap_resolve(&srcmap->pmap, vaddr) & ~(SIZE_PAGE - 1);

		if ((boffs == 0) || (eoffs >= boffs)) {
			nep = vm_pageAlloc(SIZE_PAGE, PAGE_OWNER_APP);
			ml->ep = nep;
			if (nep == NULL) {
				return NULL;
			}
		}
		else {
			nep = nbp;
		}

		vaddr = vm_mmap(msg_common.kmap, NULL, NULL, SIZE_PAGE, PROT_READ | PROT_WRITE, VM_OBJ_PHYSMEM, epa, flags);
		ml->evaddr = vaddr;
		if (vaddr == NULL) {
			return NULL;
		}

		/* Map new page into destination address space */
		if (page_map(&dstmap->pmap, w + (n + ((boffs != 0) ? 1 : 0)) * SIZE_PAGE, nep->addr, (attr | PGHD_WRITE) & ~PGHD_USER) < 0) {
			return NULL;
		}

		hal_memcpy(w + (n + ((boffs != 0) ? 1 : 0)) * SIZE_PAGE, vaddr, eoffs);

		if (page_map(&dstmap->pmap, w + (n + ((boffs != 0) ? 1 : 0)) * SIZE_PAGE, nep->addr, attr) < 0) {
			return NULL;
		}
	}

	return (w + boffs);
}


static void msg_release(kmsg_t *kmsg)
{
	process_t *process;
	vm_map_t *map;

	if (kmsg->i.bp != NULL) {
		vm_pageFree(kmsg->i.bp);
		vm_munmap(msg_common.kmap, kmsg->i.bvaddr, SIZE_PAGE);
		kmsg->i.bp = NULL;
	}

	if (kmsg->i.eoffs != 0) {
		if (kmsg->i.ep != NULL) {
			vm_pageFree(kmsg->i.ep);
		}
		vm_munmap(msg_common.kmap, kmsg->i.evaddr, SIZE_PAGE);
		kmsg->i.eoffs = 0;
		kmsg->i.ep = NULL;
	}

	process = proc_current()->process;
	if (process != NULL) {
		map = process->mapp;
	}
	else {
		map = msg_common.kmap;
	}

	if (kmsg->i.w != NULL) {
		vm_munmap(map, kmsg->i.w, CEIL((ptr_t)kmsg->msg.i.data + kmsg->msg.i.size) - FLOOR((ptr_t)kmsg->msg.i.data));
		kmsg->i.w = NULL;
	}

	if (kmsg->o.bp != NULL) {
		vm_pageFree(kmsg->o.bp);
		vm_munmap(msg_common.kmap, kmsg->o.bvaddr, SIZE_PAGE);
		kmsg->o.bp = NULL;
	}

	if (kmsg->o.eoffs) {
		if (kmsg->o.ep != NULL) {
			vm_pageFree(kmsg->o.ep);
		}
		vm_munmap(msg_common.kmap, kmsg->o.evaddr, SIZE_PAGE);
		kmsg->o.eoffs = 0;
		kmsg->o.ep = NULL;
	}

	if (kmsg->o.w != NULL) {
		vm_munmap(map, kmsg->o.w, CEIL((ptr_t)kmsg->msg.o.data + kmsg->msg.o.size) - FLOOR((ptr_t)kmsg->msg.o.data));
		kmsg->o.w = NULL;
	}
}


static void msg_ipack(kmsg_t *kmsg)
{
	size_t offset;

	if (kmsg->msg.i.data != NULL) {
		switch (kmsg->msg.type) {
			case mtOpen:
			case mtClose:
				offset = sizeof(kmsg->msg.i.openclose);
				break;

			case mtRead:
			case mtWrite:
			case mtTruncate:
				offset = sizeof(kmsg->msg.i.io);
				break;

			case mtCreate:
				offset = sizeof(kmsg->msg.i.create);
				break;

			case mtLookup:
			case mtDestroy:
			case mtGetAttrAll:
				offset = 0;
				break;

			case mtSetAttr:
			case mtGetAttr:
				offset = sizeof(kmsg->msg.i.attr);
				break;

			case mtLink:
			case mtUnlink:
				offset = sizeof(kmsg->msg.i.ln);
				break;

			case mtReaddir:
				offset = sizeof(kmsg->msg.i.readdir);
				break;

			case mtDevCtl:
			default:
				return;
		}

		if (kmsg->msg.i.size > (sizeof(kmsg->msg.i.raw) - offset)) {
			return;
		}

		hal_memcpy(kmsg->msg.i.raw + offset, kmsg->msg.i.data, kmsg->msg.i.size);
		kmsg->msg.i.data = kmsg->msg.i.raw + offset;
	}
}


static int msg_opack(kmsg_t *kmsg)
{
	size_t offset;

	if (kmsg->msg.o.data == NULL) {
		return 0;
	}

	switch (kmsg->msg.type) {
		case mtOpen:
		case mtClose:
		case mtRead:
		case mtWrite:
		case mtTruncate:
		case mtDestroy:
		case mtLink:
		case mtUnlink:
		case mtReaddir:
		case mtGetAttrAll:
			offset = 0;
			break;

		case mtCreate:
			offset = sizeof(kmsg->msg.o.create);
			break;

		case mtSetAttr:
		case mtGetAttr:
			offset = sizeof(kmsg->msg.o.attr);
			break;

		case mtLookup:
			offset = sizeof(kmsg->msg.o.lookup);
			break;

		case mtDevCtl:
		default:
			return 0;
	}

	if (kmsg->msg.o.size > (sizeof(kmsg->msg.o.raw) - offset)) {
		return 0;
	}

	kmsg->msg.o.data = kmsg->msg.o.raw + offset;

	return 1;
}


int proc_send(u32 port, msg_t *msg)
{
	port_t *p;
	int err = EOK;
	kmsg_t kmsg;
	thread_t *sender;
	spinlock_ctx_t sc;

	/* TODO - check if msg pointer belongs to user vm_map */
	if (msg == NULL) {
		return -EINVAL;
	}

	p = proc_portGet(port);
	if (p == NULL) {
		return -EINVAL;
	}

	sender = proc_current();

	hal_memcpy(&kmsg.msg, msg, sizeof(msg_t));
	kmsg.src = sender->process;
	kmsg.threads = NULL;
	kmsg.state = msg_waiting;

	kmsg.msg.pid = (sender->process != NULL) ? process_getPid(sender->process) : 0;
	kmsg.msg.priority = sender->sched->priority;

	msg_ipack(&kmsg);

	hal_spinlockSet(&p->spinlock, &sc);

	if (p->closed != 0) {
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

	if (err == EOK) {
		hal_memcpy(msg->o.raw, kmsg.msg.o.raw, sizeof(msg->o.raw));
		msg->o.err = kmsg.msg.o.err;

		/* If msg.o.data has been packed to msg.o.raw */
		if ((kmsg.msg.o.data >= (void *)kmsg.msg.o.raw) && (kmsg.msg.o.data < (void *)kmsg.msg.o.raw + sizeof(kmsg.msg.o.raw))) {
			hal_memcpy(msg->o.data, kmsg.msg.o.data, msg->o.size);
		}
	}

	return err;
}


// #define log_debug(fmt, ...) perf_traceEventsPrintf("(%d)%s: " fmt "\n", proc_getTid(proc_current()), __FUNCTION__, ##__VA_ARGS__)
// #define log_debug(fmt, ...) lib_printf("(%d)%s: " fmt "\n", proc_getTid(proc_current()), __FUNCTION__, ##__VA_ARGS__)
#define log_debug(fmt, ...)
#define log_err(fmt, ...)

// #define log_err(fmt, ...) lib_printf("(%d)%s: " fmt "\n", proc_getTid(proc_current()), __FUNCTION__, ##__VA_ARGS__)
// #define log_err(fmt, ...) do { thread_t *t = proc_current(); lib_printf("(%d,%d) %s: " fmt "\n", proc_getTid(t), t->sched->priority, __FUNCTION__, ##__VA_ARGS__); } while (0)


int proc_recv(u32 port, msg_t *msg, msg_rid_t *rid)
{
	port_t *p;
	kmsg_t *kmsg;
	int ipacked = 0, opacked = 0, err = EOK;
	spinlock_ctx_t sc;

	p = proc_portGet(port);
	if (p == NULL) {
		return -EINVAL;
	}

	hal_spinlockSet(&p->spinlock, &sc);

	p->slot.recvMsg = msg;

	while ((p->kmessages == NULL) && (p->closed == 0) && (err != -EINTR)) {
		err = proc_threadWaitInterruptible(&p->threads, &p->spinlock, 0, &sc);
	}

	kmsg = p->kmessages;

	if (p->closed) {
		/* Port is being removed */
		if (kmsg != NULL) {
			kmsg->state = msg_rejected;
			LIST_REMOVE(&p->kmessages, kmsg);
			proc_threadWakeup(&kmsg->threads);
		}

		err = -EINVAL;
	}
	else {
		if (err == EOK) {
			LIST_REMOVE(&p->kmessages, kmsg);
			kmsg->state = msg_received;
		}
	}
	hal_spinlockClear(&p->spinlock, &sc);

	if (err != EOK) {
		port_put(p, 0);
		return err;
	}

	kmsg->i.bvaddr = NULL;
	kmsg->i.boffs = 0;
	kmsg->i.w = NULL;
	kmsg->i.bp = NULL;
	kmsg->i.evaddr = NULL;
	kmsg->i.eoffs = 0;
	kmsg->i.ep = NULL;

	kmsg->o.bvaddr = NULL;
	kmsg->o.boffs = 0;
	kmsg->o.w = NULL;
	kmsg->o.bp = NULL;
	kmsg->o.evaddr = NULL;
	kmsg->o.eoffs = 0;
	kmsg->o.ep = NULL;

	if ((kmsg->msg.i.data >= (void *)kmsg->msg.i.raw) && (kmsg->msg.i.data < (void *)kmsg->msg.i.raw + sizeof(kmsg->msg.i.raw))) {
		ipacked = 1;
	}

	/* Map data in receiver space */
	/* Don't map if msg is packed */
	if (ipacked == 0) {
		kmsg->msg.i.data = msg_map(0, kmsg, (void *)kmsg->msg.i.data, kmsg->msg.i.size, kmsg->src, proc_current()->process);
	}

	opacked = msg_opack(kmsg);
	if (opacked == 0) {
		kmsg->msg.o.data = msg_map(1, kmsg, kmsg->msg.o.data, kmsg->msg.o.size, kmsg->src, proc_current()->process);
	}

	if (((kmsg->msg.i.size != 0) && (kmsg->msg.i.data == NULL)) ||
			((kmsg->msg.o.size != 0) && (kmsg->msg.o.data == NULL)) ||
			(proc_portRidAlloc(p, kmsg) < 0)) {
		msg_release(kmsg);

		hal_spinlockSet(&p->spinlock, &sc);
		kmsg->state = msg_rejected;
		proc_threadWakeup(&kmsg->threads);
		hal_spinlockClear(&p->spinlock, &sc);

		port_put(p, 0);

		return -ENOMEM;
	}

	*rid = lib_idtreeId(&kmsg->idlinkage);

	hal_memcpy(msg, &kmsg->msg, sizeof(*msg));

	if (ipacked != 0) {
		msg->i.data = msg->i.raw + (kmsg->msg.i.data - (void *)kmsg->msg.i.raw);
	}

	if (opacked != 0) {
		msg->o.data = msg->o.raw + (kmsg->msg.o.data - (void *)kmsg->msg.o.raw);
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

	p = proc_portGet(port);
	if (p == NULL) {
		return -EINVAL;
	}

	kmsg = proc_portRidGet(p, rid);
	if (kmsg == NULL) {
		port_put(p, 0);
		return -ENOENT;
	}

	/* Copy shadow pages */
	if (kmsg->i.bp != NULL) {
		hal_memcpy(kmsg->i.bvaddr + kmsg->i.boffs, kmsg->i.w + kmsg->i.boffs, min(SIZE_PAGE - kmsg->i.boffs, kmsg->msg.i.size));
	}

	if (kmsg->i.eoffs) {
		hal_memcpy(kmsg->i.evaddr, kmsg->i.w + kmsg->i.boffs + kmsg->msg.i.size - kmsg->i.eoffs, kmsg->i.eoffs);
	}

	if (kmsg->o.bp != NULL) {
		hal_memcpy(kmsg->o.bvaddr + kmsg->o.boffs, kmsg->o.w + kmsg->o.boffs, min(SIZE_PAGE - kmsg->o.boffs, kmsg->msg.o.size));
	}

	if (kmsg->o.eoffs) {
		hal_memcpy(kmsg->o.evaddr, kmsg->o.w + kmsg->o.boffs + kmsg->msg.o.size - kmsg->o.eoffs, kmsg->o.eoffs);
	}

	msg_release(kmsg);

	hal_memcpy(kmsg->msg.o.raw, msg->o.raw, sizeof(msg->o.raw));
	kmsg->msg.o.err = msg->o.err;

	hal_spinlockSet(&p->spinlock, &sc);
	kmsg->state = msg_responded;
	kmsg->src = proc_current()->process;
	proc_threadWakeup(&kmsg->threads);
	hal_spinlockClear(&p->spinlock, &sc);
	hal_cpuReschedule(NULL, NULL);

	port_put(p, 0);

	return s;
}


void *proc_configure(void)
{
	void *vaddr, *kvaddr;
	thread_t *t;
	vm_map_t *map;
	page_t *p;
	u8 prot, flags, attr;

	t = proc_current();
	map = &t->process->map;

	if (t->utcb.w != NULL) {
		return t->utcb.w;
	}

	prot = PROT_WRITE | PROT_READ | PROT_USER;
	flags = MAP_NOINHERIT;
	attr = PGHD_READ | PGHD_WRITE | PGHD_PRESENT | PGHD_USER | vm_flagsToAttr(flags);

	/* TODO: cleanups on exit */

	p = vm_pageAlloc(SIZE_PAGE, PAGE_OWNER_APP);
	if (p == NULL) {
		return NULL;
	}
	t->utcb.p = p;

	/* map to current thread space */
	vaddr = vm_mapFind(map, NULL, SIZE_PAGE, flags, prot);
	if (vaddr == NULL) {
		return NULL;
	}
	t->utcb.w = vaddr;

	if (page_map(&map->pmap, vaddr, p->addr, attr) < 0) {
		return NULL;
	}

	/* map to kernel space */
	kvaddr = vm_mapFind(msg_common.kmap, NULL, SIZE_PAGE, flags, prot);
	if (vaddr == NULL) {
		return NULL;
	}
	t->utcb.kw = kvaddr;

	if (page_map(&msg_common.kmap->pmap, kvaddr, p->addr, attr) < 0) {
		return NULL;
	}

	return vaddr;
}


#if 0
extern void interrupts_popContext(void);


__attribute__((noreturn)) static void _proc_call(port_t *p, thread_t *caller, spinlock_ctx_t *sc)
{
	hal_cpuDisableInterrupts();
	hal_lockScheduler();

	thread_t *recv = p->threads;

	LIB_ASSERT(recv != NULL, "recv is null");

	LIB_ASSERT(p->slot.caller == NULL, "there is already a caller?");

	p->slot.caller = caller;

	log_debug("utcb buf type %d", caller->utcb.kw->type);
	hal_memcpy(recv->utcb.kw, caller->utcb.kw, sizeof(*recv->utcb.kw));

	LIST_REMOVE_EX(&p->threads, recv, qnext, qprev);

	log_err("SWITCHING %p", recv);

	hal_spinlockClear(&p->spinlock, sc);
	// port_put(p, 0);

	cpu_context_t *ctx = threads_switchTo(recv, 0);

	log_err("SWITCHED");
	log_debug("recv->ctx=%p ctx->eip=%p\n", recv->context, recv->context->eip);

	asm volatile(
			"cli\n\t"
			"movl %0, %%esp\n\t"
			"jmp interrupts_popContext\n\t"
			:
			: "r"(ctx)
			: "memory");

	__builtin_unreachable();
}


static inline int _mustSlowCall(port_t *p, thread_t *caller)
{
	return p->fastpath != 0 || p->threads == NULL || p->slot.caller != NULL ||
			/* if recv is an active server, check priority */
			(p->threads->sched != NULL && caller->sched->priority < p->threads->sched->priority) ||
			threads_getHighestPrio(caller->sched->priority) != caller->sched->priority;
}


static int _proc_callSlow(port_t *p, thread_t *caller, spinlock_ctx_t *sc)
{
	int err;

	while (_mustSlowCall(p, caller) != 0) {
		log_err("to sleep p=%d", proc_getTid(proc_current()));

		err = proc_threadWaitInterruptible(&p->queue, &p->spinlock, 0, sc);
		if (err < 0) {
			hal_spinlockClear(&p->spinlock, sc);
			port_put(p, 0);
			LIB_ASSERT_ALWAYS(0, "FAIL");
			return err;
		}
		log_err("woke up");

		/* TODO: abort on server fault/port closure */
	}

	log_err("im going in");
	_proc_call(p, caller, sc);
}


int proc_call(u32 port, msg_t *msg)
{
	port_t *p;
	thread_t *caller;
	spinlock_ctx_t sc;

	if (msg == NULL) {
		return -EINVAL;
	}

	p = proc_portGet(port);
	if (p == NULL) {
		return -EINVAL;
	}

	caller = proc_current();

	if (caller->utcb.w == NULL) {
		return -EINVAL;
	}

	hal_spinlockSet(&p->spinlock, &sc);

	while (_mustSlowCall(p, caller) != 0) {
		log_err("slow (prio=%d, tid=%d)", caller->sched->priority, proc_getTid(caller));
		return _proc_callSlow(p, caller, &sc);
	}

	/* commit to fastpath - point of no return */

	log_err("fast (prio=%d, hprio=%d)", caller->sched->priority, threads_getHighestPrio(caller->sched->priority));
	_proc_call(p, caller, &sc);
}


int proc_respondAndRecv(u32 port, msg_t *msg, msg_rid_t *rid)
{
	port_t *p;
	spinlock_ctx_t sc;
	thread_t *caller;

	if (rid == NULL) {
		return -EINVAL;
	}

	p = proc_portGet(port);
	if (p == NULL) {
		return -EINVAL;
	}

	hal_spinlockSet(&p->spinlock, &sc);

	log_debug("utcb buf type %d", proc_current()->utcb.kw->type);

	thread_t *recv = proc_current();

	/* recv SC is actually caller's SC */
	/* TODO: this should probably be accounted in better way */
	if (threads_getHighestPrio(recv->sched->priority) != recv->sched->priority) {
		/* someone to respond but cannot be scheduled directly */
		log_err("slow: prio %d != %d", threads_getHighestPrio(recv->sched->priority), recv->sched->priority);

		LIB_ASSERT(0, "HAAAAAAAAAAA");

		if (p->slot.caller != NULL) {
			hal_memcpy(&p->slot.caller->utcb.kw->o, &recv->utcb.kw->o, sizeof(p->slot.caller->utcb.kw->o));
			p->slot.caller->state = READY;
			p->slot.caller = NULL;
		}

		hal_spinlockClear(&p->spinlock, &sc);
		// port_put(p, 0);
		hal_cpuReschedule(NULL, NULL);

		__builtin_unreachable();
		/* i think this point actually reachable ... */
	}


	if (p->slot.caller == NULL) {
		/* slowpath */
		log_err("passive");
		LIST_ADD_EX(&p->threads, recv, qnext, qprev);

		hal_spinlockClear(&p->spinlock, &sc);
		// port_put(p, 0);

		threads_becomePassive();
		__builtin_unreachable();
	}

	/* TODO: handle caller faults */
	LIB_ASSERT(p->slot.caller->exit == 0, "exit=%d", p->slot.caller->exit);
	LIB_ASSERT(p->slot.caller->state != GHOST, "huh");

	/* wake next caller if exists */
	if (p->queue != NULL) {
		log_err("wake next (prio=%d)", p->queue->sched->priority);
		proc_threadWakeup(&p->queue);
	}

	/* noone to reply, doing a fastpath and switching to caller thread */
	/* commit to fastpath - point of no return */

	hal_cpuDisableInterrupts();
	hal_lockScheduler();
	log_err("fast (%d, %d)", proc_getTid(recv), recv->sched->priority);

	LIST_ADD_EX(&p->threads, recv, qnext, qprev);

	caller = p->slot.caller;
	hal_memcpy(&caller->utcb.kw->o, &recv->utcb.kw->o, sizeof(caller->utcb.kw->o));
	p->slot.caller = NULL;

	p->fastpath = 0;

	hal_spinlockClear(&p->spinlock, &sc);
	// port_put(p, 0);

	/* 
	 * TODO: must to some sort of lock_kernel() like qnx to avoid preemptions here
	 * https://community.qnx.com/sf/wiki/do/viewHtml/projects.core_os/wiki/KernelSystemCall
	 * preemption may lead to two threads of same priority to do threads_switchTo
	 * at the same time
	 */

	cpu_context_t *ctx = threads_switchTo(caller, 1);

	asm volatile(
			"movl %0, %%esp\n\t"
			"jmp interrupts_popContext\n\t"
			:
			: "r"(ctx)
			: "memory");

	__builtin_unreachable();
}
#endif


void _msg_init(vm_map_t *kmap, vm_object_t *kernel)
{
	msg_common.kmap = kmap;
	msg_common.kernel = kernel;
}
