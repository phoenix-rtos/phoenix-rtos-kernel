/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Thread manager
 *
 * Copyright 2012-2015, 2017, 2018, 2020 Phoenix Systems
 * Copyright 2001, 2005-2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk, Jacek Popko, Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/hal.h"
#include "include/errno.h"
#include "include/signal.h"
#include "threads.h"
#include "lib/lib.h"
#include "posix/posix.h"
#include "log/log.h"
#include "resource.h"
#include "msg.h"
#include "ports.h"
#include "perf/trace-events.h"
#include "perf/trace-ipc.h"

#include "syscalls.h"


#define THREAD_PULSED 1

/* clang-format off */
enum { event_scheduling, event_enqueued, event_waking, event_preempted };
/* clang-format on */

#define UNLOCK_DONT_YIELD 0
#define UNLOCK_DO_YIELD   1
#define UNLOCK_TRY        0
#define UNLOCK_FORCE      1


/* Maximum depth for transitive lock PI chain walking (scheduler-side) */
#define BWI_MAX_CHAIN_DEPTH 8

const struct lockAttr proc_lockAttrDefault = { .type = PH_LOCK_NORMAL };

/* Special empty queue value used to wakeup next enqueued thread. This is used to implement sticky conditions */
static thread_t *const wakeupPending = (void *)-1;

static struct {
	vm_map_t *kmap;
	spinlock_t spinlock;
	lock_t lock;
	sched_context_t *ready[MAX_PRIO];
	sched_context_t **current;
	time_t utcoffs;

	/* Synchronized by spinlock */
	rbtree_t sleeping;

	/* Synchronized by mutex */
	unsigned int idcounter;
	idtree_t id;

	intr_handler_t timeintrHandler;

#ifdef PENDSV_IRQ
	intr_handler_t pendsvHandler;
#endif

	sched_context_t *ghosts;
	thread_t *reaper;

	/* Debug */
	unsigned char stackCanary[16];
	time_t prev;

	/*
	 * Per-boot cookie XOR'd into msg_rid_t values written to userspace.
	 * Prevents the raw kernel thread_t* from being observable in user memory.
	 * Unmask with the same XOR before dereferencing the pointer in proc_respond.
	 */
	ptr_t ridCookie;
} threads_common;


_Static_assert(sizeof(threads_common.ready) / sizeof(threads_common.ready[0]) <= (u8)-1, "queue size must fit into priority type");


static thread_t *_proc_current(void);
static void _proc_threadDequeue(thread_t *t);
static int _proc_threadWait(thread_t **queue, time_t timeout, spinlock_ctx_t *scp);


static time_t _proc_gettimeRaw(void)
{
	time_t now = hal_timerGetUs();

	LIB_ASSERT(now >= threads_common.prev, "timer non-monotonicity detected (%llu < %llu)", now, threads_common.prev);

	threads_common.prev = now;

	return now;
}


static int threads_sleepcmp(rbnode_t *n1, rbnode_t *n2)
{
	thread_t *t1 = lib_treeof(thread_t, sleeplinkage, n1);
	thread_t *t2 = lib_treeof(thread_t, sleeplinkage, n2);

	/* parasoft-suppress-next-line MISRAC2012-DIR_4_1 "Variable pass to lib_treeof will not be NULL, so lib_treeof will not be NULL either" */
	if (t1->wakeup != t2->wakeup) {
		return (t1->wakeup > t2->wakeup) ? 1 : -1;
	}
	else {
		return (proc_getTid(t1) > proc_getTid(t2)) ? 1 : -1;
	}
}

/*
 * Thread monitoring
 */

static int _proc_threadWakeup(thread_t **queue);
static int _proc_threadBroadcast(thread_t **queue);


/* Note: always called with threads_common.spinlock set */
static void _threads_updateWaits(thread_t *t, int type)
{
	time_t now = 0, wait;

	now = _proc_gettimeRaw();

	LIB_ASSERT_ALWAYS(t->sc_active != NULL, "attempted to update unschedulable thread (type=%d)", type);

	if (type == event_waking || type == event_preempted) {
		t->sc_active->readyTime = now;
	}
	else if (type == event_scheduling) {
		wait = now - t->sc_active->readyTime;

		if (t->sc_active->maxWait < wait) {
			t->sc_active->maxWait = wait;
		}
	}
	else {
		/* No action required */
	}
}


static void _threads_scheduling(thread_t *t)
{
	_threads_updateWaits(t, event_scheduling);
	trace_eventThreadScheduling(proc_getTid(t));
}


static void _threads_preempted(thread_t *t)
{
	_threads_updateWaits(t, event_preempted);
	trace_eventThreadPreempted(proc_getTid(t));
}


static void _threads_enqueued(thread_t *t)
{
	_threads_updateWaits(t, event_enqueued);
	trace_eventThreadEnqueued(proc_getTid(t));
}


static void _threads_waking(thread_t *t)
{
	_threads_updateWaits(t, event_waking);
	trace_eventThreadWaking(proc_getTid(t));
}


/*
 * Time management
 */


static void _threads_updateWakeup(time_t now, thread_t *minimum)
{
	thread_t *t;
	time_t wakeup;

	if (minimum != NULL) {
		t = minimum;
	}
	else {
		t = lib_treeof(thread_t, sleeplinkage, lib_rbMinimum(threads_common.sleeping.root));
	}

	if (t != NULL) {
		if (now >= t->wakeup) {
			wakeup = 1;
		}
		else {
			wakeup = t->wakeup - now;
		}
	}
	else {
		wakeup = SYSTICK_INTERVAL;
	}

	if (wakeup > SYSTICK_INTERVAL + SYSTICK_INTERVAL / 8) {
		wakeup = SYSTICK_INTERVAL;
	}

	hal_timerSetWakeup((unsigned int)wakeup);
}


static int threads_timeintr(unsigned int n, cpu_context_t *context, void *arg)
{
	thread_t *t;
	time_t now;
	spinlock_ctx_t sc;

	/* parasoft-begin-suppress MISRAC2012-RULE_14_3 "hal_cpuGetID()'s return value might
	 * not be known at compile time for different architectures" */
	if (hal_cpuGetID() != 0U) {
		/* Invoke scheduler */
		return 1;
	}
	/* parasoft-end-suppress MISRAC2012-RULE_14_3 */

	hal_spinlockSet(&threads_common.spinlock, &sc);
	now = _proc_gettimeRaw();

	for (;;) {
		t = lib_treeof(thread_t, sleeplinkage, lib_rbMinimum(threads_common.sleeping.root));

		if (t == NULL || t->wakeup > now) {
			break;
		}

		_proc_threadDequeue(t);
		hal_cpuSetReturnValue(t->context, (void *)-ETIME);
	}

	_threads_updateWakeup(now, t);

	hal_spinlockClear(&threads_common.spinlock, &sc);

	/* Invoke scheduler */
	return 1;
}


/*
 * Threads management
 */


static void proc_lockForceUnlock(lock_t *lock, int doYield);
static void _proc_threadSetPriority(thread_t *thread, u8 priority);


static cpu_context_t *_getUserContext(thread_t *thread)
{
	if (thread->process != NULL) {
		// if (hal_cpuSupervisorMode(thread->context) == 0) {
		return (cpu_context_t *)((char *)thread->kstack + thread->kstacksz - sizeof(cpu_context_t));
	}
	else {
		return thread->context;
	}
}


static void _unbindFromAddedTo(thread_t *t)
{
	port_t *p;
	spinlock_ctx_t sc;

	if (t->addedTo != NULL) {
		p = t->addedTo;
		hal_spinlockSet(&p->spinlock, &sc);
		LIST_REMOVE_EX(&p->threads, t, tnext, tprev);
		/*
		 * TODO: clear refcount, but cant use port_put here as it potentially
		 * sets threads_common.spinlock...
		 */
		hal_spinlockClear(&p->spinlock, &sc);
		t->addedTo = NULL;
	}
}


static void _sc_return(thread_t *server, thread_t *caller, sched_context_t *sc);
static sched_context_t *_sc_ofDonor(thread_t *t, thread_t *donor);


void proc_freeUtcb(thread_t *t)
{
	if (t->utcb.w != NULL) {
		if (t->process != NULL) {
			vm_munmap(&t->process->map, t->utcb.w, t->utcb.size);
		}
		t->utcb.w = NULL;
	}
	if (t->utcb.kw != NULL) {
		vm_munmap(threads_common.kmap, t->utcb.kw, t->utcb.size);
		t->utcb.kw = NULL;
	}
	if (t->utcb.p != NULL) {
		page_t *p = t->utcb.p;
		page_t *next = p;
		while (p != NULL) {
			next = p->next;
			vm_pageFree(p);
			p = next;
		}
		t->utcb.p = NULL;
	}
}


static void _setCallerMsgReturn(thread_t *recv, thread_t *caller, int retval)
{
	sched_context_t *donated_sc = _sc_ofDonor(recv, caller);

	_sc_return(recv, caller, donated_sc);
	LIST_ADD(&threads_common.ready[caller->priority], caller->sc_active);

	if (caller->callReturnable == 0) {
		trace_eventSyscallExit(syscall_msgSend, proc_getTid(caller));
		caller->context = _getUserContext(caller);
		hal_cpuSetReturnValue(caller->context, (void *)(ptr_t)retval);

		/* REVISIT: is possible that caller will want to exit here? */
		LIB_ASSERT(caller->exit == 0, "HAPPENS caller wants to exit");
	}
	else {
		caller->callReturnable = 0;
	}

	LIB_ASSERT(recv->passive == 1, "recv not passive?");
	LIB_ASSERT(recv->sc_active != NULL, "recv sched null?");
	LIB_ASSERT(caller->state == READY, "caller should be ready!");
	LIB_ASSERT(recv->sc_active->t == recv, "badly linked sched context");
	LIB_ASSERT(recv->sc_active != donated_sc, "returning with donated SC that was already returned??");
}


static void thread_destroy(thread_t *thread)
{
	process_t *process;
	spinlock_ctx_t sc;
	thread_t *reply;

	trace_eventThreadEnd(thread);

	/* No need to protect thread->locks access with threads_common.spinlock */
	/* The destroyed thread is a ghost and no thread (except for the current one) can access it */
	while (thread->locks != NULL) {
		proc_lockForceUnlock(thread->locks, UNLOCK_DO_YIELD);
	}

	threads_releaseIpcBuffers(thread);

	/* REVISIT: guard with threads spinlock needed? called may hold a reference to us */
	hal_spinlockSet(&threads_common.spinlock, &sc);
	_unbindFromAddedTo(thread);

	if (thread->called != NULL) {
		LIB_ASSERT(thread->called->reply == thread, "thread->called->reply != thread");
		thread->called->reply = NULL;
		LIB_ASSERT(0, "happens c");
	}

	if (thread->sc_active != NULL) {
		if (thread->reply != NULL) {
			LIB_ASSERT(thread->reply != thread, "thread replies to itself????");
			reply = thread->reply;

			LIB_ASSERT(reply->sc_active == NULL, "reply has... sched?");
			reply->called = NULL;

			LIB_ASSERT(reply->exit == 0, "reply thread exiting?");
			LIB_ASSERT(thread->passive == 1, "thread not passive?");

			hal_spinlockClear(&threads_common.spinlock, &sc);

			/*
			 * Release reply buffers before waking the reply. Safe to
			 * be done without spinlock when done before _setCallerMsgReturn()
			 */
			threads_releaseIpcBuffers(reply);

			hal_spinlockSet(&threads_common.spinlock, &sc);

			_setCallerMsgReturn(thread, reply, -EINVAL);

			hal_spinlockClear(&threads_common.spinlock, &sc);
		}
		else {
			hal_spinlockClear(&threads_common.spinlock, &sc);
			vm_kfree(thread->sc_active);
		}
	}
	else {
		hal_spinlockClear(&threads_common.spinlock, &sc);
	}
	vm_kfree(thread->kstack);

	proc_freeUtcb(thread);

	process = thread->process;
	if (process != NULL) {
		hal_spinlockSet(&threads_common.spinlock, &sc);

		LIST_REMOVE_EX(&process->threads, thread, procnext, procprev);
		LIST_ADD_EX(&process->ghosts, thread, procnext, procprev);
		(void)_proc_threadBroadcast(&process->reaper);

		hal_spinlockClear(&threads_common.spinlock, &sc);
		(void)proc_put(process);
	}
	else {
		vm_kfree(thread);
	}
}


thread_t *threads_findThread(int tid)
{
	thread_t *t;

	(void)proc_lockSet(&threads_common.lock);
	t = lib_idtreeof(thread_t, idlinkage, lib_idtreeFind(&threads_common.id, tid));
	if (t != NULL) {
		++t->refs;
	}
	(void)proc_lockClear(&threads_common.lock);

	return t;
}


void threads_put(thread_t *thread)
{
	int refs;

	(void)proc_lockSet(&threads_common.lock);
	refs = --thread->refs;
	if (refs <= 0) {
		lib_idtreeRemove(&threads_common.id, &thread->idlinkage);
	}
	(void)proc_lockClear(&threads_common.lock);

	if (refs <= 0) {
		thread_destroy(thread);
	}
}


static void _threads_cpuTimeCalc(thread_t *current, thread_t *selected)
{
	time_t now = _proc_gettimeRaw();

	if (current != NULL && current->sc_active != NULL) {
		current->sc_active->cpuTime += now - current->sc_active->lastTime;
		current->sc_active->lastTime = now;
	}

	if (selected != NULL && current != selected) {
		LIB_ASSERT(selected->sc_active != NULL, "selected thread is unschedulable?");
		selected->sc_active->lastTime = now;
	}
}


__attribute__((noreturn)) void proc_longjmp(cpu_context_t *ctx)
{
	spinlock_ctx_t sc;
	thread_t *current;

	hal_spinlockSet(&threads_common.spinlock, &sc);
	current = _proc_current();
	current->longjmpctx = ctx;
	(void)hal_cpuReschedule(&threads_common.spinlock, &sc);
	for (;;) {
	}
}


static int _threads_checkSignal(thread_t *selected, process_t *proc, cpu_context_t *signalCtx, unsigned int oldmask, const int src);


/* TODO: this is slow, make this O(1) */
int threads_getHighestPrio(int maxPrio)
{
	int i, ret = maxPrio;
	spinlock_ctx_t sc;
	sched_context_t *sched;

	hal_spinlockSet(&threads_common.spinlock, &sc);
	for (i = 0; i < maxPrio;) {
		sched = threads_common.ready[i];
		if (sched == NULL) {
			i++;
			continue;
		}

		if (sched->t->state != READY) {
			LIST_REMOVE(&threads_common.ready[i], sched);
			continue;
		}

		ret = i;
		break;
	}
	hal_spinlockClear(&threads_common.spinlock, &sc);

	return ret;
}


void _threads_removeFromQueue(thread_t *t)
{
	lib_rbRemove(&threads_common.sleeping, &t->sleeplinkage);
}


/* TODO: crude API, fix */
void _threads_ipcBufferRelease(ipc_buf_layout_t *il)
{
	if (il->bp != NULL) {
		vm_pageFree(il->bp);
		vm_munmap(threads_common.kmap, il->bvaddr, SIZE_PAGE);
		il->bp = NULL;
	}

	if (il->eoffs != 0) {
		if (il->ep != NULL) {
			vm_pageFree(il->ep);
		}
		vm_munmap(threads_common.kmap, il->evaddr, SIZE_PAGE);
		il->eoffs = 0;
		il->ep = NULL;
	}

	if (il->w != NULL) {
		vm_munmap(il->map, il->w, il->size);
		il->w = NULL;
		il->size = 0;
		il->map = NULL;
	}
}


/* assuming aspace of `to` */
static void _threads_copyMsgBufResponse(thread_t *from, thread_t *to, msg_t *msg)
{
	if ((to->utcb.flags & IPC_OUT_FROM_RECV) != 0) {
		hal_memcpy(to->utcb.msg->o.data, from->utcb.kw, msg->o.size);
	}

	to->utcb.msg->o.size = msg->o.size;
	hal_memcpy(to->utcb.msg->o.raw, msg->o.raw, MSG_RAW_SIZE);
	to->utcb.msg->o.err = msg->o.err;

	/* TODO: handle pulse as well? */
}


static void _threads_switchToThread(cpu_context_t *context, thread_t *selected)
{
	process_t *proc;
	cpu_context_t *signalCtx, *selCtx;

	threads_common.current[hal_cpuGetID()] = selected->sc_active;
	_hal_cpuSetKernelStack(selected->kstack + selected->kstacksz);
	selCtx = selected->context;

	proc = selected->process;
	if ((proc != NULL) && (proc->pmapp != NULL)) {
		/* Switch address space */
		pmap_switch(proc->pmapp);

		/* Check for signals to handle */
		if ((hal_cpuSupervisorMode(selCtx) == 0) && (selected->longjmpctx == NULL)) {
			signalCtx = (void *)((char *)hal_cpuGetUserSP(selCtx) - sizeof(cpu_context_t));
			if (_threads_checkSignal(selected, proc, signalCtx, selected->sigmask, SIG_SRC_SCHED) == 0) {
				selCtx = signalCtx;
			}
		}
	}
	else {
		/* Protects against use after free of process' memory map in SMP environment. */
		pmap_switch(&threads_common.kmap->pmap);
	}

	if (selected->utcb.responseDeferredFrom != NULL) {
		_threads_copyMsgBufResponse(selected->utcb.responseDeferredFrom, selected, &selected->utcb.msgDeferred);
		selected->utcb.responseDeferredFrom = NULL;
	}

	if (selected->longjmpctx != NULL) {
		selCtx = selected->longjmpctx;
		selected->longjmpctx = NULL;
	}

	if ((void *)selected->tls.tls_base != NULL) {
		hal_cpuTlsSet(&selected->tls, selCtx);
	}

	_threads_scheduling(selected);
	hal_cpuRestore(context, selCtx);

#if defined(STACK_CANARY) || !defined(NDEBUG)
	if ((selected->execkstack == NULL) && (selected->context == selCtx)) {
		// LIB_ASSERT_ALWAYS((char *)selCtx > ((char *)selected->kstack + selected->kstacksz - 9 * selected->kstacksz / 10),
		// 		"pid: %d, tid: %d, kstack: 0x%p, context: 0x%p, kernel stack limit exceeded",
		// 		(selected->process != NULL) ? process_getPid(selected->process) : 0, proc_getTid(selected),
		// 		selected->kstack, selCtx);
	}

	LIB_ASSERT_ALWAYS((selected->process == NULL) || (selected->ustack == NULL) ||
					(hal_memcmp(selected->ustack, threads_common.stackCanary, sizeof(threads_common.stackCanary)) == 0),
			"pid: %d, tid: %d, path: %s, user stack corrupted",
			process_getPid(selected->process), proc_getTid(selected), selected->process->path);
#endif
}


static sched_context_t *_sched_current(void)
{
	return threads_common.current[hal_cpuGetID()];
}


void threads_setState(u8 state)
{
	spinlock_ctx_t sc;
	thread_t *current;

	hal_spinlockSet(&threads_common.spinlock, &sc);
	current = _proc_current();
	LIB_ASSERT_ALWAYS(current != NULL, "current thread null");
	current->state = state;
	hal_spinlockClear(&threads_common.spinlock, &sc);
}


static cpu_context_t *_threads_switchTo(thread_t *dest)
{
	process_t *proc;
	cpu_context_t *ctx;

	_hal_cpuSetKernelStack(dest->kstack + dest->kstacksz);

	proc = dest->process;
	if ((proc != NULL) && (proc->pmapp != NULL)) {
		/* Switch address space */
		pmap_switch(proc->pmapp);
	}
	else {
		/* Protects against use after free of process' memory map in SMP environment. */
		pmap_switch(&threads_common.kmap->pmap);
	}

	LIB_ASSERT(_proc_current() != NULL, "proc current null");

	ctx = _getUserContext(dest);

	if ((proc != NULL) && (proc->pmapp != NULL)) {
		if ((hal_cpuSupervisorMode(ctx) == 0) && (dest->longjmpctx == NULL)) {
#ifndef NDEBUG
			cpu_context_t *signalCtx = (void *)((char *)hal_cpuGetUserSP(ctx) - sizeof(cpu_context_t));
			LIB_ASSERT(_threads_checkSignal(dest, proc, signalCtx, dest->sigmask, SIG_SRC_SCHED) != 0, "oho");
#endif
		}
	}

	if ((void *)dest->tls.tls_base != NULL) {
		hal_cpuTlsSet(&dest->tls, ctx);
	}

	LIB_ASSERT(dest->exit == 0, "switching to exiting thread");
	LIB_ASSERT(dest->sc_active != NULL, "dest shed is null");

	threads_common.current[hal_cpuGetID()] = dest->sc_active;

	_threads_scheduling(dest);

	return ctx;
}


static sched_context_t *_sc_best(thread_t *t)
{
	/* TODO: optimize */
	sched_context_t *best = t->sc_own;
	sched_context_t *sc = t->sc_donated;

	if (sc != NULL) {
		do {
			if (sc->priority < best->priority) {
				best = sc;
			}
			sc = sc->dnext;
		} while (sc != t->sc_donated);
	}

	return best;
}


/* WARN: Assumes t is not on any ready queue */
static void _sc_updateEffPriority(thread_t *t)
{
	t->priority = t->sc_active->priority;
	t->priorityBase = t->sc_active->priorityBase;
}


static sched_context_t *_sc_ofDonor(thread_t *t, thread_t *donor)
{
	sched_context_t *sc = t->sc_donated;
	LIB_ASSERT(t->sc_donated != NULL, "sc_donated NULL?");

	if (sc != NULL) {
		do {
			if (sc->donor == donor) {
				return sc;
			}
			sc = sc->dnext;
		} while (sc != t->sc_donated);
	}

	LIB_ASSERT(0, "would return null SC");

	return NULL;
}


static void _sc_donate(thread_t *from, thread_t *to, sched_context_t *sc)
{
	// LIB_ASSERT(from->exit == 0, "got it...");

	LIB_ASSERT(sc != NULL, "what?");

	/* Remove SC from `from` */
	if (sc == from->sc_own) {
		/* own SC: mark as donated but keep sc_own pointer */
	}
	else {
		LIST_REMOVE_EX(&from->sc_donated, sc, dnext, dprev);
	}

	/*
	 * BWI: set the caller's effective priority onto the SC before donation. If the caller
	 * was mutex-PI-boosted, sc->priority still has the stale base value. Propagate the
	 * boost so the receiver runs at the correct effective priority.
	 */
	if (from->priority < sc->priority) {
		sc->priority = from->priority;
	}

	from->sc_active = NULL;
	from->state = BLOCKED_ON_REPLY;

	/* see FIXME from _proc_threadExit */
	// from->interruptible = 1;

	/* Add SC to `to` */
	sc->donor = from;
	sc->t = to;

	LIB_ASSERT(sc != to->sc_own, "EEEEE?");
	LIST_ADD_EX(&to->sc_donated, sc, dnext, dprev);

	/* Recalculate to's active SC */
	to->sc_active = _sc_best(to);
	_sc_updateEffPriority(to);

	LIB_ASSERT(to->sc_active->t == to && (to->sc_active->donor != NULL || to->sc_active->owner == to), "mismanaged SC");

	to->state = READY;

	/* TODO: could this be a part of SC? donor? */
	to->reply = from;
	from->called = to;
}


static void _sc_return(thread_t *server, thread_t *caller, sched_context_t *sc)
{
	LIB_ASSERT(sc->donor == caller, "returning SC donated by someone else");

	LIB_ASSERT(caller->called == NULL, "_sc_return but called not cleared?");

	// LIB_ASSERT(server->reply != NULL, "_sc_return but no reply?");

	LIB_ASSERT(server->sc_donated != NULL || server->sc_donated->dnext != NULL, "empty/corrupted donation queue?");

	/* Remove donated SC from server */
	LIST_REMOVE_EX(&server->sc_donated, sc, dnext, dprev);

	/* Return to caller */
	sc->t = caller;

	/* BWI: restore SC priority to its base (the mutex PI boost was temporary) */
	sc->priority = sc->priorityBase;

	if (caller->sc_own != sc) {
		/* caller is in a reply chain */
		LIST_ADD_EX(&caller->sc_donated, sc, dnext, dprev);

		LIB_ASSERT(caller->reply != NULL, "caller has a donated SC but is not replying to anyone?");

		/*
		 * FIXME: doesnt fix it. caller can have multiple clients and caller->reply will be
		 * overwritten by last one while the SC can come from any
		 * so for now the SCs can still end up mixed (sc->donor can point to a
		 * caller's client that wasn't the actual donor)
		 */
		sc->donor = caller->reply;
	}
	else {
		sc->donor = NULL;
	}

	caller->sc_active = sc; /* or re-evaluate _sc_best (TODO?) */
	caller->state = READY;

	/* once locks get unified with BWI, this assertion should work */
	// LIB_ASSERT(caller->priority == sc->priority, "TODO lock bwi")

	/* Recalculate server's active SC */
	server->sc_active = _sc_best(server);
	server->sc_active->t = server;
	_sc_updateEffPriority(server);

	/* If server has no more SCs (all clients responded), it goes passive */

	LIB_ASSERT(server->sc_active->donor != NULL || server->sc_active->owner == server, "mismanaged SC");

	/* TODO: remove passive for the sake of sc_active == NULL? */

	server->reply = NULL;
}

// BIG TODO: sched queues should use sc priority everywhere not thread's
// the thread's priority is supposed to be a quick lookup (maybe remove it
// first as its just an opt)


/* parasoft-suppress-next-line MISRAC2012-RULE_8_4 "Function is used externally within assembler code" */
int _threads_schedule(unsigned int n, cpu_context_t *context, void *arg)
{
	thread_t *current, *selected = NULL;
	sched_context_t *sched;
	unsigned int i;
	int cpuId = hal_cpuGetID();
	u32 tsc;

	(void)arg;
	(void)n;
	hal_lockScheduler();

	tsc = trace_eventSchedEnter(cpuId);

	current = _proc_current();
	threads_common.current[cpuId] = NULL;

	/* Save current thread context */
	if (current != NULL) {
		if (current->fastpathExitCtx == NULL) {
			current->context = context;
		}
		else {
			/*
			 * current would exit the kernel with fastpathed ctx, so use it instead
			 * this is an optimization - we could save the kernel context as
			 * current->context, but we know it is on the exiting path to switch to
			 * fastpathExitCtx
			 */
			current->context = current->fastpathExitCtx;
			current->fastpathExitCtx = NULL;

			/* see note in proc_send_ex */
			if (current->saveCtxInReply != 0) {
				LIB_ASSERT(current->reply != NULL, "reply null?");
				current->reply->context = context;
				current->saveCtxInReply = 0;
			}
		}

		// LIB_ASSERT(current->exit == 0 || current->state == READY, "exiting thread will get lost!");

		/* Move thread to the end of queue */
		if (current->state == READY || current->exit != 0) {
			// LIB_ASSERT(current->sc_active != NULL, "READY but unschedulable? tid: %d, pc=%p, ra=%p", proc_getTid(current), current->context->sepc, current->context->ra);

			LIST_ADD(&threads_common.ready[current->priority], current->sc_active);
			_threads_preempted(current);
		}
	}

	/* Get next thread */
	i = 0;
	while (i < MAX_PRIO) {
		sched = threads_common.ready[i];
		if (sched == NULL) {
			i++;
			continue;
		}

		LIB_ASSERT(sched->t != NULL, "dangling scheduling context");

		LIST_REMOVE(&threads_common.ready[i], sched);

		if (sched->t->state != READY) {
			/* BWI: Follow lock dependency chain to boost the ultimate lock holder.
			 * This offloads transitive PI cost to the scheduler (NOVA-style):
			 * lock contention is O(1), the scheduler walks the chain. */
			if (sched->t->waitingOn != NULL) {
				LIB_ASSERT(0, "happens?");
				thread_t *target = sched->t;
				unsigned int depth = 0;

				while (target->waitingOn != NULL && depth < BWI_MAX_CHAIN_DEPTH) {
					lock_t *lk = target->waitingOn;

					if (lk->owner == NULL) {
						break; /* lock was destroyed */
					}

					target = lk->owner;

					if (target == sched->t) {
						break; /* cycle (deadlock) */
					}

					depth++;
				}

				if (target != NULL && target != sched->t &&
						target->state == READY && i < target->priority) {
					_proc_threadSetPriority(target, i);
				}
			}

			LIB_ASSERT(sched->t->exit == 0, "what about this guy!");
			/* lazy update */
			continue;
		}

		LIB_ASSERT(sched->t->sc_active != NULL, "sched points to unschedulable thread");

		selected = sched->t;

		if (selected->exit == 0U) {
			break;
		}

		if ((hal_cpuSupervisorMode(selected->context) != 0) && (selected->exit < THREAD_END_NOW)) {
			break;
		}

		selected->state = GHOST;
		LIST_ADD(&threads_common.ghosts, sched);
		(void)_proc_threadWakeup(&threads_common.reaper);
	}

	LIB_ASSERT(selected != NULL, "no threads to schedule");

	if (selected != NULL) {
		_threads_switchToThread(context, selected);
	}

	/* Update CPU usage */
	_threads_cpuTimeCalc(current, selected);

	trace_eventSchedExit(cpuId, tsc);

	return EOK;
}


/* parasoft-suppress-next-line MISRAC2012-RULE_8_4 "Function is used externally within assembler code" */
int threads_schedule(unsigned int n, cpu_context_t *context, void *arg)
{
	spinlock_ctx_t sc;
	int ret;

	hal_spinlockSet(&threads_common.spinlock, &sc);
	ret = _threads_schedule(n, context, arg);
	hal_spinlockClear(&threads_common.spinlock, &sc);

	return ret;
}


static thread_t *_proc_current(void)
{
	sched_context_t *sched = _sched_current();
	return sched == NULL ? NULL : sched->t;
}


thread_t *proc_current(void)
{
	return _proc_current();
}


static int thread_alloc(thread_t *thread)
{
	int id;

	(void)proc_lockSet(&threads_common.lock);
	id = lib_idtreeAlloc(&threads_common.id, &thread->idlinkage, (int)threads_common.idcounter);
	if (id < 0) {
		/* Try from the start */
		threads_common.idcounter = 0;
		id = lib_idtreeAlloc(&threads_common.id, &thread->idlinkage, (int)threads_common.idcounter);
	}

	if (id >= 0) {
		if (threads_common.idcounter == MAX_TID) {
			threads_common.idcounter = 0U;
		}
		else {
			threads_common.idcounter++;
		}
	}
	(void)proc_lockClear(&threads_common.lock);

	return id;
}


void threads_canaryInit(thread_t *t, void *ustack)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&threads_common.spinlock, &sc);

	t->ustack = ustack;
	if (t->ustack != NULL) {
		hal_memcpy(t->ustack, threads_common.stackCanary, sizeof(threads_common.stackCanary));
	}

	hal_spinlockClear(&threads_common.spinlock, &sc);
}


int proc_threadCreate(process_t *process, startFn_t start, int *id, u8 priority, size_t kstacksz, void *stack, size_t stacksz, void *arg)
{
	thread_t *t;
	spinlock_ctx_t sc;
	int err;

	if (priority >= MAX_PRIO) {
		return -EINVAL;
	}

	t = vm_kmalloc(sizeof(thread_t));
	if (t == NULL) {
		return -ENOMEM;
	}

	t->kstacksz = kstacksz;
	t->kstack = vm_kmalloc(t->kstacksz);
	if (t->kstack == NULL) {
		vm_kfree(t);
		return -ENOMEM;
	}
	hal_memset(t->kstack, 0xba, t->kstacksz);

	t->state = READY;
	t->wakeup = 0;
	t->process = process;
	t->parentkstack = NULL;
	t->sigmask = t->sigpend = 0;
	t->refs = 1;
	t->interruptible = 0;
	t->passive = 0;
	t->exit = 0;
	t->execdata = NULL;
	t->wait = NULL;
	t->locks = NULL;
	t->waitingOn = NULL;
	t->longjmpctx = NULL;
	hal_memset(&t->utcb, 0, sizeof(t->utcb));

	t->fastpathExitCtx = NULL;
	t->callReturnable = 0;
	t->saveCtxInReply = 0;
	t->respondAndRecv = 0;

	t->priorityBase = priority;
	t->priority = priority;
	t->sc_own = vm_kmalloc(sizeof(sched_context_t));
	if (t->sc_own == NULL) {
		vm_kfree(t->kstack);
		vm_kfree(t);
		return -ENOMEM;
	}
	t->sc_own->cpuTime = 0;
	t->sc_own->maxWait = 0;
	t->sc_own->t = t;
	t->sc_own->next = NULL;
	t->sc_own->prev = NULL;
	proc_gettime(&t->sc_own->startTime, NULL);
	t->sc_own->lastTime = t->sc_own->startTime;
	t->sc_own->owner = t;
	t->sc_own->donor = NULL;
	t->sc_own->priority = priority;
	t->sc_own->priorityBase = priority;
	t->sc_active = t->sc_own;
	t->sc_donated = NULL;

	t->reply = NULL;
	t->called = NULL;
	t->addedTo = NULL;
	t->flags = 0;

	t->mappedTo = NULL;

	if (thread_alloc(t) < 0) {
		vm_kfree(t->sc_active);
		vm_kfree(t->kstack);
		vm_kfree(t);
		return -ENOMEM;
	}

	if (process != NULL && (process->tls.tdata_sz != 0U || process->tls.tbss_sz != 0U)) {
		err = process_tlsInit(&t->tls, &process->tls, process->mapp);
		if (err != EOK) {
			lib_idtreeRemove(&threads_common.id, &t->idlinkage);
			vm_kfree(t->sc_active);
			vm_kfree(t->kstack);
			vm_kfree(t);
			return err;
		}
	}
	else {
		t->tls.tls_base = 0;
		t->tls.tdata_sz = 0;
		t->tls.tbss_sz = 0;
		t->tls.tls_sz = 0;
		t->tls.arm_m_tls = 0;
	}

	if (id != NULL) {
		*id = proc_getTid(t);
	}

	/* Prepare initial stack */
	(void)hal_cpuCreateContext(&t->context, start, t->kstack, t->kstacksz, (stack == NULL) ? NULL : (unsigned char *)stack + stacksz, arg, &t->tls);
	threads_canaryInit(t, stack);

	if (process != NULL) {
		hal_cpuSetCtxGot(t->context, process->got);
		hal_spinlockSet(&threads_common.spinlock, &sc);

		LIST_ADD_EX(&process->threads, t, procnext, procprev);
	}
	else {
		hal_spinlockSet(&threads_common.spinlock, &sc);
	}

	trace_eventThreadCreate(t);

	/* Insert thread to scheduler queue */

	_threads_waking(t);
	LIST_ADD(&threads_common.ready[priority], t->sc_active);

	hal_spinlockClear(&threads_common.spinlock, &sc);

	return EOK;
}


static u8 _proc_lockGetPriority(lock_t *lock)
{
	u8 priority = MAX_PRIO;
	thread_t *thread = lock->queue;

	if (thread != NULL) {
		do {
			if (thread->priority < priority) {
				priority = thread->priority;
			}
			thread = thread->qnext;
		} while (thread != lock->queue);
	}

	return priority;
}


static u8 _proc_threadGetLockPriority(thread_t *thread)
{
	u8 ret, priority = MAX_PRIO;
	lock_t *lock = thread->locks;

	if (lock != NULL) {
		do {
			ret = _proc_lockGetPriority(lock);
			if (ret < priority) {
				priority = ret;
			}
			lock = lock->next;
		} while (lock != thread->locks);
	}

	return priority;
}


static u8 _proc_threadGetPriority(thread_t *thread)
{
	unsigned int lockPrio, scPrio;

	lockPrio = _proc_threadGetLockPriority(thread);
	scPrio = (thread->sc_active != NULL) ? thread->sc_active->priority : thread->priorityBase;

	return (lockPrio < scPrio) ? lockPrio : scPrio;
}


static void _proc_threadSetPriority(thread_t *thread, u8 priority)
{
	unsigned int i;

	/* Don't allow decreasing the priority below base level */
	if (priority > thread->priorityBase) {
		priority = thread->priorityBase;
	}

	if (thread->state == READY) {
		for (i = 0; i < hal_cpuGetCount(); i++) {
			if (threads_common.current[i] != NULL && thread == threads_common.current[i]->t) {
				break;
			}
		}

		if (i == hal_cpuGetCount()) {
			LIB_ASSERT(LIST_BELONGS(&threads_common.ready[thread->priority], thread->sc_active) != 0,
					"thread: 0x%p, tid: %d, priority: %d, is not on the ready list",
					thread, proc_getTid(thread), thread->priority);
			LIST_REMOVE(&threads_common.ready[thread->priority], thread->sc_active);
			LIST_ADD(&threads_common.ready[priority], thread->sc_active);
		}
	}

	thread->priority = priority;
	trace_eventThreadPriority(proc_getTid(thread), thread->priority);
}


int proc_threadPriority(int signedPriority)
{
	thread_t *current;
	spinlock_ctx_t sc;
	int ret, reschedule = 0;
	u8 priority;

	if (signedPriority < -1) {
		return -EINVAL;
	}

	if ((signedPriority >= 0) && ((size_t)signedPriority >= sizeof(threads_common.ready) / sizeof(threads_common.ready[0]))) {
		return -EINVAL;
	}

	priority = (u8)signedPriority;

	hal_spinlockSet(&threads_common.spinlock, &sc);

	current = _proc_current();

	/* NOTE: -1 is used to retrieve the current thread priority only */
	if (signedPriority >= 0) {
		if (priority < current->priority) {
			current->priority = priority;
		}
		else if (priority > current->priority) {
			/* Make sure that the inherited priority from the lock is not reduced */
			if ((current->locks == NULL) || (priority <= _proc_threadGetLockPriority(current))) {
				current->priority = priority;
				/* Trigger immediate rescheduling if the task has lowered its priority */
				reschedule = 1;
			}
		}
		else {
			/* No action required */
		}

		current->priorityBase = priority;

		ret = (int)current->priorityBase;

		if (current->sc_active == current->sc_own) {
			current->sc_active->priority = priority;
			current->sc_active->priorityBase = current->priorityBase;
		}
	}
	else {
		/* Query mode: return effective priority (reflects PI boost + SC donation) */
		ret = current->priority;
	}

	if (reschedule != 0) {
		(void)hal_cpuReschedule(&threads_common.spinlock, &sc);
	}
	else {
		(void)hal_spinlockClear(&threads_common.spinlock, &sc);
	}

	trace_eventThreadPriority(proc_getTid(current), current->priority);

	return ret;
}


static void _wakePassive(thread_t *t)
{
	LIB_ASSERT(t->passive == 1, "t is not passive!");

	if (t->sc_donated == NULL) {
		/* this is ours SC */
		t->passive = 0;
	}

	t->sc_active = _sc_best(t);

	_proc_threadDequeue(t);
}


static void _thread_interrupt(thread_t *t)
{
	if (t->passive == 1) {
		_wakePassive(t);
		_unbindFromAddedTo(t);
		t->utcb.msg->o.err = -EINTR;
	}
	else {
		LIB_ASSERT(t->sc_donated == NULL, "SC donated but we are not passive?");
		_proc_threadDequeue(t);
	}

	hal_cpuSetReturnValue(t->context, (void *)-EINTR);
}


__attribute__((noreturn)) void proc_threadEnd(void)
{
	thread_t *t;
	int cpu;
	spinlock_ctx_t sc;

	(void)hal_spinlockSet(&threads_common.spinlock, &sc);

	cpu = (int)hal_cpuGetID();
	t = threads_common.current[cpu]->t;
	threads_common.current[cpu] = NULL;
	t->state = GHOST;
	LIB_ASSERT(t->sc_active != NULL, "null sched? maybe ok but must be handled");
	LIST_ADD(&threads_common.ghosts, t->sc_active);
	(void)_proc_threadWakeup(&threads_common.reaper);

	(void)hal_cpuReschedule(&threads_common.spinlock, &sc);

	__builtin_unreachable();
}


static void _proc_threadExit(thread_t *t)
{
	t->exit = THREAD_END;
	if (t->interruptible != 0U) {
		_thread_interrupt(t);
	}

	/*
	 * FIXME: ok, so here it may happen that t->sc_active == NULL
	 * for a thread that has donated its SC via _sc_donate()
	 * but there is no easy fix for this
	 */
}


void proc_threadDestroy(thread_t *t)
{
	spinlock_ctx_t sc;
	if (t != NULL) {
		hal_spinlockSet(&threads_common.spinlock, &sc);
		_proc_threadExit(t);
		hal_spinlockClear(&threads_common.spinlock, &sc);
	}
}


void proc_threadsDestroy(thread_t **threads, const thread_t *except)
{
	thread_t *t;
	spinlock_ctx_t sc;

	hal_spinlockSet(&threads_common.spinlock, &sc);
	t = *threads;
	if (t != NULL) {
		do {
			if (t != except) {
				_proc_threadExit(t);
			}
			t = t->procnext;
		} while (t != *threads);
	}
	hal_spinlockClear(&threads_common.spinlock, &sc);
}


void proc_reap(void)
{
	sched_context_t *ghost;
	spinlock_ctx_t sc;

	hal_spinlockSet(&threads_common.spinlock, &sc);
	while (threads_common.ghosts == NULL) {
		(void)_proc_threadWait(&threads_common.reaper, 0, &sc);
	}
	ghost = threads_common.ghosts;
	LIST_REMOVE(&threads_common.ghosts, ghost);
	hal_spinlockClear(&threads_common.spinlock, &sc);

	threads_put(ghost->t);
}


void proc_changeMap(process_t *proc, vm_map_t *map, vm_map_t *imap, pmap_t *pmap)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&threads_common.spinlock, &sc);
	proc->mapp = map;
	proc->pmapp = pmap;
	proc->imapp = imap;
	hal_spinlockClear(&threads_common.spinlock, &sc);
}


/*
 * Sleeping and waiting
 */

static void _proc_threadDequeue(thread_t *t)
{
	unsigned int i;

	if (t->state == GHOST) {
		return;
	}

	LIB_ASSERT(t->sc_active != NULL, "dequeueing unschedulable thread! tid: %d", proc_getTid(t));

	_threads_waking(t);

	if (t->wait != NULL) {
		LIST_REMOVE_EX(t->wait, t, qnext, qprev);
	}

	if (t->wakeup != 0) {
		lib_rbRemove(&threads_common.sleeping, &t->sleeplinkage);
	}

	t->wakeup = 0;
	t->wait = NULL;
	t->state = READY;
	t->interruptible = 0;

	/* MOD */
	for (i = 0; i < hal_cpuGetCount(); i++) {
		if (threads_common.current[i] != NULL && t == threads_common.current[i]->t) {
			break;
		}
	}

	if (i == hal_cpuGetCount()) {
		LIST_ADD(&threads_common.ready[t->priority], t->sc_active);
	}
}


static void _proc_threadEnqueueThread(thread_t *t, thread_t **queue, time_t timeout, int interruptible)
{
	LIST_ADD_EX(queue, t, qnext, qprev);

	t->state = SLEEP;
	t->wakeup = 0;
	t->wait = queue;
	t->interruptible = interruptible;

	if (timeout) {
		t->wakeup = timeout;
		lib_rbInsert(&threads_common.sleeping, &t->sleeplinkage);
		_threads_updateWakeup(_proc_gettimeRaw(), NULL);
	}
}


static void _proc_threadEnqueue(thread_t **queue, time_t timeout, u8 interruptible)
{
	thread_t *current;

	if (*queue == wakeupPending) {
		(*queue) = NULL;
		return;
	}

	current = _proc_current();

	_proc_threadEnqueueThread(current, queue, timeout, interruptible);

	_threads_enqueued(current);
}


static int _proc_threadWait(thread_t **queue, time_t timeout, spinlock_ctx_t *scp)
{
	int err;

	_proc_threadEnqueue(queue, timeout, 0);

	if (*queue == NULL) {
		return EOK;
	}

	err = hal_cpuReschedule(&threads_common.spinlock, scp);
	(void)hal_spinlockSet(&threads_common.spinlock, scp);

	return err;
}


static int _proc_threadSleepAbs(time_t abs, time_t now, spinlock_ctx_t *sc)
{
	/* Handle usleep(0) (yield) */
	if (abs > now) {
		thread_t *current = _proc_current();

		current->state = SLEEP;
		current->wait = NULL;
		current->wakeup = abs;
		current->interruptible = 1;

		(void)lib_rbInsert(&threads_common.sleeping, &current->sleeplinkage);

		_threads_enqueued(current);
		_threads_updateWakeup(now, NULL);
	}

	return hal_cpuReschedule(&threads_common.spinlock, sc);
}


static int _proc_threadSleep(time_t us, time_t now, spinlock_ctx_t *sc)
{
	return _proc_threadSleepAbs(now + us, now, sc);
}


int proc_threadSleep(time_t us)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&threads_common.spinlock, &sc);
	return _proc_threadSleep(us, _proc_gettimeRaw(), &sc);
}


int proc_threadNanoSleep(time_t *sec, long int *nsec, int absolute)
{
	time_t us, start, stop, elapsed, unslept;
	int err;
	spinlock_ctx_t sc;

	if ((*sec < 0) || ((*nsec) < 0) || ((*nsec) >= (1000 * 1000 * 1000))) {
		return -EINVAL;
	}

	us = ((*sec) * 1000LL * 1000LL) + (((time_t)(*nsec) + 999LL) / 1000LL);

	hal_spinlockSet(&threads_common.spinlock, &sc);

	start = _proc_gettimeRaw();

	if (absolute != 0) {
		err = _proc_threadSleepAbs(us, start, &sc);
	}
	else {
		err = _proc_threadSleep(us, start, &sc);
		if (err == -EINTR) {
			proc_gettime(&stop, NULL);
			elapsed = stop - start;
			if (us > elapsed) {
				unslept = us - elapsed;
				*sec = unslept / (1000 * 1000);
				*nsec = (long int)(unslept % (1000 * 1000)) * 1000;
			}
			else {
				*sec = 0;
				*nsec = 0;
			}
		}
	}

	return (err == -ETIME) ? EOK : err;
}


static int proc_threadWaitEx(thread_t **queue, spinlock_t *spinlock, time_t timeout, u8 interruptible, spinlock_ctx_t *scp)
{
	int err;
	spinlock_ctx_t tsc;

	hal_spinlockSet(&threads_common.spinlock, &tsc);

	if ((interruptible != 0U) && (_proc_current()->exit != 0U)) {
		/* Waiting in this state can lead to becoming a hanging zombie */
		hal_spinlockClear(&threads_common.spinlock, &tsc);
		return -EINTR;
	}

	_proc_threadEnqueue(queue, timeout, interruptible);

	if (*queue == NULL) {
		hal_spinlockClear(&threads_common.spinlock, &tsc);
		return EOK;
	}

	/* tsc and scp are swapped intentionally, we need to enable interrupts */
	hal_spinlockClear(spinlock, &tsc);
	err = hal_cpuReschedule(&threads_common.spinlock, scp);
	hal_spinlockSet(spinlock, scp);

	return err;
}


int proc_threadWait(thread_t **queue, spinlock_t *spinlock, time_t timeout, spinlock_ctx_t *scp)
{
	return proc_threadWaitEx(queue, spinlock, timeout, 0U, scp);
}


int proc_threadWaitInterruptible(thread_t **queue, spinlock_t *spinlock, time_t timeout, spinlock_ctx_t *scp)
{
	return proc_threadWaitEx(queue, spinlock, timeout, 1U, scp);
}


static int _proc_threadWakeup(thread_t **queue)
{
	int ret = 1;

	if ((*queue != NULL) && (*queue != wakeupPending)) {
		_proc_threadDequeue(*queue);
	}
	else {
		*queue = wakeupPending;
		ret = 0;
	}

	return ret;
}


int proc_threadWakeup(thread_t **queue)
{
	int ret = 0;
	spinlock_ctx_t sc;

	hal_spinlockSet(&threads_common.spinlock, &sc);
	ret = _proc_threadWakeup(queue);
	hal_spinlockClear(&threads_common.spinlock, &sc);
	return ret;
}


static int _proc_threadBroadcast(thread_t **queue)
{
	int ret = 0;

	do {
		ret += _proc_threadWakeup(queue);
	} while ((*queue != NULL) && (*queue != wakeupPending));

	return ret;
}


int proc_threadBroadcast(thread_t **queue)
{
	int ret = 0;
	spinlock_ctx_t sc;

	hal_spinlockSet(&threads_common.spinlock, &sc);
	ret = _proc_threadBroadcast(queue);
	hal_spinlockClear(&threads_common.spinlock, &sc);

	return ret;
}


void proc_threadWakeupYield(thread_t **queue)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&threads_common.spinlock, &sc);
	if (_proc_threadWakeup(queue) != 0) {
		(void)hal_cpuReschedule(&threads_common.spinlock, &sc);
	}
	else {
		hal_spinlockClear(&threads_common.spinlock, &sc);
	}
}


void proc_threadBroadcastYield(thread_t **queue)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&threads_common.spinlock, &sc);
	if (_proc_threadBroadcast(queue) != 0) {
		(void)hal_cpuReschedule(&threads_common.spinlock, &sc);
	}
	else {
		hal_spinlockClear(&threads_common.spinlock, &sc);
	}
}


int proc_join(int tid, time_t timeout)
{
	int err = EOK, found = 0, id = 0;
	thread_t *current;
	process_t *process;
	thread_t *ghost, *firstGhost;
	spinlock_ctx_t sc;
	time_t now, abstimeout;

	hal_spinlockSet(&threads_common.spinlock, &sc);

	now = _proc_gettimeRaw();
	current = _proc_current();
	if (proc_getTid(current) == tid) {
		hal_spinlockClear(&threads_common.spinlock, &sc);
		return -EDEADLK;
	}

	process = current->process;
	ghost = process->ghosts;
	firstGhost = process->ghosts;

	abstimeout = (timeout == 0) ? 0 : now + timeout;

	if (tid >= 0) {
		do {
			if (firstGhost != NULL) {
				do {
					if (proc_getTid(ghost) == tid) {
						found = 1;
						break;
					}
					else {
						ghost = ghost->procnext;
					}
				} while (ghost != NULL && ghost != firstGhost);
			}
			if (found == 1) {
				break;
			}
			else {
				err = _proc_threadWait(&process->reaper, abstimeout, &sc);
				firstGhost = process->ghosts;
				ghost = firstGhost;
			}
		} while (err != -ETIME && err != -EINTR);
	}
	else {
		/* compatibility with existing code */
		while (process->ghosts == NULL) {
			err = _proc_threadWait(&process->reaper, abstimeout, &sc);
			if (err == -EINTR || err == -ETIME) {
				break;
			}
		}
		ghost = process->ghosts;
	}

	if (ghost != NULL) {
		LIST_REMOVE_EX(&process->ghosts, ghost, procnext, procprev);
		id = proc_getTid(ghost);
	}
	hal_spinlockClear(&threads_common.spinlock, &sc);

	if ((ghost != NULL) && (ghost->tls.tls_sz != 0U)) {
		(void)process_tlsDestroy(&ghost->tls, process->mapp);
	}

	vm_kfree(ghost);
	return err < 0 ? err : id;
}


time_t proc_uptime(void)
{
	time_t time;

	proc_gettime(&time, NULL);

	return time;
}


void proc_gettime(time_t *raw, time_t *offs)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&threads_common.spinlock, &sc);
	if (raw != NULL) {
		(*raw) = _proc_gettimeRaw();
	}
	if (offs != NULL) {
		(*offs) = threads_common.utcoffs;
	}
	hal_spinlockClear(&threads_common.spinlock, &sc);
}


int proc_settime(time_t offs)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&threads_common.spinlock, &sc);
	threads_common.utcoffs = offs;
	hal_spinlockClear(&threads_common.spinlock, &sc);

	return EOK;
}


static time_t _proc_nextWakeup(void)
{
	thread_t *thread;
	time_t wakeup = 0;
	time_t now;

	thread = lib_treeof(thread_t, sleeplinkage, lib_rbMinimum(threads_common.sleeping.root));
	if (thread != NULL) {
		now = _proc_gettimeRaw();
		if (now >= thread->wakeup) {
			wakeup = 0;
		}
		else {
			wakeup = thread->wakeup - now;
		}
	}

	return wakeup;
}


/*
 * Signals
 */


int threads_sigpost(process_t *process, thread_t *thread, int sig)
{
	u32 sigbit = (u32)1U << (unsigned int)sig;

	spinlock_ctx_t sc;

	switch (sig) {
		case signal_segv:
		/* parasoft-suppress-next-line MISRAC2012-RULE_16_1 MISRAC2012-RULE_16_3 "Intentional fall-through" */
		case signal_illegal:
			if (process->sighandler != NULL) {
				break;
			}

		/* Fall-through */
		case signal_kill:
			proc_kill(process);
			return EOK;

		case signal_cancel:
			proc_threadDestroy(thread);
			return EOK;

		case 0:
			return EOK;

		default:
			/* Handles any value of 'sig' not covered by the case labels. */
			break;
	}
	hal_spinlockSet(&threads_common.spinlock, &sc);

	if (thread != NULL) {
		thread->sigpend |= sigbit;
	}
	else {
		process->sigpend |= sigbit;
		thread = process->threads;

		if (thread != NULL) {
			do {
				if ((sigbit & ~thread->sigmask) != 0U) {
					if (thread->interruptible != 0U) {
						_thread_interrupt(thread);
					}

					break;
				}
				thread = thread->procnext;
			} while (thread != process->threads);
		}
		else {
			/* Case for process without any theads
			 * Might happen during small window between last
			 * thread destroy and process destroy. This process
			 * will end anyway, no point in delivering the signal */
			hal_spinlockClear(&threads_common.spinlock, &sc);
			return -ESRCH;
		}
	}

	(void)hal_cpuReschedule(&threads_common.spinlock, &sc);

	return EOK;
}


static int _threads_checkSignal(thread_t *selected, process_t *proc, cpu_context_t *signalCtx, unsigned int oldmask, const int src)
{
#ifndef KERNEL_SIGNALS_DISABLE
	LIB_ASSERT(proc != NULL, "proc is null");

	unsigned int sig;

	sig = (selected->sigpend | proc->sigpend) & ~selected->sigmask;
	if ((sig != 0U) && (proc->sighandler != NULL)) {
		sig = hal_cpuGetLastBit(sig);

		if (hal_cpuPushSignal(selected->kstack + selected->kstacksz, proc->sighandler, signalCtx, (int)sig, oldmask, src) == 0) {
			selected->sigpend &= ~(0x1U << sig);
			proc->sigpend &= ~(0x1U << sig);
			return 0;
		}
	}

#endif

	return -1;
}


void threads_setupUserReturn(void *retval, cpu_context_t *ctx)
{
	spinlock_ctx_t sc;
	cpu_context_t *signalCtx, *fpCtx;
	void *f;
	void *kstackTop;
	thread_t *thread;

	// hal_cpuDisableInterrupts();
	hal_spinlockSet(&threads_common.spinlock, &sc);
	thread = _proc_current();
	if (thread->fastpathExitCtx == NULL) {
		kstackTop = thread->kstack + thread->kstacksz;
		signalCtx = (void *)((char *)hal_cpuGetUserSP(ctx) - sizeof(*signalCtx));
		hal_cpuSetReturnValue(ctx, retval);

		if (_threads_checkSignal(thread, thread->process, signalCtx, thread->sigmask, SIG_SRC_SCALL) == 0) {
			/* parasoft-suppress-next-line MISRAC2012-RULE_11_1 "f is passed to function hal_jmp which need void * type" */
			f = thread->process->sighandler;
			hal_spinlockClear(&threads_common.spinlock, &sc);
			hal_jmp(f, kstackTop, hal_cpuGetUserSP(signalCtx), 0, NULL);
			/* no return */
		}
	}
	else {
		fpCtx = thread->fastpathExitCtx;
		thread->fastpathExitCtx = NULL;
		// hal_spinlockGetCtx(&sc);
		hal_spinlockClear(&threads_common.spinlock, &sc);
		/* FIXME: race with sched is possible here */
		hal_endSyscall(fpCtx, &sc);
	}

	hal_spinlockClear(&threads_common.spinlock, &sc);
}


int threads_sigsuspend(unsigned int mask)
{
	thread_t *thread;
	spinlock_ctx_t sc;
	cpu_context_t *ctx, *signalCtx;
	void *kstackTop, *f;
	unsigned int oldmask;


	/* changing sigmask and sleep shall be atomic - do it under lock (sigpost is done also under threads_common.spinlock) */
	hal_spinlockSet(&threads_common.spinlock, &sc);
	thread = _proc_current();

	/* setup syscall return value - sigsuspend always returns -EINTR */
	kstackTop = thread->kstack + thread->kstacksz;
	ctx = kstackTop - sizeof(*ctx);
	signalCtx = (void *)((char *)hal_cpuGetUserSP(ctx) - sizeof(*signalCtx));
	hal_cpuSetReturnValue(ctx, (void *)-EINTR);

	oldmask = thread->sigmask;
	thread->sigmask = mask;

	/* check for pending signals before sleep - with the new mask */
	if (_threads_checkSignal(thread, thread->process, signalCtx, oldmask, SIG_SRC_SCALL) == 0) {
		/* parasoft-suppress-next-line MISRAC2012-RULE_11_1 "f is passed to function hal_jmp which need void * type" */
		f = thread->process->sighandler;
		hal_spinlockClear(&threads_common.spinlock, &sc);
		hal_jmp(f, kstackTop, hal_cpuGetUserSP(signalCtx), 0, NULL);
		/* no return */
	}

	/* Sleep forever (atomic lock release), interruptible */
	thread_t *tqueue = NULL;
	_proc_threadEnqueue(&tqueue, 0, 1);
	(void)hal_cpuReschedule(&threads_common.spinlock, &sc);
	/* after wakeup */

	/* check for pending signals before restoring the old mask */
	hal_spinlockSet(&threads_common.spinlock, &sc);
	if (_threads_checkSignal(thread, thread->process, signalCtx, oldmask, SIG_SRC_SCALL) == 0) {
		/* parasoft-suppress-next-line MISRAC2012-RULE_11_1 "f is passed to function hal_jmp which need void * type" */
		f = thread->process->sighandler;
		hal_spinlockClear(&threads_common.spinlock, &sc);
		hal_jmp(f, kstackTop, hal_cpuGetUserSP(signalCtx), 0, NULL);
		/* no return */
	}

	/* interrupted by signal but no sighandler installed */
	thread->sigmask = oldmask;
	hal_spinlockClear(&threads_common.spinlock, &sc);

	/* sigsuspend always exits with -EINTR */
	return -EINTR;
}


/*
 * Locks
 */


/* Assumes `lock->spinlock` and `threads_common.spinlock` are set. */
static int _proc_lockTry(thread_t *current, lock_t *lock)
{
	if (lock->owner != NULL) {
		return -EBUSY;
	}

	LIST_ADD(&current->locks, lock);

	lock->owner = current;

	return EOK;
}


int proc_lockTry(lock_t *lock)
{
	thread_t *current;
	spinlock_ctx_t lsc;
	spinlock_ctx_t tcsc;
	int err;

	if (hal_started() == 0) {
		return -EINVAL;
	}

	hal_spinlockSet(&lock->spinlock, &lsc);
	hal_spinlockSet(&threads_common.spinlock, &tcsc);

	current = _proc_current();

	err = _proc_lockTry(current, lock);

	hal_spinlockClear(&threads_common.spinlock, &tcsc);
	hal_spinlockClear(&lock->spinlock, &lsc);

	return err;
}


static int _proc_lockSet(lock_t *lock, u8 interruptible, spinlock_ctx_t *scp)
{
	thread_t *current;
	spinlock_ctx_t sc;
	int ret = EOK, tid;

	hal_spinlockSet(&threads_common.spinlock, &sc);

	current = _proc_current();
	tid = proc_getTid(current);

	_trace_eventLockSetEnter(lock, tid);

	if ((lock->attr.type == PH_LOCK_ERRORCHECK) && (lock->owner == current)) {
		hal_spinlockClear(&threads_common.spinlock, &sc);
		ret = -EDEADLK;
		_trace_eventLockSetExit(lock, tid, ret);
		return ret;
	}

	if ((lock->attr.type == PH_LOCK_RECURSIVE) && (lock->owner == current)) {
		if (((int)lock->depth + 1) == 0) {
			ret = -EAGAIN;
		}
		else {
			lock->depth++;
			ret = EOK;
		}

		hal_spinlockClear(&threads_common.spinlock, &sc);
		_trace_eventLockSetExit(lock, tid, ret);
		return ret;
	}

	LIB_ASSERT(lock->owner != current, "lock: %s, pid: %d, tid: %d, deadlock on itself",
			lock->name, (current->process != NULL) ? process_getPid(current->process) : 0, proc_getTid(current));

	if (_proc_lockTry(current, lock) < 0) {
		/* Track lock dependency for transitive PI (scheduler follows this chain) */
		current->waitingOn = lock;

		/* Eagerly propagate priority through the lock dependency chain.
		 * Walks: current -> lock->owner -> owner->waitingOn->owner -> ...
		 * Each owner in the chain is boosted to current's effective priority.
		 * Bounded by BWI_MAX_CHAIN_DEPTH to prevent unbounded work under spinlock. */
		{
			thread_t *target = lock->owner;
			unsigned int prio = current->priority;
			unsigned int depth = 0;

			while (target != NULL && depth < BWI_MAX_CHAIN_DEPTH) {
				if (prio < target->priority) {
					_proc_threadSetPriority(target, prio);
				}

				if (target->waitingOn == NULL) {
					break;
				}

				if (target->waitingOn->owner == NULL ||
						target->waitingOn->owner == current) {
					break; /* broken chain or cycle */
				}

				target = target->waitingOn->owner;
				depth++;
			}
		}

		hal_spinlockClear(&threads_common.spinlock, &sc);

		do {
			/* _proc_lockUnlock will give us a lock by it's own */
			if (proc_threadWaitEx(&lock->queue, &lock->spinlock, 0, interruptible, scp) == -EINTR) {
				/* Can happen when thread_destroy is called on lock owner and current */
				if (lock->owner == NULL) {
					hal_spinlockSet(&threads_common.spinlock, &sc);
					current->waitingOn = NULL;
					hal_spinlockClear(&threads_common.spinlock, &sc);
					ret = -EINTR;
					_trace_eventLockSetExit(lock, tid, ret);
					return ret;
				}
				/* Don't return EINTR if we got lock anyway */
				if (lock->owner != current) {
					hal_spinlockSet(&threads_common.spinlock, &sc);

					current->waitingOn = NULL;

					/* Recalculate lock owner priority (it might have been inherited from the current thread) */
					_proc_threadSetPriority(lock->owner, _proc_threadGetPriority(lock->owner));

					hal_spinlockClear(&threads_common.spinlock, &sc);

					ret = -EINTR;
					_trace_eventLockSetExit(lock, tid, ret);
					return ret;
				}
			}
		} while (lock->owner != current);
	}
	else {
		hal_spinlockClear(&threads_common.spinlock, &sc);
	}

	lock->depth = 1;

	_trace_eventLockSetExit(lock, tid, ret);
	return ret;
}


int proc_lockSet(lock_t *lock)
{
	spinlock_ctx_t sc;
	int err;

	if (hal_started() == 0) {
		return -EINVAL;
	}

	hal_spinlockSet(&lock->spinlock, &sc);

	err = _proc_lockSet(lock, 0U, &sc);

	hal_spinlockClear(&lock->spinlock, &sc);

	return err;
}


int proc_lockSetInterruptible(lock_t *lock)
{
	spinlock_ctx_t sc;
	int err;

	if (hal_started() == 0) {
		return -EINVAL;
	}

	hal_spinlockSet(&lock->spinlock, &sc);

	err = _proc_lockSet(lock, 1U, &sc);

	hal_spinlockClear(&lock->spinlock, &sc);

	return err;
}


static int _proc_lockUnlock(lock_t *lock, int doForceUnlock)
{
	thread_t *owner = lock->owner, *current;
	spinlock_ctx_t sc;
	int ret = 0;
	u8 lockPriority;

	hal_spinlockSet(&threads_common.spinlock, &sc);

	current = _proc_current();

	_trace_eventLockClear(lock, proc_getTid(current));

	LIB_ASSERT(LIST_BELONGS(&owner->locks, lock) != 0, "lock: %s, owner pid: %d, owner tid: %d, lock is not on the list",
			lock->name, (owner->process != NULL) ? process_getPid(owner->process) : 0, proc_getTid(owner));

	if (doForceUnlock == UNLOCK_TRY) {
		if ((lock->attr.type == PH_LOCK_ERRORCHECK) || (lock->attr.type == PH_LOCK_RECURSIVE)) {
			if (lock->owner != current) {
				hal_spinlockClear(&threads_common.spinlock, &sc);
				return -EPERM;
			}
		}
	}

	if ((lock->attr.type == PH_LOCK_RECURSIVE) && (lock->depth > 0U)) {
		if (doForceUnlock == UNLOCK_TRY) {
			lock->depth--;
			if (lock->depth != 0U) {
				hal_spinlockClear(&threads_common.spinlock, &sc);
				return 0;
			}
		}
		else {
			lock->depth = 0U;
		}
	}

	LIST_REMOVE(&owner->locks, lock);
	if (lock->queue != NULL) {
		/* Transfer lock to the first waiter */
		lock->owner = lock->queue;
		lock->owner->waitingOn = NULL;

		/* Wake the new owner and add lock to its held-locks list */
		_proc_threadDequeue(lock->owner);
		LIST_ADD(&lock->owner->locks, lock);

		/* Recalculate new owner's effective priority from ALL held locks + SC.
		 * This handles transitive PI: if the new owner holds another lock with
		 * a high-priority waiter, it will be boosted accordingly. */
		lockPriority = _proc_threadGetPriority(lock->owner);
		if ((unsigned int)lockPriority < lock->owner->priority) {
			_proc_threadSetPriority(lock->owner, lockPriority);
		}

		ret = 1;
	}
	else {
		lock->owner = NULL;
	}

	/* Restore previous owner's priority from its remaining held locks + SC */
	_proc_threadSetPriority(owner, _proc_threadGetPriority(owner));

	LIB_ASSERT(current->priority <= current->priorityBase, "pid: %d, tid: %d, basePrio: %d, priority degraded (%d)",
			(current->process != NULL) ? process_getPid(current->process) : 0, proc_getTid(current), current->priorityBase,
			current->priority);

	hal_spinlockClear(&threads_common.spinlock, &sc);

	return ret;
}


static void proc_lockForceUnlock(lock_t *lock, int doYield)
{
	spinlock_ctx_t sc;
	int ret = 0;

	hal_spinlockSet(&lock->spinlock, &sc);
	if (lock->owner != NULL) {
		ret = _proc_lockUnlock(lock, UNLOCK_FORCE);
	}

	hal_spinlockClear(&lock->spinlock, &sc);
	if ((ret > 0) && (doYield != UNLOCK_DONT_YIELD)) {
		(void)hal_cpuReschedule(NULL, NULL);
	}

	LIB_ASSERT(ret >= 0, "lock: %s, force unlocking failed (%d)", lock->name, ret);
}


static int _proc_lockClear(lock_t *lock)
{
#ifndef NDEBUG
	thread_t *current = proc_current();

	LIB_ASSERT(lock->owner != NULL, "lock: %s, pid: %d, tid: %d, unlock on not locked lock",
			lock->name, (current->process != NULL) ? process_getPid(current->process) : 0, proc_getTid(current));

	LIB_ASSERT(lock->owner == current, "lock: %s, pid: %d, tid: %d, owner: %d, unlocking someone's else lock",
			lock->name, (current->process != NULL) ? process_getPid(current->process) : 0,
			proc_getTid(current), proc_getTid(lock->owner));
#endif

	if (lock->owner == NULL) {
		return -EPERM;
	}

	return _proc_lockUnlock(lock, UNLOCK_TRY);
}


int proc_lockClear(lock_t *lock)
{
	spinlock_ctx_t sc;
	int err;

	if (hal_started() == 0) {
		return -EINVAL;
	}

	(void)hal_spinlockSet(&lock->spinlock, &sc);

	err = _proc_lockClear(lock);
	if (err > 0) {
		hal_spinlockClear(&lock->spinlock, &sc);
		(void)hal_cpuReschedule(NULL, NULL);
		return EOK;
	}

	hal_spinlockClear(&lock->spinlock, &sc);

	return err;
}


int proc_lockSet2(lock_t *l1, lock_t *l2)
{
	int err;

	err = proc_lockSet(l1);
	if (err < 0) {
		return err;
	}

	while (proc_lockTry(l2) < 0) {
		(void)proc_lockClear(l1);
		err = proc_lockSet(l2);
		if (err < 0) {
			return err;
		}
		swap(l1, l2);
	}

	return EOK;
}


int proc_lockWait(thread_t **queue, lock_t *lock, time_t timeout)
{
	spinlock_ctx_t sc;
	int err;

	if (hal_started() == 0) {
		return -EINVAL;
	}

	hal_spinlockSet(&lock->spinlock, &sc);

	err = _proc_lockClear(lock);
	if (err >= 0) {
		err = proc_threadWaitEx(queue, &lock->spinlock, timeout, 1U, &sc);
		if (err != -EINTR) {
			(void)_proc_lockSet(lock, 0U, &sc);
		}
	}

	hal_spinlockClear(&lock->spinlock, &sc);

	return err;
}

int proc_lockDone(lock_t *lock)
{
	proc_lockForceUnlock(lock, UNLOCK_DONT_YIELD);
	hal_spinlockDestroy(&lock->spinlock);
	return EOK;
}


int proc_lockInit(lock_t *lock, const struct lockAttr *attr, const char *name)
{
	hal_spinlockCreate(&lock->spinlock, "lock.spinlock");
	lock->owner = NULL;
	lock->queue = NULL;
	lock->name = name;
	lock->epoch = -1;

	hal_memcpy(&lock->attr, attr, sizeof(struct lockAttr));

	return EOK;
}


int _proc_lockSetTraceEpoch(lock_t *lock, int epoch)
{
	int prev;

	prev = lock->epoch;
	lock->epoch = epoch;

	return prev;
}


/*
 * Initialization
 */


static void threads_idlethr(void *arg)
{
	time_t wakeup;
	spinlock_ctx_t sc;

	for (;;) {
		/* Scrub any potential kernel logs (wake up readers) */
		log_scrubTry();

		if (hal_cpuLowPowerAvail() != 0) {
			hal_spinlockSet(&threads_common.spinlock, &sc);
			wakeup = _proc_nextWakeup();

			if (wakeup > (2 * SYSTICK_INTERVAL)) {
				hal_cpuLowPower(wakeup, &threads_common.spinlock, &sc);
				continue;
			}
			hal_spinlockClear(&threads_common.spinlock, &sc);
		}
		hal_cpuHalt();
	}
}


void proc_threadsDump(u8 priority)
{
	sched_context_t *sched;
	spinlock_ctx_t sc;

	/* Strictly needed - no lock can be taken
	 * while threads_common.spinlock is being
	 * held! */
	log_disable();

	lib_printf("threads: ");
	hal_spinlockSet(&threads_common.spinlock, &sc);

	sched = threads_common.ready[priority];
	do {
		lib_printf("[%p] ", sched->t);

		if (sched == NULL) {
			break;
		}

		sched = sched->next;
	} while (sched != threads_common.ready[priority]);
	hal_spinlockClear(&threads_common.spinlock, &sc);

	lib_printf("\n");


	return;
}


int proc_threadsIter(int n, proc_threadsListCb_t cb, void *arg)
{
	int i = 0;
	thread_t *t;
	map_entry_t *entry;
	vm_map_t *map;
	time_t now;
	spinlock_ctx_t sc;
	threadinfo_t tinfo;

	(void)proc_lockSet(&threads_common.lock);

	t = lib_treeof(thread_t, idlinkage, lib_rbMinimum(threads_common.id.root));

	while (i < n && t != NULL) {
		if (t->process != NULL) {
			tinfo.pid = process_getPid(t->process);
			// tinfo.ppid = t->process->parent != NULL ? t->process->parent->id : 0;
			/* TODO: tinfo.ppid = t->process->parent != NULL ? t->process->parent->id : 0; */
			tinfo.ppid = 0;
		}
		else {
			tinfo.pid = 0;
			tinfo.ppid = 0;
		}

		hal_spinlockSet(&threads_common.spinlock, &sc);
		tinfo.tid = (unsigned int)proc_getTid(t);
		tinfo.priority = (int)t->priorityBase;
		tinfo.state = (int)t->state;

		now = _proc_gettimeRaw();
		if (t->sc_active != NULL && now != t->sc_active->startTime) {
			tinfo.load = (int)((t->sc_active->cpuTime * 1000) / (now - t->sc_active->startTime));
		}
		else {
			tinfo.priority = -1;
			tinfo.load = 0;
			tinfo.cpuTime = 0;
			tinfo.wait = 0;
		}
		hal_spinlockClear(&threads_common.spinlock, &sc);

		if (t->process != NULL) {
			map = t->process->mapp;
			process_getName(t->process, tinfo.name, sizeof(tinfo.name));
		}
		else {
			map = threads_common.kmap;
			hal_memcpy(tinfo.name, "[idle]", sizeof("[idle]"));
		}

		tinfo.vmem = 0;

#ifdef NOMMU
		if (t->process != NULL) {
			entry = t->process->entries;
			if (entry != NULL) {
				do {
					tinfo.vmem += (int)entry->size;
					entry = entry->next;
				} while (entry != t->process->entries);
			}
		}
		else
#endif
				if (map != NULL) {
			(void)proc_lockSet(&map->lock);
			entry = lib_treeof(map_entry_t, linkage, lib_rbMinimum(map->tree.root));

			while (entry != NULL) {
				tinfo.vmem += (int)entry->size;
				entry = lib_treeof(map_entry_t, linkage, lib_rbNext(&entry->linkage));
			}
			(void)proc_lockClear(&map->lock);
		}
		else {
			/* No action required */
		}

		cb(arg, i, &tinfo);

		++i;
		t = lib_idtreeof(thread_t, idlinkage, lib_idtreeNext(&t->idlinkage.linkage));
	}

	(void)proc_lockClear(&threads_common.lock);

	return i;
}


int proc_threadsOther(thread_t *t)
{
	int ret;
	spinlock_ctx_t sc;

	hal_spinlockSet(&threads_common.spinlock, &sc);
	/* Assumes t is not NULL and belongs to a process */
	ret = (t->procnext != t) ? 1 : 0;
	hal_spinlockClear(&threads_common.spinlock, &sc);

	return ret;
}


static void proc_threadsListCb(void *arg, int i, threadinfo_t *tinfo)
{
	threadinfo_t *tinfos = (threadinfo_t *)arg;
	hal_memcpy(tinfos + i, tinfo, sizeof(threadinfo_t));
}


int proc_threadsList(int n, threadinfo_t *info)
{
	return proc_threadsIter(n, proc_threadsListCb, info);
}


int _threads_init(vm_map_t *kmap, vm_object_t *kernel)
{
	unsigned int i;
	cycles_t cycles = 0;

	threads_common.kmap = kmap;
	threads_common.ghosts = NULL;
	threads_common.reaper = NULL;
	threads_common.utcoffs = 0;
	threads_common.idcounter = 0;
	threads_common.prev = 0;

	(void)proc_lockInit(&threads_common.lock, &proc_lockAttrDefault, "threads.common");

	for (i = 0U; i < sizeof(threads_common.stackCanary); ++i) {
		threads_common.stackCanary[i] = ((i & 1U) != 0U) ? 0xaaU : 0x55U;
	}

	/* FIXME: trivial to predict, implement good kernel entropy source */
	do {
		hal_cpuGetCycles(&cycles);
		threads_common.ridCookie = (ptr_t)cycles + 1;
	} while (threads_common.ridCookie == 0);

	/* Initiaizlie scheduler queue */
	for (i = 0; i < sizeof(threads_common.ready) / sizeof(thread_t *); i++) {
		threads_common.ready[i] = NULL;
	}

	lib_rbInit(&threads_common.sleeping, threads_sleepcmp, NULL);
	lib_idtreeInit(&threads_common.id);

	lib_printf("proc: Initializing thread scheduler, priorities=%d\n", sizeof(threads_common.ready) / sizeof(thread_t *));

	hal_spinlockCreate(&threads_common.spinlock, "threads.spinlock");

	/* Allocate and initialize current threads array */
	/* parasoft-suppress-next-line MISRAC2012-DIR_4_7 "return value of hal_cpuGetCount() is used, false positive" */
	threads_common.current = (sched_context_t **)vm_kmalloc(sizeof(sched_context_t *) * hal_cpuGetCount());
	if (threads_common.current == NULL) {
		return -ENOMEM;
	}

	/* Run idle thread on every cpu */
	for (i = 0; i < hal_cpuGetCount(); i++) {
		threads_common.current[i] = NULL;
		(void)proc_threadCreate(NULL, threads_idlethr, NULL, MAX_PRIO - 1, (size_t)SIZE_KSTACK, NULL, 0, NULL);
	}

	/* Install scheduler on clock interrupt */
#ifdef PENDSV_IRQ
	hal_memset(&threads_common.pendsvHandler, 0, sizeof(threads_common.pendsvHandler));
	threads_common.pendsvHandler.f = threads_schedule;
	threads_common.pendsvHandler.n = PENDSV_IRQ;
	(void)hal_interruptsSetHandler(&threads_common.pendsvHandler);
#endif

	hal_memset(&threads_common.timeintrHandler, 0, sizeof(threads_common.timeintrHandler));
	(void)hal_timerRegister(threads_timeintr, NULL, &threads_common.timeintrHandler);

	return EOK;
}


static inline int _mustSlowCall(port_t *p, thread_t *caller)
{
	/*
	 * TODO: can there be several receivers? (e.g., first bad, second ready)
	 * is the second branch even possible?
	 */

	/* No passive receiver available */
	if (p->threads == NULL) {
		return 1;
	}

	/* Receiver currently has its own SC (active server, not passive) */
	if (p->threads->sc_active != NULL) {
		return 1;
	}

	return 0;
}

#define VERBOSE 0

static int proc_setupSharedBuffer(thread_t *t, thread_t *recv, void *buf, size_t bufsz, ipc_buf_layout_t *il, void **rbuf);


static void _portEnqueue(port_t *p, thread_t *t)
{
	LIST_ADD_EX(&p->threads, t, tnext, tprev);
	t->addedTo = p;
}


static void _portDequeue(port_t *p, thread_t *t)
{
	LIB_ASSERT(t->addedTo == p, "thread not added to this port");
	LIST_REMOVE_EX(&p->threads, t, tnext, tprev);
	t->addedTo = NULL;
}


static void _threads_copyShadowPages(ipc_buf_layout_t *il, size_t size)
{
	if (il->bp != NULL) {
		hal_memcpy(il->bvaddr + il->boffs, il->w + il->boffs, min(SIZE_PAGE - il->boffs, size));
	}
	if (il->eoffs != 0) {
		size = min(size, il->size);
		hal_memcpy(il->evaddr, il->w + il->boffs + size - il->eoffs, il->eoffs);
	}
}


static void _threads_copyShadowBuffers(thread_t *from, thread_t *to, msg_t *msg)
{
	if ((to->utcb.flags & IPC_OUT_DATA_MAPPED) != 0) {
		if (msg->o.size > 0) {
			LIB_ASSERT(to->mappedTo == from, "hm, %p != %p", to->mappedTo, from);
			_threads_copyShadowPages(&to->utcb.oil, msg->o.size);
		}
	}
}


/*
 * BIG TODO: msg fields are validated in syscalls.c, but then re-read in send -
 * another thread could mess with us here
 */


static vm_map_t *_getMap(process_t *process)
{
	return (process == NULL || process->mapp == NULL) ? threads_common.kmap : process->mapp;
}


static int _borrowBuf(thread_t *from, thread_t *to)
{
	vm_map_t *dstmap = _getMap(to->process);

	u8 flags = MAP_NOINHERIT;
	u8 attr = PGHD_READ | PGHD_WRITE | PGHD_PRESENT | vm_flagsToAttr(flags);
	u8 prot = PROT_WRITE | PROT_READ;

	if (to->process != NULL) {
		attr |= PGHD_USER;
		prot |= PROT_USER;
	}

	/* TODO: find only for payload size not whole buf */
	/* TODO: this doesn't handle non-contigous >1page buffers */
	void *vaddr = vm_mapFind(dstmap, NULL, from->utcb.size, flags, prot);
	if (vaddr == NULL) {
		return -ENOMEM;
	}

	if (page_map(&dstmap->pmap, vaddr, from->utcb.p->addr, attr) < 0) {
		return -ENOMEM;
	}

	to->utcb.bw = vaddr;
	to->utcb.bsize = from->utcb.size;

	return EOK;
}

typedef enum {
	IPC_XFER_NONE = 0, /* nothing to transfer */
	IPC_XFER_EXTRA,    /* payload fits into the receiver's IPC buffer */
	IPC_XFER_BORROW,   /* payload lives inside the caller's IPC buffer, which the receiver can temporarily borrow via _borrowBuf() */
	IPC_XFER_MAP,      /* fallback - must create dedicated shared mapping via proc_setupSharedBuffer() */
} ipcXferKind_t;

typedef enum {
	IPC_SIDE_IN = 0,
	IPC_SIDE_OUT,
} ipcSide_t;

typedef struct {
	ipcXferKind_t kind;
	const void *data; /* original caller-supplied pointer, valid for IPC_XFER_MAP */
	size_t size;
	size_t ofs; /* IPC_XFER_BORROW: offset in caller->utcb.w; IPC_XFER_EXTRA: offset in recv->utcb.w */
} ipcXferPlan_t;


static ipcXferPlan_t _classifyIpcXfer(thread_t *caller, thread_t *recv, const void *data, size_t size, size_t extraUsed)
{
	ipcXferPlan_t plan = { IPC_XFER_NONE, data, size, 0 };
	void *w = caller->utcb.w;
	size_t wsize = caller->utcb.size;

	if (size == 0) {
		return plan;
	}

	if (size + extraUsed <= recv->utcb.size) {
		plan.kind = IPC_XFER_EXTRA;
		plan.ofs = extraUsed;
		return plan;
	}

	if (size <= wsize && data >= w && (const char *)data + size <= (const char *)w + wsize) {
		plan.kind = IPC_XFER_BORROW;
		plan.ofs = (const char *)data - (const char *)w;
		return plan;
	}

	plan.kind = IPC_XFER_MAP;
	return plan;
}


/* Assumes _borrowBuf() has already been called if either side's plan.kind is IPC_XFER_BORROW */
static int _setupIpcSide(thread_t *caller, thread_t *recv, const ipcXferPlan_t *plan, ipc_buf_layout_t *il, void **rbuf, ipcSide_t side)
{
	switch (plan->kind) {
		case IPC_XFER_MAP:
			/* TODO: permissions, incoming data doesnt need to be writable */
			if (proc_setupSharedBuffer(caller, recv, (void *)plan->data, plan->size, il, rbuf) < 0) {
				return -ENOMEM;
			}
			caller->utcb.flags |= (side == IPC_SIDE_IN) ? IPC_IN_DATA_MAPPED : IPC_OUT_DATA_MAPPED;
			break;

		case IPC_XFER_BORROW:
			*rbuf = caller->utcb.w + plan->ofs;
			break;

		case IPC_XFER_EXTRA:
			*rbuf = recv->utcb.w + plan->ofs;
			if (side == IPC_SIDE_OUT) {
				caller->utcb.flags |= IPC_OUT_FROM_RECV;
			}
			break;

		default:
			LIB_ASSERT(plan->size == 0, "EEE");
			break;
	}

	return EOK;
}


static int proc_send_ex(u32 port, msg_t *msg, int returnable)
{
	port_t *p;
	thread_t *caller, *recv;
	spinlock_ctx_t sc;
	int err;
	cpu_context_t *ctx;

	caller = proc_current();

#if PERF_IPC
	u64 tscs[TSCS_SIZE];
	hal_memset(tscs, 0, sizeof(tscs));
	size_t step = 0;
	u64 currTsc;
	u16 tid = proc_getTid(caller);
#endif

	TRACE_IPC_PROFILE_POINT(tid, &step, &currTsc, tscs);  // 0

	p = proc_portGet(port);
	if (p == NULL) {
		return -EINVAL;
	}

	TRACE_IPC_PROFILE_POINT(tid, &step, &currTsc, tscs);  // 1

	hal_spinlockSet(&p->spinlock, &sc);

	if (p->closed != 0) {
		hal_spinlockClear(&p->spinlock, &sc);
		port_put(p, 0);
		return -EINVAL;
	}

	while ((err = _mustSlowCall(p, caller)) != 0) {
		p->queue.nonempty |= (1u << caller->priority);
		err = proc_threadWaitInterruptible(&p->queue.pq[caller->priority], &p->spinlock, 0, &sc);
		if (p->closed != 0 || err < 0) {
			if (p->closed != 0) {
				err = -EINVAL;
			}
			hal_spinlockClear(&p->spinlock, &sc);
			port_put(p, 0);
			return err;
		}
	}

	TRACE_IPC_PROFILE_POINT(tid, &step, &currTsc, tscs);  // 2

	/* commit to IPC */

	recv = p->threads;
	LIB_ASSERT(recv != NULL, "recv is null");

	LIST_REMOVE_EX(&p->threads, recv, tnext, tprev);

	recv->interruptible = 0;

	hal_spinlockClear(&p->spinlock, &sc);

	recv->addedTo = NULL;

	if (returnable != 0) {
		caller->callReturnable = 1;
	}

	caller->utcb.msg = msg;
	caller->utcb.esize = msg->esize;

	size_t isize = 0, osize = 0;

	TRACE_IPC_PROFILE_POINT(tid, &step, &currTsc, tscs);  // 3

	hal_memcpy(caller->utcb.msgbuf, msg->i.raw, MSG_RAW_SIZE);

	isize = msg->i.size;

	ipcXferPlan_t inPlan = _classifyIpcXfer(caller, recv, msg->i.data, isize, 0);
	if (inPlan.kind == IPC_XFER_EXTRA) {
		/* small message: fits the predefined recv buffer */
		hal_memcpy(recv->utcb.kw, msg->i.data, isize);
	}

	TRACE_IPC_PROFILE_POINT(tid, &step, &currTsc, tscs);  // 4

	osize = msg->o.size;
	ipcXferPlan_t outPlan = _classifyIpcXfer(caller, recv, msg->o.data, osize, (inPlan.kind == IPC_XFER_EXTRA) ? isize : 0);

	TRACE_IPC_PROFILE_POINT(tid, &step, &currTsc, tscs);  // 5

	oid_t oid;
	int type;

	hal_memcpy(&oid, &msg->oid, sizeof(oid_t));
	type = msg->type;

	hal_spinlockSet(&threads_common.spinlock, &sc);
	_sc_donate(caller, recv, caller->sc_active);

	TRACE_IPC_PROFILE_POINT(tid, &step, &currTsc, tscs);  // 6
	ctx = _threads_switchTo(recv);
	TRACE_IPC_PROFILE_POINT(tid, &step, &currTsc, tscs);  // 7

	/*
	 * FIXME: make recv interruptible in some checkpoints (e.g. between i.data and
	 * o.data setup, add rollbacks)
	 */
	recv->interruptible = 0;

	LIB_ASSERT(_proc_current() == recv, "we are not recv?");
	LIB_ASSERT(_proc_current()->sc_active != NULL, "proc current unschedulable?");

	thread_t *prevMappedTo = NULL;

	if (inPlan.kind == IPC_XFER_MAP || outPlan.kind == IPC_XFER_MAP) {
		prevMappedTo = caller->mappedTo;
		caller->mappedTo = recv;
	}

	hal_spinlockClear(&threads_common.spinlock, &sc);

	TRACE_IPC_PROFILE_POINT(tid, &step, &currTsc, tscs);  // 8

	LIB_ASSERT(recv->refs > 0, "attempting to return to refs=0 rcv? port=%d caller tid=%d recv tid=%d refs: %d",
			p->linkage.id, proc_getTid(caller), proc_getTid(recv), recv->refs);

	port_put(p, 0);

	TRACE_IPC_PROFILE_POINT(tid, &step, &currTsc, tscs);  // 9

	LIB_ASSERT(recv->exit == 0, "recv exit=%d", recv->exit);
	LIB_ASSERT(recv->utcb.msg != NULL, "recv msg is null");

	/* message transfer */

	TRACE_IPC_PROFILE_POINT(tid, &step, &currTsc, tscs);  // 10

	hal_memcpy(recv->utcb.msg->i.raw, caller->utcb.msgbuf, MSG_RAW_SIZE);

	TRACE_IPC_PROFILE_POINT(tid, &step, &currTsc, tscs);  // 11
	if (prevMappedTo != NULL) {
		LIB_ASSERT_ALWAYS(0, "HAPPENS?");
	}
	TRACE_IPC_PROFILE_POINT(tid, &step, &currTsc, tscs);  // 12

	caller->utcb.flags = 0;

	if ((inPlan.kind == IPC_XFER_BORROW || outPlan.kind == IPC_XFER_BORROW) && _borrowBuf(caller, recv) != 0) {
		LIB_ASSERT(0, "enomem, todo");
		return -ENOMEM;
	}

	if (_setupIpcSide(caller, recv, &inPlan, &caller->utcb.iil, (void *)&recv->utcb.msg->i.data, IPC_SIDE_IN) < 0) {
		LIB_ASSERT(0, "enomem");
		return -ENOMEM;
	}

	if (_setupIpcSide(caller, recv, &outPlan, &caller->utcb.oil, &recv->utcb.msg->o.data, IPC_SIDE_OUT) < 0) {
		LIB_ASSERT(0, "enomem, todo");
		return -ENOMEM;
	}


	recv->utcb.msg->i.size = isize;
	recv->utcb.msg->o.size = osize;
	recv->utcb.msg->pid = (caller->process != NULL) ? process_getPid(caller->process) : 0;
	hal_memcpy(&recv->utcb.msg->oid, &oid, sizeof(oid_t));
	recv->utcb.msg->type = type;
	recv->utcb.msg->priority = caller->priority;          /* ??? */
	TRACE_IPC_PROFILE_POINT(tid, &step, &currTsc, tscs);  // 13

	/* msg transfer should be done by now */

	recv->interruptible = 0;

	*recv->utcb.ridPtr = (msg_rid_t)((ptr_t)caller ^ threads_common.ridCookie);
	hal_cpuSetReturnValue(ctx, EOK);

	hal_spinlockSet(&threads_common.spinlock, &sc);

	recv->fastpathExitCtx = ctx;

	LIB_ASSERT(_proc_current() == recv, "we should be recv here");
	LIB_ASSERT(recv->exit == 0, "recv wants to exit! TODO");

	trace_eventSyscallExit(recv->respondAndRecv ? syscall_msgRespondAndRecv : syscall_msgRecv, proc_getTid(recv));

	TRACE_IPC_PROFILE_EXIT_FUNC(tid, syscall_msgSend, &step, &currTsc, tscs);

	if (recv->process == NULL || returnable != 0) {
		/*
		 * tricky part: reschedule will cause the scheduler to save recv->fastpath as recv context,
		 * while the kernel context *at this moment* will be saved to the caller
		 * via current->reply->context = context (see _threads_schedule)
		 */
		recv->saveCtxInReply = 1;
		hal_cpuReschedule(&threads_common.spinlock, &sc);
		LIB_ASSERT(recv->saveCtxInReply == 0, "not saved?");
		LIB_ASSERT(recv->fastpathExitCtx == NULL, "not cleared?");
		LIB_ASSERT(caller->callReturnable == 0, "callReturnable not cleared?");
		LIB_ASSERT(_proc_current() != recv, "we should NOT be a receiver here");
		LIB_ASSERT(_proc_current() == caller, "we should be a caller here");
	}
	else {
		hal_spinlockClear(&threads_common.spinlock, &sc);
		/* exit the syscall and return to userspace as recv */
	}

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


int proc_forward(u32 port, msg_t *msg, msg_rid_t rid)
{
	port_t *p;
	thread_t *caller, *recv, *forward;
	spinlock_ctx_t sc, tsc;

	void *reply = (void *)((ptr_t)rid ^ threads_common.ridCookie);

	recv = proc_current();
	LIB_ASSERT(recv != NULL, "recv is null???");

	if (reply == NULL || pmap_belongs(&threads_common.kmap->pmap, reply) == 0) {
		return -EINVAL;
	}

	p = proc_portGet(port);
	if (p == NULL) {
		return -EINVAL;
	}

	hal_spinlockSet(&p->spinlock, &sc);

	if (p->closed != 0) {
		hal_spinlockClear(&p->spinlock, &sc);
		port_put(p, 0);
		return -EINVAL;
	}

	int err;
	while ((err = _mustSlowCall(p, recv)) != 0) {
		p->queue.nonempty |= (1u << recv->priority);
		err = proc_threadWaitInterruptible(&p->queue.pq[recv->priority], &p->spinlock, 0, &sc);
		if (p->closed != 0 || err < 0) {
			hal_spinlockClear(&p->spinlock, &sc);
			port_put(p, 0);
			return p->closed != 0 ? -EINVAL : err;
		}
	}

	hal_spinlockSet(&threads_common.spinlock, &tsc);
	caller = reply;

	if (caller->called != recv || caller->exit != 0) {
		LIB_ASSERT(0, "TODO: commodify the respond paths");
	}

	forward = p->threads;
	LIB_ASSERT(forward != NULL, "forward null");
	LIB_ASSERT(recv != NULL, "recv is null");

	if (_getMap(forward->process) != _getMap(recv->process)) {
		LIB_ASSERT(0, "he");
		hal_spinlockClear(&threads_common.spinlock, &tsc);
		hal_spinlockClear(&p->spinlock, &sc);
		port_put(p, 0);
		return -EINVAL;
	}

	LIST_REMOVE_EX(&p->threads, forward, tnext, tprev);

	/* TODO: use port dequeue? */
	forward->addedTo = NULL;

	recv->interruptible = 1;
	forward->interruptible = 0;
	hal_spinlockClear(&threads_common.spinlock, &tsc);
	hal_spinlockClear(&p->spinlock, &sc);

	/* same aspace, we can copy directly */
	hal_memcpy(forward->utcb.msg, msg, sizeof(*msg));

	*forward->utcb.ridPtr = rid;
	forward->fastpathExitCtx = _getUserContext(forward);
	trace_eventSyscallExit(forward->respondAndRecv ? syscall_msgRespondAndRecv : syscall_msgRecv, proc_getTid(forward));

	// forward->utcb.msglen = recv->utcb.msglen;

	hal_cpuSetReturnValue(forward->fastpathExitCtx, EOK);

	hal_memcpy(&forward->utcb.iil, &recv->utcb.iil, sizeof(recv->utcb.iil));
	hal_memcpy(&forward->utcb.oil, &recv->utcb.oil, sizeof(recv->utcb.oil));

	hal_spinlockSet(&threads_common.spinlock, &tsc);

	sched_context_t *donated_sc = _sc_ofDonor(recv, caller);

	caller->mappedTo = forward;

	/* TODO: optimize these */
	caller->called = NULL;
	_sc_return(recv, caller, donated_sc);
	_sc_donate(caller, forward, donated_sc);

	/* could have changed as part of _sc_return */
	threads_common.current[hal_cpuGetID()] = recv->sc_active;

	LIST_ADD(&threads_common.ready[forward->priority], forward->sc_active);

	hal_spinlockClear(&threads_common.spinlock, &tsc);

	/* TODO: potentially unnecessary */
	hal_memset(&recv->utcb.iil, 0, sizeof(recv->utcb.iil));
	hal_memset(&recv->utcb.oil, 0, sizeof(recv->utcb.oil));

	port_put(p, 0);

	return EOK;
}


static int _proc_threadWakeupPrio(prio_queue_t *queue)
{
	unsigned int prio;
	if (queue->nonempty == 0) {
		return 0;
	}

	prio = __builtin_ctz(queue->nonempty);
	if (_proc_threadWakeup(&queue->pq[prio]) != 0) {
		if (queue->pq[prio] == NULL) {
			queue->nonempty &= ~(1u << prio);
		}
		return 1;
	}

	/* slot drained */
	queue->nonempty &= ~(1u << prio);
	return 0;
}


int proc_threadBroadcastPrio(prio_queue_t *queue)
{
	int ret = 0;
	spinlock_ctx_t sc;
	size_t prio;

	hal_spinlockSet(&threads_common.spinlock, &sc);

	for (prio = 0; prio < MAX_PRIO; prio++) {
		ret += _proc_threadBroadcast(&queue->pq[prio]);
	}

	hal_spinlockClear(&threads_common.spinlock, &sc);

	return ret;
}


/* TODO: move this queue to lib */
void proc_threadPrioQueueInit(prio_queue_t *queue)
{
	size_t prio;
	for (prio = 0; prio < MAX_PRIO; prio++) {
		queue->pq[prio] = NULL;
	}
	queue->nonempty = 0;
}


static int _postPassiveWakeup(port_t *p, thread_t *recv)
{
	int err;

	if ((recv->flags & THREAD_PULSED) != 0) {
		recv->flags &= (~(int)THREAD_PULSED);
		recv->utcb.msg->o.pulse = recv->utcb.pulse;
		recv->utcb.msg->o.err = EOK;
		err = -EPULSE;
	}
	else if (recv->reply == NULL) {
		err = -EINTR;
	}
	else {
		*recv->utcb.ridPtr = (msg_rid_t)((ptr_t)recv->reply ^ threads_common.ridCookie);
		err = EOK;
	}

	return err;
}

static int _becomePassive(port_t *p, thread_t *recv, spinlock_ctx_t *sc)
{
	spinlock_ctx_t tsc;

	/*
	 * Handle recv exit - normally this is done at the end of syscall dispatch,
	 * but recv is potentially not returning there. If we don't handle it here,
	 * the thread may get lost - it's unschedulable and not on a rqeueue so it
	 * won't be marked as ghost by the scheduler.
	 */
	if (recv->exit != 0) {
		hal_spinlockClear(&p->spinlock, sc);
		proc_threadEnd();
	}

	hal_spinlockSet(&threads_common.spinlock, &tsc);
	recv->sc_active = NULL;
	recv->state = BLOCKED_ON_RECV;
	recv->passive = 1;
	recv->interruptible = 1;
	recv->flags &= (~(int)THREAD_PULSED);

	_portEnqueue(p, recv);
	(void)_proc_threadWakeupPrio(&p->queue);
	hal_spinlockClear(&threads_common.spinlock, &tsc);

	hal_spinlockClear(&p->spinlock, sc);
	port_put(p, 0);

	hal_cpuReschedule(NULL, NULL);

	/* WARN: won't be reached if recv is woken in fastpath proc_send switch */
	return _postPassiveWakeup(p, recv);
}


/* assumes aspace of recv */
int _returnWithPulse(thread_t *recv, port_t *p, spinlock_ctx_t *sc)
{
	recv->utcb.msg->o.pulse = p->pulse;
	recv->utcb.msg->o.err = EOK;
	p->pulse = 0;
	hal_spinlockClear(&p->spinlock, sc);
	return -EPULSE;
}


static vm_flags_t _getMapFlags(vm_map_t *map, void *data)
{
	if (pmap_belongs(&map->pmap, data) != 0) {
		return vm_mapFlags(map, data);
	}
	else {
		return vm_mapFlags(threads_common.kmap, data);
	}
}


/* make these as lib macros please... */
#define FLOOR(x) ((x) & ~(SIZE_PAGE - 1))
#define CEIL(x)  (((x) + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1))


int proc_recv_ex(port_t *p, msg_t *msg, msg_rid_t *rid, int rr)
{
	spinlock_ctx_t sc;
	thread_t *recv;

	recv = proc_current();
	recv->utcb.ridPtr = rid;
	recv->utcb.msg = msg;
	recv->utcb.esize = 0;  // msg->esize; FIXME: bad API, add explicit syscall to set global recv pool

	hal_spinlockSet(&p->spinlock, &sc);
	if (p->closed != 0) {
		hal_spinlockClear(&p->spinlock, &sc);
		port_put(p, 0);
		return -EINVAL;
	}

	if (p->pulse != 0) {
		return _returnWithPulse(recv, p, &sc);
	}

	recv->respondAndRecv = rr;

	return _becomePassive(p, recv, &sc);
}


int proc_recv(u32 port, msg_t *msg, msg_rid_t *rid)
{
	port_t *p = proc_portGet(port);

	if (p == NULL) {
		return -EINVAL;
	}

	return proc_recv_ex(p, msg, rid, 0);
}


int proc_pulse(u32 port, u8 pulse)
{
	port_t *p;
	spinlock_ctx_t sc;
	thread_t *recv;

	p = proc_portGet(port);
	if (p == NULL) {
		return -EINVAL;
	}

	hal_spinlockSet(&p->spinlock, &sc);
	recv = p->threads;

	if (recv != NULL) {
		LIB_ASSERT(recv->state != READY, "how is recv ready while on port queue?");

		_wakePassive(recv);
		_portDequeue(p, recv);

		recv->utcb.pulse = pulse;
		recv->flags |= THREAD_PULSED;

		if (recv->priority < proc_current()->priority) {
			hal_cpuReschedule(&p->spinlock, &sc);
		}
		else {
			hal_spinlockClear(&p->spinlock, &sc);
		}
	}
	else {
		/* stick the pulse to port for late receivers */
		p->pulse = pulse;
		hal_spinlockClear(&p->spinlock, &sc);
	}

	port_put(p, 0);

	return EOK;
}


static void releaseBorrowedBuf(thread_t *t)
{
	if (t->utcb.bw != NULL) {
		vm_munmap(_getMap(t->process), t->utcb.bw, t->utcb.bsize);
		t->utcb.bw = NULL;
		t->utcb.bsize = 0;
	}
}


static int proc_respond_ex(port_t *p, msg_t *msg, msg_rid_t rid)
{
	spinlock_ctx_t sc, tsc;
	thread_t *caller, *recv;
	int err = EOK;

	void *reply = (void *)((ptr_t)rid ^ threads_common.ridCookie);

	recv = proc_current();

	if (reply == NULL || pmap_belongs(&threads_common.kmap->pmap, reply) == 0) {
		return -EINVAL;
	}

	hal_spinlockSet(&p->spinlock, &sc);
	if (p->closed != 0) {
		hal_spinlockClear(&p->spinlock, &sc);
		return -EINVAL;
	}

	caller = reply;
	LIB_ASSERT(caller != NULL, "caller null!");

	hal_spinlockSet(&threads_common.spinlock, &tsc);

	do {
		if (caller->called != recv) {
			LIB_ASSERT(0, "unmatched reply for %p: response from %p, but reply called %p\n", reply, recv, caller->called);
			err = -EINVAL;
			break;
		}

		/* clear called already to prevent races on SMP */
		caller->called = NULL;

		if (caller->exit != 0) {
			/* caller is dying, don't respond */
			LIB_ASSERT(recv->passive == 1, "recv not passive?");

			sched_context_t *donated_sc = _sc_ofDonor(recv, caller);
			_sc_return(recv, caller, donated_sc);

			caller->state = GHOST;
			LIST_ADD(&threads_common.ghosts, caller->sc_active);
			_proc_threadWakeup(&threads_common.reaper);

			recv->reply = NULL;
			if (recv->sc_donated == NULL) {
				/* this is our SC */
				recv->passive = 0;
			}
			recv->sc_active = _sc_best(recv);
			_sc_updateEffPriority(recv);

			threads_common.current[hal_cpuGetID()] = recv->sc_active;

			LIB_ASSERT(recv->state == READY, "recv not ready?");
			LIB_ASSERT(recv->sc_active->t == recv, "badly linked sched context");

			err = -EINVAL;
			break;
		}
	} while (0);

	hal_spinlockClear(&threads_common.spinlock, &tsc);
	hal_spinlockClear(&p->spinlock, &sc);

	if (err < 0) {
		return err;
	}

	_threads_copyShadowBuffers(recv, caller, msg);

	/*
	 * OPTIMIZATION: defer the copy of the msg to _threads_switchToThread() where
	 * we switch aspaces anyways.
	 * We *could* do it here, but would need to switch to caller aspace and back.
	 * Delegation saves us two pmap switches (potential TLB flushes) per respond fastpath
	 */
	hal_memcpy(&caller->utcb.msgDeferred, msg, sizeof(*msg));
	caller->utcb.responseDeferredFrom = recv;

	/* TODO: lazy remapping */
	threads_releaseIpcBuffers(caller);

	releaseBorrowedBuf(recv);

	hal_spinlockSet(&threads_common.spinlock, &sc);

	_setCallerMsgReturn(recv, caller, EOK);

	threads_common.current[hal_cpuGetID()] = recv->sc_active;

	LIB_ASSERT(recv->state == READY, "recv should be ready!");

	/* REVISIT: should we reschedule if client has higher prio than the server? */
	hal_spinlockClear(&threads_common.spinlock, &sc);

	return EOK;
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
	int err;
	spinlock_ctx_t sc;
	int respond = 1;

	port_t *p = proc_portGet(port);
	if (p == NULL) {
		return -EINVAL;
	}

	/*
	 * Read rid and unmask once under the lock to prevent other thread to change *rid
	 * between validation here and respond below
	 */
	hal_spinlockSet(&threads_common.spinlock, &sc);
	msg_rid_t saved_rid = *rid;
	thread_t *reply = (thread_t *)((ptr_t)saved_rid ^ threads_common.ridCookie);
	if (reply == NULL || pmap_belongs(&threads_common.kmap->pmap, reply) == 0 || reply->called != _proc_current()) {
		respond = 0;
	}
	hal_spinlockClear(&threads_common.spinlock, &sc);

	if (respond != 0) {
		err = proc_respond_ex(p, msg, saved_rid);
		if (err < 0) {
			return err;
		}
	}

	return proc_recv_ex(p, msg, rid, 1);
}


static void *_mapBufferUnaligned(
		void *data, size_t size, process_t *from, process_t *to, ipc_buf_layout_t *il)
{
	void *w = NULL, *vaddr;
	u64 boffs, eoffs;
	unsigned int n = 0, i;
	page_t *nep = NULL, *nbp = NULL;
	vm_map_t *srcmap, *dstmap;
	int flags;
	addr_t bpa, pa, epa;

	if ((size == 0) || (data == NULL)) {
		return NULL;
	}

	unsigned int attr = PGHD_WRITE | PGHD_READ | PGHD_PRESENT | PGHD_USER;
	unsigned int prot = PROT_READ | PROT_WRITE | PROT_USER;

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

	srcmap = _getMap(from);
	dstmap = _getMap(to);

	if ((srcmap == dstmap) && (pmap_belongs(&dstmap->pmap, data) != 0)) {
		return data;
	}

	size_t sz = (((boffs != 0) ? 1 : 0) + ((eoffs != 0) ? 1 : 0) + n) * SIZE_PAGE;
	w = vm_mapFind(dstmap, NULL, sz, MAP_NOINHERIT, prot);
	il->w = w;
	if (w == NULL) {
		return NULL;
	}
	il->size = sz;
	il->map = dstmap;

	flags = _getMapFlags(srcmap, data);
	if (flags < 0) {
		return NULL;
	}

	attr |= vm_flagsToAttr(flags);

	if (boffs > 0) {
		il->boffs = boffs;
		bpa = pmap_resolve(&srcmap->pmap, data) & ~(SIZE_PAGE - 1);
		if (bpa == 0) {
			return NULL;
		}

		nbp = vm_pageAlloc(SIZE_PAGE, PAGE_OWNER_APP);
		il->bp = nbp;
		if (nbp == NULL) {
			return NULL;
		}

		vaddr = vm_mmap(threads_common.kmap, NULL, NULL, SIZE_PAGE, PROT_READ | PROT_WRITE, VM_OBJ_PHYSMEM, bpa, flags);
		il->bvaddr = vaddr;
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
		il->eoffs = eoffs;
		vaddr = (void *)FLOOR((ptr_t)data + size);
		epa = pmap_resolve(&srcmap->pmap, vaddr) & ~(SIZE_PAGE - 1);
		if (epa == 0) {
			return NULL;
		}

		if ((boffs == 0) || (eoffs >= boffs)) {
			nep = vm_pageAlloc(SIZE_PAGE, PAGE_OWNER_APP);
			il->ep = nep;
			if (nep == NULL) {
				return NULL;
			}
		}
		else {
			nep = nbp;
		}

		vaddr = vm_mmap(threads_common.kmap, NULL, NULL, SIZE_PAGE, PROT_READ | PROT_WRITE, VM_OBJ_PHYSMEM, epa, flags);
		il->evaddr = vaddr;
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


void threads_releaseIpcBuffers(thread_t *thread)
{
	thread_t *mappedTo;
	spinlock_ctx_t sc;

	hal_spinlockSet(&threads_common.spinlock, &sc);
	/*
	 * THOUGHT:
	 * mappedTo != NULL => thread is mapped to some external dstmap
	 * what if dstmap owner dies?
	 */
	mappedTo = thread->mappedTo;
	thread->mappedTo = NULL;
	hal_spinlockClear(&threads_common.spinlock, &sc);

	if (mappedTo != NULL) {
		_threads_ipcBufferRelease(&thread->utcb.iil);
		_threads_ipcBufferRelease(&thread->utcb.oil);
	}

	thread->utcb.flags = 0;
}

static int proc_setupSharedBuffer(thread_t *t, thread_t *recv, void *buf, size_t bufsz, ipc_buf_layout_t *il, void **rbuf)
{
	void *w;
	LIB_ASSERT(buf != NULL && bufsz > 0, "bad args");

	w = _mapBufferUnaligned(buf, bufsz, t->process, recv->process, il);
	if (w == NULL) {
		return -ENOMEM;
	}

	*rbuf = w;
	return EOK;
}


void *proc_setup(thread_t *t, size_t sz)
{
	void *vaddr = NULL, *kvaddr = NULL;
	vm_map_t *map;
	page_t *p;
	u8 prot, flags, attr;

	if ((sz & (SIZE_PAGE - 1)) != 0) {
		return NULL;
	}

	if (t->utcb.kw != NULL) {
		if (t->utcb.size == sz) {
			LIB_ASSERT(t->utcb.w != NULL, "");
			LIB_ASSERT(t->utcb.p != NULL, "");
			return t->utcb.w;
		}
		else {
			proc_freeUtcb(t);
		}
	}

	map = _getMap(t->process);

	prot = PROT_WRITE | PROT_READ;
	flags = MAP_NOINHERIT;
	attr = PGHD_READ | PGHD_WRITE | PGHD_PRESENT | vm_flagsToAttr(flags);

	/* map to kernel space */
	kvaddr = vm_mapFind(threads_common.kmap, NULL, sz, flags, prot);
	if (kvaddr == NULL) {
		proc_freeUtcb(t);
		return NULL;
	}
	t->utcb.kw = kvaddr;

	if (t->process != NULL) {
		/* map to current thread space */
		vaddr = vm_mapFind(map, NULL, sz, flags, prot | PROT_USER);
		if (vaddr == NULL) {
			proc_freeUtcb(t);
			return NULL;
		}
		t->utcb.w = vaddr;
	}
	else {
		/* this is a kernel thread, so t->utcb.w is already mapped to its space */
		t->utcb.w = t->utcb.kw;
	}

	size_t ofs;
	for (ofs = 0; ofs < sz; ofs += SIZE_PAGE) {
		p = vm_pageAlloc(SIZE_PAGE, PAGE_OWNER_APP);

		if (p == NULL) {
			proc_freeUtcb(t);
			return NULL;
		}

		p->next = t->utcb.p;
		t->utcb.p = p;

		if (page_map(&threads_common.kmap->pmap, kvaddr + ofs, p->addr, attr) < 0) {
			proc_freeUtcb(t);
			return NULL;
		}

		if (t->process != NULL && page_map(&map->pmap, vaddr + ofs, p->addr, attr | PGHD_USER) < 0) {
			proc_freeUtcb(t);
			return NULL;
		}
	}

	t->utcb.size = sz;

	return t->utcb.w;
}
