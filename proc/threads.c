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
 * SPDX-License-Identifier: BSD-3-Clause
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
#include "xfer.h"
#include "ports.h"
#include "perf/trace-events.h"
#include "perf/trace-msg.h"

#include "syscalls.h"


#define IPC_PULSED 1

/* clang-format off */
enum { event_scheduling, event_enqueued, event_waking, event_preempted };
/* clang-format on */

#define UNLOCK_DONT_YIELD 0
#define UNLOCK_DO_YIELD   1
#define UNLOCK_TRY        0
#define UNLOCK_FORCE      1

#define THREAD_WAIT_INTERRUPTIBLE (1U << 0)
#define THREAD_WAIT_EXCLUSIVE     (1U << 1) /* reject a second waiter (unlocked exclusive conditional) */

const struct lockAttr proc_lockAttrDefault = { .type = PH_LOCK_NORMAL, .protocol = PH_LOCK_PROTO_INHERIT, .robust = PH_LOCK_STALLED };

/* Maximum depth for transitive lock PI chain walking (scheduler-side) */
#define BWI_MAX_CHAIN_DEPTH 8

/* Special empty queue value used to wakeup next enqueued thread. This is used to implement sticky conditions */
static thread_t *const wakeupPending = (void *)-1;

static struct {
	vm_map_t *kmap;
	spinlock_t spinlock;
	lock_t lock;
	sched_context_t *ready[NPRIOS];
	sched_context_t **currentSc;
	thread_t **currentThread;
	time_t utcoffs;

	u64 readyNonempty;

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


_Static_assert(MAX_PRIO <= (u8)-1, "MAX_PRIO must fit into priority type");


static thread_t *_proc_current(void);
static void _proc_threadUnlink(thread_t *t);
static void _proc_threadDequeue(thread_t *t);
static int _proc_threadWait(thread_t **queue, time_t timeout, spinlock_ctx_t *scp);
static int _proc_threadWakeupPrio(prio_queue_t *queue);


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

	LIB_ASSERT_ALWAYS(t->scActive != NULL, "attempted to update unschedulable thread (type=%d)", type);

	if (type == event_waking || type == event_preempted) {
		t->scActive->readyTime = now;
	}
	else if (type == event_scheduling) {
		wait = now - t->scActive->readyTime;

		if (t->scActive->maxWait < wait) {
			t->scActive->maxWait = wait;
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


static void _sc_return(thread_t *server, thread_t *caller, sched_context_t *sc);
static sched_context_t *_sc_ofDonor(thread_t *t, thread_t *donor);


static void _readyAdd(thread_t *t)
{
	unsigned int prio = t->priority;
	LIST_ADD(&threads_common.ready[prio], t->scActive);
	threads_common.readyNonempty |= ((u64)1U << prio);
	t->onReady = 1;
}


static void _readyRemoveSc(sched_context_t *sched, unsigned int prio)
{
	LIST_REMOVE(&threads_common.ready[prio], sched);
	if (threads_common.ready[prio] == NULL) {
		threads_common.readyNonempty &= ~((u64)1U << prio);
	}
	sched->t->onReady = 0;
}


static void _readyRemove(thread_t *t)
{
	_readyRemoveSc(t->scActive, t->priority);
}


/* Returns 1 if t was already on ready queue. */
static int _readyUnlink(thread_t *t)
{
	int queued = (t->onReady != 0U) ? 1 : 0;

	if (queued != 0) {
		LIB_ASSERT(LIST_BELONGS(&threads_common.ready[t->priority], t->scActive) != 0,
				"thread: 0x%p, tid: %d, priority: %d, is not on the ready list",
				t, proc_getTid(t), t->priority);
		_readyRemove(t);
	}

	return queued;
}


static void _readyRelink(thread_t *t, int queued)
{
	if (queued != 0) {
		_readyAdd(t);
	}
}


static u8 _readyMin(void)
{
	return (threads_common.readyNonempty != 0ULL) ? __builtin_ctzll(threads_common.readyNonempty) : NPRIOS;
}


static void _setCallerMsgReturn(thread_t *recv, thread_t *caller, int retval)
{
	sched_context_t *donated_sc = _sc_ofDonor(recv, caller);

	_sc_return(recv, caller, donated_sc);
	caller->state = READY;
	recv->reply = NULL;
	_readyAdd(caller);

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
	LIB_ASSERT(recv->scActive != NULL, "recv sched null?");
	LIB_ASSERT(caller->state == READY, "caller should be ready!");
	LIB_ASSERT(recv->scActive->t == recv, "badly linked sched context");
	LIB_ASSERT(recv->scActive != donated_sc, "returning with donated SC that was already returned??");
}


__attribute__((noreturn)) void threads_halt(void)
{
	spinlock_ctx_t sc;

	/*
	 * Take the threads spinlock and not release it - this is an attempt to stop other cores in SMP from
	 * continuing to execute code (they should eventually hang trying to run the scheduler).
	 */
	hal_spinlockSet(&threads_common.spinlock, &sc);
	for (;;) {
		hal_cpuHalt();
	}

	__builtin_unreachable();
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

	threads_releaseXferBufs(thread);

	/* REVISIT: guard with threads spinlock needed? called may hold a reference to us */
	hal_spinlockSet(&threads_common.spinlock, &sc);

	/* may still be sleeping on a port's receiver queue */
	_proc_threadUnlink(thread);

	if (thread->called != NULL) {
		LIB_ASSERT(thread->called->reply == thread, "thread->called->reply != thread");
		thread->called->reply = NULL;
		LIB_ASSERT(0, "happens ever?");
	}

	if (thread->scActive != thread->scOwn) {
		if (thread->reply != NULL) {
			reply = thread->reply;

			LIB_ASSERT(thread->passive == 1, "thread not passive?");
			LIB_ASSERT(reply != thread, "thread replies to itself?");
			LIB_ASSERT(reply->scActive == NULL, "reply has an active sc?");
			LIB_ASSERT(reply->exit == 0, "reply thread exiting? (TODO?)");

			reply->called = NULL;

			hal_spinlockClear(&threads_common.spinlock, &sc);

			/*
			 * Release reply buffers before waking the reply. Safe to
			 * be done without spinlock when done before _setCallerMsgReturn()
			 */
			threads_releaseXferBufs(reply);
			xfer_clearFlags(reply);

			hal_spinlockSet(&threads_common.spinlock, &sc);

			_setCallerMsgReturn(thread, reply, -EPIPE);

			hal_spinlockClear(&threads_common.spinlock, &sc);
		}
		else {
			hal_spinlockClear(&threads_common.spinlock, &sc);
		}
	}
	else {
		hal_spinlockClear(&threads_common.spinlock, &sc);
	}

	LIB_ASSERT(thread->scOwn->t == thread, "own SC still donated?");
	vm_kfree(thread->scOwn);
	vm_kfree(thread->kstack);

	xfer_releaseIpcBuf(thread);

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
	t = lib_treeof(thread_t, idlinkage, lib_idtreeFind(&threads_common.id, tid));
	if (t != NULL) {
		++t->refs;
	}
	(void)proc_lockClear(&threads_common.lock);

	return t;
}


int proc_firstThreadTid(process_t *proc)
{
	spinlock_ctx_t sc;
	int tid;

	hal_spinlockSet(&threads_common.spinlock, &sc);
	if (proc->threads == NULL) {
		tid = -1;
	}
	else {
		tid = proc_getTid(proc->threads);
	}
	hal_spinlockClear(&threads_common.spinlock, &sc);

	return tid;
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

	if (current != NULL && current->scActive != NULL) {
		current->scActive->cpuTime += now - current->scActive->lastTime;
		current->scActive->lastTime = now;
	}

	if (selected != NULL && current != selected) {
		LIB_ASSERT(selected->scActive != NULL, "selected thread is unschedulable?");
		selected->scActive->lastTime = now;
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


/* assuming aspace of `to` and that msg pointers in aspace of `from` */
static void _threads_copyMsgBufResponse(thread_t *from, thread_t *to, msg_t *msg)
{
	xfer_copyOutExtra(from, to, msg);

	to->ipc.msgPtr->o.size = msg->o.size;
	hal_memcpy(to->ipc.msgPtr->o.raw, msg->o.raw, MSG_RAW_SIZE);
	to->ipc.msgPtr->o.err = msg->o.err;

	/* TODO: handle pulse as well? */
}


static void _threads_switchToThread(cpu_context_t *context, thread_t *selected)
{
	process_t *proc;
	cpu_context_t *signalCtx, *selCtx;

	threads_common.currentSc[hal_cpuGetID()] = selected->scActive;
	threads_common.currentThread[hal_cpuGetID()] = selected;
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

	if (selected->ipc.defer != NULL) {
		_threads_copyMsgBufResponse(selected->ipc.defer, selected, &selected->ipc.msgDefer);
		selected->ipc.defer = NULL;
	}

	if (selected->ipcInterrupted != 0) {
		selected->ipc.msgPtr->o.err = -EINTR;
		selected->ipcInterrupted = 0;
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


/* WARN: do not replace with threads_common.current[cpu]->t - see bwi-docs.md. */
static thread_t *_proc_runningThread(void)
{
	return threads_common.currentThread[hal_cpuGetID()];
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
	LIB_ASSERT(dest->scActive != NULL, "dest shed is null");

	threads_common.currentSc[hal_cpuGetID()] = dest->scActive;
	threads_common.currentThread[hal_cpuGetID()] = dest;

	_threads_scheduling(dest);

	return ctx;
}


static sched_context_t *_sc_best(thread_t *t)
{
	/* TODO: optimize */
	sched_context_t *best = t->scOwn;
	sched_context_t *sc = t->scDonated;

	if (sc != NULL) {
		do {
			if (sc->priority < best->priority) {
				best = sc;
			}
			sc = sc->dnext;
		} while (sc != t->scDonated);
	}

	return best;
}


static void _sc_setActive(thread_t *t, sched_context_t *sc)
{
	int queued = _readyUnlink(t);
	t->scActive = sc;

	if (t->scActive != NULL) {
		t->scActive->t = t;

		/*
		 * NOTE: t->priorityBase must never be derived from scActive->priorityBase: scActive
		 * can be a donated SC, and clobbering t's own permanent base with the
		 * donor's would leave t unable to revert once un-boosted.
		 */
		t->priority = t->scActive->priority;
	}

	_readyRelink(t, queued);
}


static void _sc_recalculate(thread_t *t)
{
	_sc_setActive(t, _sc_best(t));
}


static sched_context_t *_sc_ofDonor(thread_t *t, thread_t *donor)
{
	sched_context_t *sc = t->scDonated;
	LIB_ASSERT(t->scDonated != NULL, "scDonated NULL?");

	if (sc != NULL) {
		do {
			if (sc->donor == donor) {
				return sc;
			}
			sc = sc->dnext;
		} while (sc != t->scDonated);
	}

	LIB_ASSERT(0, "would return null SC");

	return NULL;
}


static void _sc_donateAt(thread_t *from, thread_t *to, sched_context_t *sc, unsigned int depth);
static void _sc_relay(thread_t *to, unsigned int depth);


/* BWI: who `t` is blocked behind while away (lock owner or IPC callee), or
 * NULL if `t` isn't away (e.g. a passive IPC receiver). See bwi-docs.md. */
static thread_t *_sc_awayTarget(thread_t *t)
{
	if (t->waitingOn != NULL) {
		return t->waitingOn->owner;
	}

	if (t->called != NULL) {
		return t->called;
	}

	return NULL;
}


/*
 * BWI: reclaim `sc` back onto `owner` without waking it. `sc->donor` tracks
 * only the immediate previous hop, not the original owner, so if it isn't
 * `owner` yet, unwind one hop at a time via recursion first. See bwi-docs.md.
 */
static void _sc_reclaim(thread_t *owner, sched_context_t *sc, unsigned int depth)
{
	thread_t *holder;

	/* TODO: make this an explicit error. Failing this is a caller's fault */
	LIB_ASSERT(depth < BWI_MAX_CHAIN_DEPTH, "PI chain too deep (cycle?)");

	if (sc->donor != owner) {
		_sc_reclaim(sc->donor, sc, depth + 1);
	}

	LIB_ASSERT(sc->donor == owner, "reclaiming SC not forwarded by `owner`");

	holder = sc->t;

	if (sc != holder->scOwn) {
		LIST_REMOVE_EX(&holder->scDonated, sc, dnext, dprev);
	}

	/*
	 * holder may have been forwarding sc further by itself. Clear the pointer so
	 * that _sc_relay() below doesn't reclaim it
	 */
	if (holder->relayed == sc) {
		holder->relayed = NULL;
	}

	sc->t = owner;
	sc->donor = owner->prevDonor;
	owner->prevDonor = NULL;

	if (owner->scOwn != sc) {
		LIST_ADD_EX(&owner->scDonated, sc, dnext, dprev);
	}

	if (_sc_awayTarget(holder) != NULL) {
		/* holder is away - it must always be forwarding its current best. */
		_sc_relay(holder, depth + 1);
	}
	else if (holder->scActive == sc) {
		_sc_recalculate(holder);
	}
	else {
		/* No action required */
	}
}


/* BWI: `to` is away - make sure it forwards its current best SC to whoever
 * it's deferring to, reclaiming and replacing what it forwarded before. */
static void _sc_relay(thread_t *to, unsigned int depth)
{
	thread_t *next;
	sched_context_t *best;

	LIB_ASSERT(depth < BWI_MAX_CHAIN_DEPTH, "PI chain too deep (cycle?)");

	if (to->relayed != NULL) {
		_sc_reclaim(to, to->relayed, depth + 1);
		to->relayed = NULL;
	}

	next = _sc_awayTarget(to);
	best = _sc_best(to);
	to->relayed = best;
	_sc_donateAt(to, next, best, depth + 1);
}


static void _sc_donateAt(thread_t *from, thread_t *to, sched_context_t *sc, unsigned int depth)
{
	// LIB_ASSERT(from->exit == 0, "got it...");

	LIB_ASSERT(depth < BWI_MAX_CHAIN_DEPTH, "PI chain too deep (cycle?)");
	LIB_ASSERT(sc != NULL, "what?");

	/* Remove SC from `from` */
	if (sc == from->scOwn) {
		/* own SC: mark as donated but keep scOwn pointer */
	}
	else {
		LIST_REMOVE_EX(&from->scDonated, sc, dnext, dprev);
	}

	_sc_setActive(from, NULL);
	from->relayed = sc;

	/*
	 * Stack the donors. prevDonor/donor fields express a donor stack. Since a thread
	 * can donate at most one SC, we can keep the previous donor of that SC in
	 * its thread_t and mark him as the current donor.
	 */
	from->prevDonor = sc->donor;
	sc->donor = from;

	sc->t = to;

	LIB_ASSERT(sc != to->scOwn, "EEEEE?");
	LIST_ADD_EX(&to->scDonated, sc, dnext, dprev);

	if (_sc_awayTarget(to) == NULL) {
		/* Not away (running, or idle/passive - both also carry
		 * scActive == NULL): use sc directly. */
		_sc_recalculate(to);

		LIB_ASSERT(to->scActive->t == to && (to->scActive->donor != NULL || to->scActive->owner == to), "mismanaged SC");
		return;
	}

	/* `to` is itself away - relay instead of using sc directly. */
	_sc_relay(to, depth + 1);
}


static void _sc_donate(thread_t *from, thread_t *to, sched_context_t *sc)
{
	_sc_donateAt(from, to, sc, 0);
}


static void _sc_return(thread_t *from, thread_t *to, sched_context_t *sc)
{
	LIB_ASSERT(sc->donor == to, "returning SC donated by someone else");
	LIB_ASSERT(to->called == NULL, "_sc_return but called not cleared?");
	LIB_ASSERT(from->scDonated != NULL && from->scDonated->dnext != NULL, "empty/corrupted donation queue?");

	/* Remove donated SC from server */
	LIST_REMOVE_EX(&from->scDonated, sc, dnext, dprev);

	/* Return to caller */
	sc->t = to;

	if (to->scOwn != sc) {
		/* caller is in a reply or lock-wait chain */
		LIST_ADD_EX(&to->scDonated, sc, dnext, dprev);

		/*
		 * Either `to` is an IPC forward-chain receiver (reply != NULL), or it
		 * was itself away and sc must be exactly what it was relaying -
		 * to->waitingOn isn't a reliable proxy here, it's cleared as soon as
		 * a lock is granted (see _proc_lockUnlock).
		 */
		LIB_ASSERT(sc == to->relayed || to->reply != NULL, "caller has a donated SC but isn't relaying it and is not in a reply chain?");
	}

	/* Unstack the donors */
	sc->donor = to->prevDonor;
	to->prevDonor = NULL;

	_sc_setActive(to, sc);
	to->state = READY;

	/* `to` is awake now, no longer forwarding anything. */
	to->relayed = NULL;

	_sc_recalculate(from);

	LIB_ASSERT(from->scActive->donor != NULL || from->scActive->owner == from, "mismanaged SC");
}


/* BWI: re-home a still-on-loan SC to a new recipient without touching its
 * donor - used when a lock hand-off leaves other waiters' donations
 * following the lock to its new owner. */
static void _sc_migrate(thread_t *from, thread_t *to, sched_context_t *sc)
{
	LIB_ASSERT(sc->t == from, "migrating SC not held by `from`");

	LIST_REMOVE_EX(&from->scDonated, sc, dnext, dprev);
	sc->t = to;
	LIST_ADD_EX(&to->scDonated, sc, dnext, dprev);
}


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
	threads_common.currentSc[cpuId] = NULL;
	threads_common.currentThread[cpuId] = NULL;

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
			// LIB_ASSERT(current->scActive != NULL, "READY but unschedulable? tid: %d, pc=%p, ra=%p", proc_getTid(current), current->context->sepc, current->context->ra);

			_readyAdd(current);
			_threads_preempted(current);
		}
	}

	/* Get next thread */
	i = _readyMin();
	while (i < NPRIOS) {
		sched = threads_common.ready[i];
		LIB_ASSERT(sched != NULL, "sched null despite ctz?");

		LIB_ASSERT(sched->t != NULL, "dangling scheduling context");

		_readyRemoveSc(sched, i);

		LIB_ASSERT(sched->t->scActive != NULL, "sched points to unschedulable thread");

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
	return _proc_runningThread();
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


int proc_threadCreate(process_t *process, startFn_t start, int *id, u8 priority, size_t kstacksz, void *stack, size_t stacksz, unsigned int sigmask, void *arg)
{
	thread_t *t;
	spinlock_ctx_t sc;
	int err;

	if (priority >= NPRIOS) {
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
	t->sigmask = sigmask;
	t->sigpend = 0;
	t->refs = 1;
	t->interruptible = 0;
	t->passive = 0;
	t->exit = 0;
	t->execdata = NULL;
	t->wait = NULL;
	t->locks = NULL;
	t->waitingOn = NULL;
	t->relayed = NULL;
	t->longjmpctx = NULL;
	hal_memset(&t->ipc, 0, sizeof(t->ipc));

	t->fastpathExitCtx = NULL;
	t->callReturnable = 0;
	t->saveCtxInReply = 0;
	t->respondAndRecv = 0;

	t->priorityBase = priority;
	t->priority = priority;
	t->longjmpctx = NULL;
	t->scOwn = vm_kmalloc(sizeof(sched_context_t));
	if (t->scOwn == NULL) {
		vm_kfree(t->kstack);
		vm_kfree(t);
		return -ENOMEM;
	}
	t->scOwn->cpuTime = 0;
	t->scOwn->maxWait = 0;
	t->scOwn->t = t;
	t->scOwn->next = NULL;
	t->scOwn->prev = NULL;
	proc_gettime(&t->scOwn->startTime, NULL);
	t->scOwn->lastTime = t->scOwn->startTime;
	t->scOwn->owner = t;
	t->scOwn->donor = NULL;
	t->scOwn->priority = priority;
	t->scOwn->priorityBase = priority;
	t->scActive = t->scOwn;
	t->scDonated = NULL;
	t->prevDonor = NULL;

	t->reply = NULL;
	t->called = NULL;
	t->flags = 0;
	t->onReady = 0;
	t->ipcInterrupted = 0;

	if (thread_alloc(t) < 0) {
		vm_kfree(t->scActive);
		vm_kfree(t->kstack);
		vm_kfree(t);
		return -ENOMEM;
	}

	if (process != NULL && (process->tls.tdata_sz != 0U || process->tls.tbss_sz != 0U)) {
		err = process_tlsInit(&t->tls, &process->tls, process->mapp);
		if (err != EOK) {
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
	_readyAdd(t);

	hal_spinlockClear(&threads_common.spinlock, &sc);

	return EOK;
}


static u8 _proc_lockGetPriority(lock_t *lock)
{
	u8 priority = MAX_PRIO;
	thread_t *thread = lock->queue;

	if (lock->attr.protocol == PH_LOCK_PROTO_PRIOCEILING) {
		return __atomic_load_n(&lock->attr.prioceiling, __ATOMIC_RELAXED);
	}

	if (lock->attr.protocol == PH_LOCK_PROTO_INHERIT && thread != NULL) {
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
	scPrio = (thread->scActive != NULL) ? thread->scActive->priority : thread->priorityBase;

	return (lockPrio < scPrio) ? lockPrio : scPrio;
}


static void _proc_threadSetPriority(thread_t *thread, u8 priority)
{
	int queued;

	/* Clamp against thread's own base, not scActive->priorityBase - scActive
	 * may be a donated SC whose base belongs to the donor. */
	if (priority > thread->priorityBase) {
		priority = thread->priorityBase;
	}

	queued = _readyUnlink(thread);
	thread->priority = priority;
	_readyRelink(thread, queued);

	trace_eventThreadPriority(proc_getTid(thread), thread->priority);
}


int proc_threadPriority(thread_t *t, int signedPriority)
{
	spinlock_ctx_t sc;
	int ret, reschedule = 0;
	u8 priority;

	if ((signedPriority < -1) || (signedPriority > (int)MAX_PRIO)) {
		return -EINVAL;
	}

	priority = (u8)signedPriority;

	hal_spinlockSet(&threads_common.spinlock, &sc);

	/* NOTE: -1 is used to retrieve the thread priority only */
	if (signedPriority >= 0) {
		/*
		 * _proc_threadSetPriority will clamp the priority to priorityBase, so it
		 * must be updated prior to the call
		 */
		t->priorityBase = priority;

		if (priority < t->priority) {
			_proc_threadSetPriority(t, priority);
		}
		else if (priority > t->priority) {
			/* Make sure that the inherited priority from the lock is not reduced */
			if ((t->locks == NULL) || (priority <= _proc_threadGetLockPriority(t))) {
				_proc_threadSetPriority(t, priority);

				if (t == _proc_current()) {
					/* Trigger immediate rescheduling if the task has lowered its priority */
					reschedule = 1;
				}
			}
		}
		else {
			/* No action required */
		}

		if (t->scActive == t->scOwn) {
			t->scActive->priority = priority;
			t->scActive->priorityBase = t->priorityBase;
		}
	}

	ret = (int)t->priority;

	if (reschedule != 0) {
		(void)hal_cpuReschedule(&threads_common.spinlock, &sc);
	}
	else {
		(void)hal_spinlockClear(&threads_common.spinlock, &sc);
	}

	trace_eventThreadPriority(proc_getTid(t), t->priority);

	return ret;
}


/* Assumes `threads_common.spinlock` is set. */
static void _wakePassive(thread_t *t)
{
	LIB_ASSERT(t->passive == 1, "t is not passive!");

	if (t->scDonated == NULL) {
		/* this is ours SC */
		t->passive = 0;
	}

	_sc_setActive(t, _sc_best(t));

	_proc_threadDequeue(t);
}


static void _sc_reclaimRelayed(thread_t *t)
{
	sched_context_t *sc = t->relayed;
	thread_t *holder = sc->t;

	_sc_return(holder, t, sc);
	t->waitingOn = NULL;

	_proc_threadSetPriority(holder, _proc_threadGetPriority(holder));
}


static void _thread_interrupt(thread_t *t)
{
	if (t->passive == 1) {
		_wakePassive(t);
		t->ipcInterrupted = 1;
	}
	else {
		if (t->waitingOn != NULL) {
			/* Reclaim the donated SC before dequeueing, otherwise t would be left with scActive == NULL and unschedulable. */
			_sc_reclaimRelayed(t);
		}

		LIB_ASSERT(t->scDonated == NULL, "SC donated but we are not passive?");
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
	t = threads_common.currentThread[cpu];
	threads_common.currentSc[cpu] = NULL;
	threads_common.currentThread[cpu] = NULL;
	t->state = GHOST;

	_sc_setActive(t, t->scOwn);
	LIST_ADD(&threads_common.ghosts, t->scActive);

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

/* Remove t from its wait queue and the sleeping tree without making it runnable. */
static void _proc_threadUnlink(thread_t *t)
{
	if (t->wait != NULL) {
		LIST_REMOVE_EX(t->wait, t, qnext, qprev);
		t->wait = NULL;
	}

	if (t->wakeup != 0) {
		lib_rbRemove(&threads_common.sleeping, &t->sleeplinkage);
		t->wakeup = 0;
	}
}


static void _proc_threadDequeue(thread_t *t)
{
	unsigned int i;

	_proc_threadUnlink(t);

	if (t->state == GHOST) {
		return;
	}

	_threads_waking(t);

	LIB_ASSERT(t->scActive != NULL, "dequeueing unschedulable thread! tid: %d", proc_getTid(t));

	t->state = READY;
	t->interruptible = 0;

	/* MOD */
	for (i = 0; i < hal_cpuGetCount(); i++) {
		if (threads_common.currentThread[i] == t) {
			break;
		}
	}

	if ((i == hal_cpuGetCount()) && (t->onReady == 0U)) {
		_readyAdd(t);
	}
}


static void _proc_threadEnqueueThread(thread_t *t, thread_t **queue, u8 state, time_t timeout, int interruptible)
{
	LIST_ADD_EX(queue, t, qnext, qprev);

	t->state = state;
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

	_proc_threadEnqueueThread(current, queue, SLEEP, timeout, interruptible);

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


static int proc_threadWaitEx(thread_t **queue, spinlock_t *spinlock, time_t timeout, u32 flags, spinlock_ctx_t *scp)
{
	int err;
	spinlock_ctx_t tsc;
	spinlock_ctx_t *rescheduleScp = (scp == NULL) ? &tsc : scp;

	LIB_ASSERT((spinlock == NULL) == (scp == NULL), "spinlock and scp must both be NULL or both be non-NULL");

	hal_spinlockSet(&threads_common.spinlock, &tsc);

	if (((flags & THREAD_WAIT_INTERRUPTIBLE) != 0U) && (_proc_current()->exit != 0U)) {
		/* Waiting in this state can lead to becoming a hanging zombie */
		hal_spinlockClear(&threads_common.spinlock, &tsc);
		return -EINTR;
	}

	if (((flags & THREAD_WAIT_EXCLUSIVE) != 0U) && (*queue != NULL) && (*queue != wakeupPending)) {
		hal_spinlockClear(&threads_common.spinlock, &tsc);
		return -EBUSY;
	}

	_proc_threadEnqueue(queue, timeout, ((flags & THREAD_WAIT_INTERRUPTIBLE) != 0U) ? 1U : 0U);

	if (*queue == NULL) {
		hal_spinlockClear(&threads_common.spinlock, &tsc);
		return EOK;
	}

	if (spinlock != NULL) {
		/* tsc and scp are swapped intentionally, we need to enable interrupts */
		hal_spinlockClear(spinlock, &tsc);
	}
	err = hal_cpuReschedule(&threads_common.spinlock, rescheduleScp);
	if (spinlock != NULL) {
		hal_spinlockSet(spinlock, scp);
	}

	return err;
}


int proc_threadWait(thread_t **queue, spinlock_t *spinlock, time_t timeout, spinlock_ctx_t *scp)
{
	return proc_threadWaitEx(queue, spinlock, timeout, 0U, scp);
}


int proc_threadWaitInterruptible(thread_t **queue, spinlock_t *spinlock, time_t timeout, spinlock_ctx_t *scp)
{
	return proc_threadWaitEx(queue, spinlock, timeout, THREAD_WAIT_INTERRUPTIBLE, scp);
}


int proc_threadWaitExclusive(thread_t **queue, time_t timeout)
{
	return proc_threadWaitEx(queue, NULL, timeout, THREAD_WAIT_INTERRUPTIBLE | THREAD_WAIT_EXCLUSIVE, NULL);
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

	if (timeout < 0) {
		return -EINVAL;
	}

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
				if (err == 0) {
					firstGhost = process->ghosts;
					ghost = firstGhost;
				}
			}
		} while (err != -ETIME && err != -EINTR);

		/* the loop could have ended without a match (e.g. on timeout) with
		 * ghost still pointing to a list element - don't reap it */
		if (found == 0) {
			ghost = NULL;
		}
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
	spinlock_ctx_t sc;
	u32 sigbit;

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

	if ((sig < 0) || (sig >= NSIG)) {
		return -EINVAL;
	}
	sigbit = (u32)1U << (unsigned int)sig;

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


static int _proc_lockUnlock(lock_t *lock, int doForceUnlock);


static int _proc_lockObtained(thread_t *current, lock_t *lock)
{
	lock->owner = current;
	lock->depth = 1;

	if (lock->attr.robust == PH_LOCK_ROBUST) {
		if (lock->consistency == LOCK_INCONSISTENT) {
			return -EOWNERDEAD;
		}

		if (lock->consistency == LOCK_NOT_RECOVERABLE) {
			/* Lock was handed off to us while unrecoverable, give it back */
			(void)_proc_lockUnlock(lock, UNLOCK_TRY);
			return -ENOTRECOVERABLE;
		}
	}

	return EOK;
}


/* Assumes `lock->spinlock` and `threads_common.spinlock` are set. */
static int _proc_lockTryRaw(thread_t *current, lock_t *lock)
{
	if (lock->attr.robust == PH_LOCK_ROBUST && lock->consistency == LOCK_NOT_RECOVERABLE) {
		return -ENOTRECOVERABLE;
	}

	if (lock->owner != NULL) {
		if ((lock->attr.type == PH_LOCK_RECURSIVE) && (lock->owner == current)) {
			/* parasoft-suppress-next-line MISRAC2012-RULE_14_3-ac "False-positive - checking for overflow this way is defined in the standard" */
			if (lock->depth + 1U == 0U) {
				/* recursive lock locked too many times */
				return -EAGAIN;
			}

			lock->depth++;
			return EOK;
		}
		else {
			return -EBUSY;
		}
	}

	LIST_ADD(&current->locks, lock);

	return _proc_lockObtained(current, lock);
}


static int _proc_lockTry(thread_t *current, lock_t *lock)
{
	int ret;

	if (lock->attr.protocol == PH_LOCK_PROTO_PRIOCEILING && current->priorityBase < lock->attr.prioceiling) {
		return -EINVAL;
	}

	ret = _proc_lockTryRaw(current, lock);
	if ((ret == EOK) || (ret == -EOWNERDEAD)) {
		if (lock->attr.protocol == PH_LOCK_PROTO_PRIOCEILING) {
			_proc_threadSetPriority(current, _proc_threadGetPriority(current));
		}
	}

	return ret;
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


/* WARN: lock is already obtained when returning with EOK (handed off during _proc_lockUnlock()) */
/*
 * WARN: current must be captured by the caller before any SC donation and not re-derived via
 * _proc_current(), which would resolve to the donation recipient instead.
 */
static int _proc_lockWaitWake(lock_t *lock, thread_t *current, u8 interruptible, spinlock_ctx_t *sc, spinlock_ctx_t *scp)
{
	int err = EOK;

	for (;;) {
		if ((interruptible != 0U) && ((current->exit != 0U) || (err == -EINTR))) {
			/* lock->owner == NULL can happen when thread_destroy is called on lock owner and current */
			if (lock->owner == NULL || lock->owner != current) {
				return -EINTR;
			}
			/* else: we got the lock, we shouldn't return EINTR */
		}
		else {
			_proc_threadEnqueueThread(current, &lock->queue, SLEEP, 0, interruptible);
			/*
			 * FIXME: too many spinlocks. Make current->exit atomic and shrink the
			 * critical section of threads_common.spinlock to just the _proc_threadEnqueue()?
			 */
			hal_spinlockClear(&lock->spinlock, sc);
			err = hal_cpuReschedule(&threads_common.spinlock, scp);
			hal_spinlockSet(&lock->spinlock, scp);
			hal_spinlockSet(&threads_common.spinlock, sc);
		}

		if (lock->owner == current) {
			return EOK;
		}
	}
}


/* protocol-unaware variant of `_proc_lockSet` */
static int _proc_lockSetRaw(lock_t *lock, u8 interruptible, spinlock_ctx_t *scp)
{
	thread_t *current;
	spinlock_ctx_t sc;
	int ret = EOK, tid;

	hal_spinlockSet(&threads_common.spinlock, &sc);

	current = _proc_current();
	tid = proc_getTid(current);

	_trace_eventLockSetEnter(lock, tid);

	if ((lock->attr.type == PH_LOCK_ERRORCHECK) && (lock->owner == current)) {
		ret = -EDEADLK;
	}
	else {
		ret = _proc_lockTryRaw(current, lock);
	}

	if (ret == -EBUSY) {
		ret = _proc_lockWaitWake(lock, current, interruptible, &sc, scp);
		if (ret == EOK) {
			ret = _proc_lockObtained(current, lock);
		}
	}

	hal_spinlockClear(&threads_common.spinlock, &sc);

	_trace_eventLockSetExit(lock, tid, ret);
	return ret;
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
		ret = -EDEADLK;
	}
	else {
		ret = _proc_lockTry(current, lock);
	}

	sched_context_t *donated = NULL;

	if (ret == -EBUSY) {
		LIB_ASSERT(lock->owner != current, "lock: %s, pid: %d, tid: %d, deadlock on itself",
				lock->name, (current->process != NULL) ? process_getPid(current->process) : 0, proc_getTid(current));

		if (lock->attr.protocol == PH_LOCK_PROTO_INHERIT) {
			donated = current->scActive;
			current->waitingOn = lock;
			_sc_donate(current, lock->owner, donated);
			LIB_ASSERT(current->scActive == NULL, "?");
		}

		ret = _proc_lockWaitWake(lock, current, interruptible, &sc, scp);
		if (ret == EOK) {
			current->waitingOn = NULL;
			ret = _proc_lockObtained(current, lock);
		}
		else if (current->waitingOn != NULL) {
			/* Could happen if exit != 0 on entry to _proc_lockWaitWake() */
			_sc_reclaimRelayed(current);
		}
		else {
			/* No action required, _thread_interrupt() reclaimed the SC and cleared waitingOn */
		}
	}

	hal_spinlockClear(&threads_common.spinlock, &sc);

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


/* Assumes `lock->spinlock` and `threads_common.spinlock` are set. */
static int _proc_lockUnlock(lock_t *lock, int doForceUnlock)
{
	thread_t *owner = lock->owner, *current;
	int ret = 0;
	u8 lockPriority;

	current = _proc_current();

	_trace_eventLockClear(lock, proc_getTid(current));

	LIB_ASSERT(LIST_BELONGS(&owner->locks, lock) != 0,
			"lock: %s, owner pid: %d, owner tid: %d, lock is not on the list "
			"(current pid: %d, current tid: %d, doForceUnlock: %d, queue empty: %d, "
			"owner scActive: %p, owner waitingOn: %p, current waitingOn: %p)",
			lock->name, (owner->process != NULL) ? process_getPid(owner->process) : 0, proc_getTid(owner),
			(current->process != NULL) ? process_getPid(current->process) : 0, proc_getTid(current), doForceUnlock,
			(lock->queue == NULL), (void *)owner->scActive, (void *)owner->waitingOn, (void *)current->waitingOn);

	if (doForceUnlock == UNLOCK_TRY) {
		if ((lock->attr.type == PH_LOCK_ERRORCHECK) || (lock->attr.type == PH_LOCK_RECURSIVE) || (lock->attr.robust == PH_LOCK_ROBUST)) {
			if (lock->owner != current) {
				return -EPERM;
			}
		}
	}

	if (lock->attr.robust == PH_LOCK_ROBUST) {
		/*
		 * Don't make the lock irrecoverable on force unlock (thread death).
		 * POSIX says that if the owner of inconsistent lock dies, the
		 * inconsistency should be passed to the next lock owner.
		 */
		if ((doForceUnlock != UNLOCK_FORCE) && (lock->consistency == LOCK_INCONSISTENT)) {
			lock->consistency = LOCK_NOT_RECOVERABLE;
		}

		if ((doForceUnlock == UNLOCK_FORCE) && (lock->consistency == LOCK_CONSISTENT)) {
			lock->consistency = LOCK_INCONSISTENT;
		}
	}

	if ((lock->attr.type == PH_LOCK_RECURSIVE) && (lock->depth > 0U)) {
		if (doForceUnlock == UNLOCK_TRY) {
			lock->depth--;
			if (lock->depth != 0U) {
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

		if (lock->attr.protocol == PH_LOCK_PROTO_INHERIT) {
			_sc_return(owner, lock->owner, _sc_ofDonor(owner, lock->owner));

			/* Move donations to a new owner */
			/* FIXME: this is not ideal, as it extends the critical section of spinlocks. Come up with a way to do this interruptibly */
			if (lock->queue->qnext != lock->queue) {
				thread_t *waiter = lock->queue->qnext;

				while (waiter != lock->queue) {
					_sc_migrate(owner, lock->owner, _sc_ofDonor(owner, waiter));
					waiter = waiter->qnext;
				}

				_sc_recalculate(lock->owner);

				/* find new best SC for owner, as owner->scActive may be dangling now */
				_sc_recalculate(owner);
			}

			/*
			 * Must clear now. New owner would see lock->owner == itself and _sc_awayTarget()
			 * would resolve to itself, turning into a self-referential relay.
			 */
			lock->owner->waitingOn = NULL;
		}

		/* Wake the new owner and add lock to its held-locks list */
		_proc_threadDequeue(lock->owner);
		LIST_ADD(&lock->owner->locks, lock);

		/* Recalculate new owner's effective priority from ALL held locks + SC */
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

	return ret;
}


static void proc_lockForceUnlock(lock_t *lock, int doYield)
{
	spinlock_ctx_t scp, sc;
	int ret = 0;

	hal_spinlockSet(&lock->spinlock, &scp);
	if (lock->owner != NULL) {
		hal_spinlockSet(&threads_common.spinlock, &sc);
		ret = _proc_lockUnlock(lock, UNLOCK_FORCE);
		hal_spinlockClear(&threads_common.spinlock, &sc);
	}

	hal_spinlockClear(&lock->spinlock, &scp);
	if ((ret > 0) && (doYield != UNLOCK_DONT_YIELD)) {
		(void)hal_cpuReschedule(NULL, NULL);
	}

	LIB_ASSERT(ret >= 0, "lock: %s, force unlocking failed (%d)", lock->name, ret);
}


static int _proc_lockClear(lock_t *lock)
{
	spinlock_ctx_t sc;
	int ret;
	thread_t *current = proc_current();

	/* Safe without threads_common.spinlock: every writer of lock->owner also
	 * requires lock->spinlock, already held by the caller. */
	LIB_ASSERT(lock->owner != NULL, "lock: %s, pid: %d, tid: %d, unlock on not locked lock",
			lock->name, (current->process != NULL) ? process_getPid(current->process) : 0, proc_getTid(current));

	LIB_ASSERT(lock->owner == current, "lock: %s, pid: %d, tid: %d, owner pid: %d, owner tid: %d, unlocking someone else's lock",
			lock->name, (current->process != NULL) ? process_getPid(current->process) : 0, proc_getTid(current),
			(lock->owner->process != NULL) ? process_getPid(lock->owner->process) : 0, proc_getTid(lock->owner));

	if (lock->owner == NULL) {
		return -EPERM;
	}

	hal_spinlockSet(&threads_common.spinlock, &sc);
	ret = _proc_lockUnlock(lock, UNLOCK_TRY);
	hal_spinlockClear(&threads_common.spinlock, &sc);

	return ret;
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
	int err, lockErr;

	if (hal_started() == 0) {
		return -EINVAL;
	}

	hal_spinlockSet(&lock->spinlock, &sc);

	err = _proc_lockClear(lock);
	if (err >= 0) {
		err = proc_threadWaitInterruptible(queue, &lock->spinlock, timeout, &sc);
		if (err != -EINTR) {
			lockErr = _proc_lockSet(lock, 0U, &sc);
			if (lockErr < 0) {
				err = lockErr;
			}
		}
	}

	hal_spinlockClear(&lock->spinlock, &sc);

	return err;
}


int proc_lockConsistent(lock_t *lock)
{
	spinlock_ctx_t sc;
	int err = EOK;

	if (hal_started() == 0) {
		return -EINVAL;
	}

	if (lock->attr.robust != PH_LOCK_ROBUST) {
		return -EINVAL;
	}

	hal_spinlockSet(&lock->spinlock, &sc);

	if (lock->owner != proc_current()) {
		err = -EPERM;
	}
	else if (lock->consistency != LOCK_INCONSISTENT) {
		err = -EINVAL;
	}
	else {
		lock->consistency = LOCK_CONSISTENT;
	}

	hal_spinlockClear(&lock->spinlock, &sc);

	return err;
}


/* prioceiling == -1 retrieves current priority ceiling for the lock without obtaining it */
int proc_lockPrioCeiling(lock_t *lock, int prioceiling)
{
	spinlock_ctx_t sc;
	int err;

	if (hal_started() == 0) {
		return -EINVAL;
	}

	if (prioceiling < -1 || prioceiling > (int)MAX_PRIO) {
		return -EINVAL;
	}

	if (lock->attr.protocol != PH_LOCK_PROTO_PRIOCEILING) {
		return -EINVAL;
	}

	if (prioceiling >= 0) {
		hal_spinlockSet(&lock->spinlock, &sc);
		err = _proc_lockSetRaw(lock, 0, &sc);
		if (err < 0) {
			hal_spinlockClear(&lock->spinlock, &sc);
			return err;
		}
		hal_spinlockClear(&lock->spinlock, &sc);

		err = (int)__atomic_exchange_n(&lock->attr.prioceiling, (unsigned char)prioceiling, __ATOMIC_RELAXED);

		(void)proc_lockClear(lock);
	}
	else {
		err = (int)__atomic_load_n(&lock->attr.prioceiling, __ATOMIC_RELAXED);
	}

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
	lock->consistency = LOCK_CONSISTENT;

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
		if (t->scActive != NULL && now != t->scActive->startTime) {
			tinfo.load = (int)((t->scActive->cpuTime * 1000) / (now - t->scActive->startTime));
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
		{
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
		}

		cb(arg, i, &tinfo);

		++i;
		t = lib_treeof(thread_t, idlinkage, lib_idtreeNext(&t->idlinkage.linkage));
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


int proc_schedInfo(int policy, sched_info_t *info)
{
	if (policy != SCHED_FIFO && policy != SCHED_RR && policy != SCHED_OTHER) {
		return -EINVAL;
	}

	if (policy != SCHED_RR) {
		return -ENOTSUP;
	}

	info->interval = SYSTICK_INTERVAL;
	info->minPriority = 0;
	info->maxPriority = (int)MAX_PRIO;

	return EOK;
}


int proc_schedGet(thread_t *t, sched_params_t *params)
{
	spinlock_ctx_t sc;
	int priority, priorityBase;

	hal_spinlockSet(&threads_common.spinlock, &sc);
	priority = (int)t->priority;
	priorityBase = (int)t->priorityBase;
	hal_spinlockClear(&threads_common.spinlock, &sc);

	params->priority = priority;
	params->priorityBase = priorityBase;
	params->policy = SCHED_RR;

	return EOK;
}


int proc_schedSet(thread_t *t, int policy, sched_params_t *params)
{
	int err;

	if (policy != SCHED_FIFO && policy != SCHED_RR && policy != SCHED_OTHER) {
		return -EINVAL;
	}

	if (policy != SCHED_RR) {
		return -ENOTSUP;
	}

	if (params->priorityBase < 0) {
		return -EINVAL;
	}

	err = proc_threadPriority(t, params->priorityBase);
	return err < 0 ? err : EOK;
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

	/* Initialize scheduler queues */
	for (i = 0; i < sizeof(threads_common.ready) / sizeof(thread_t *); i++) {
		threads_common.ready[i] = NULL;
	}
	threads_common.readyNonempty = 0;

	lib_rbInit(&threads_common.sleeping, threads_sleepcmp, NULL);
	lib_idtreeInit(&threads_common.id);

	lib_printf("proc: Initializing thread scheduler, priorities=%d\n", sizeof(threads_common.ready) / sizeof(thread_t *));

	hal_spinlockCreate(&threads_common.spinlock, "threads.spinlock");

	/* Allocate and initialize current threads array */
	/* parasoft-suppress-next-line MISRAC2012-DIR_4_7 "return value of hal_cpuGetCount() is used, false positive" */
	threads_common.currentSc = (sched_context_t **)vm_kmalloc(sizeof(sched_context_t *) * hal_cpuGetCount());
	if (threads_common.currentSc == NULL) {
		return -ENOMEM;
	}

	/* parasoft-suppress-next-line MISRAC2012-DIR_4_7 "return value of hal_cpuGetCount() is used, false positive" */
	threads_common.currentThread = (thread_t **)vm_kmalloc(sizeof(thread_t *) * hal_cpuGetCount());
	if (threads_common.currentThread == NULL) {
		return -ENOMEM;
	}

	/* Run idle thread on every cpu */
	for (i = 0; i < hal_cpuGetCount(); i++) {
		threads_common.currentSc[i] = NULL;
		threads_common.currentThread[i] = NULL;
		(void)proc_threadCreate(NULL, threads_idlethr, NULL, MAX_PRIO, (size_t)SIZE_KSTACK, NULL, 0, 0, NULL);
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


/* Assumes `p->spinlock` and `threads_common.spinlock` are set. */
static void _portAddReceiver(port_t *p, thread_t *t)
{
	_proc_threadEnqueueThread(t, &p->threads, BLOCKED_ON_RECV, 0, 1);
}


/*
 * Takes the first passive receiver of p, making the caller its sole owner.
 * Returns NULL if none is ready. Assumes `p->spinlock` and `threads_common.spinlock` are set.
 */
static thread_t *_portTakeReceiver(port_t *p)
{
	thread_t *recv = p->threads;
	if ((recv == NULL) || (recv->scActive != NULL)) {
		return NULL;
	}

	_proc_threadUnlink(recv);
	recv->interruptible = 0;

	return recv;
}


/* Assumes `p->spinlock` is set. */
void _threads_portWakeReceivers(port_t *p)
{
	thread_t *t;
	spinlock_ctx_t sc;

	hal_spinlockSet(&threads_common.spinlock, &sc);
	while ((t = p->threads) != NULL) {
		_wakePassive(t);
	}
	hal_spinlockClear(&threads_common.spinlock, &sc);
}


static int _portPrioWait(port_t *p, thread_t *caller, thread_t **recvp, spinlock_ctx_t *sc)
{
	int err;
	spinlock_ctx_t tsc;
	thread_t *recv;

	for (;;) {
		hal_spinlockSet(&threads_common.spinlock, &tsc);
		recv = _portTakeReceiver(p);
		hal_spinlockClear(&threads_common.spinlock, &tsc);

		if (recv != NULL) {
			*recvp = recv;
			return EOK;
		}

		p->queue.nonempty |= (1u << caller->priority);
		err = proc_threadWaitInterruptible(&p->queue.pq[caller->priority], &p->spinlock, 0, sc);
		if (p->closed != 0) {
			return -EINVAL;
		}
		if (err < 0) {
			return err;
		}
	}
}


static void _setReplyChain(thread_t *from, thread_t *to)
{
	/* TODO: could this be a part of SC? donor? */
	to->reply = from;
	from->called = to;
}


/* Assumes _borrowBuf() has already been called if either side's plan.kind is msg_xfer_borrow */
static int proc_send_ex(u32 port, msg_t *msg, int returnable)
{
	port_t *p;
	thread_t *caller, *recv;
	spinlock_ctx_t sc;
	int err;
	cpu_context_t *ctx;

	caller = proc_current();

#if PERF_MSG
	u64 tscs[TSCS_SIZE];
	hal_memset(tscs, 0, sizeof(tscs));
	size_t step = 0;
	u64 currTsc;
	u16 tid = proc_getTid(caller);
#endif

	TRACE_MSG_PROFILE_POINT(tid, &step, &currTsc, tscs);  // 0

	p = proc_portGet(port);
	if (p == NULL) {
		return -EINVAL;
	}

	TRACE_MSG_PROFILE_POINT(tid, &step, &currTsc, tscs);  // 1

	hal_spinlockSet(&p->spinlock, &sc);

	if (p->closed != 0) {
		hal_spinlockClear(&p->spinlock, &sc);
		port_put(p, 0);
		return -EINVAL;
	}

	/* commit to IPC - returns with recv detached from the port */
	err = _portPrioWait(p, caller, &recv, &sc);
	if (err < 0) {
		hal_spinlockClear(&p->spinlock, &sc);
		port_put(p, 0);
		return err;
	}

	TRACE_MSG_PROFILE_POINT(tid, &step, &currTsc, tscs);  // 2

	LIB_ASSERT(recv != NULL, "recv is null");

	hal_spinlockClear(&p->spinlock, &sc);

	port_put(p, 0);

	size_t isize = 0, osize = 0;

	TRACE_MSG_PROFILE_POINT(tid, &step, &currTsc, tscs);  // 3

	hal_memcpy(caller->ipc.rawBuf, msg->i.raw, MSG_RAW_SIZE);

	isize = msg->i.size;

	xferPlan_t inPlan = xfer_classify(caller, recv, msg->i.data, isize, 0);

	TRACE_MSG_PROFILE_POINT(tid, &step, &currTsc, tscs);  // 4

	osize = msg->o.size;
	xferPlan_t outPlan = xfer_classify(caller, recv, msg->o.data, osize, (inPlan.kind == msg_xfer_extra) ? isize : 0);

	TRACE_MSG_PROFILE_POINT(tid, &step, &currTsc, tscs);  // 5

	oid_t oid;
	int type;

	hal_memcpy(&oid, &msg->oid, sizeof(oid_t));
	type = msg->type;

	xfer_clearFlags(caller);

	void *idata = NULL, *odata = NULL;

	/* message transfer */

	if ((inPlan.kind == msg_xfer_borrow || outPlan.kind == msg_xfer_borrow) && xfer_ipcBufBorrow(caller, recv) != 0) {
		return -ENOMEM;
	}

	if (xfer_setup(caller, recv, &inPlan, &caller->ipc.ibl, &idata, msg_side_in) < 0) {
		return -ENOMEM;
	}

	if (xfer_setup(caller, recv, &outPlan, &caller->ipc.obl, &odata, msg_side_out) < 0) {
		xfer_bufRelease(&caller->ipc.ibl);
		return -ENOMEM;
	}

	if (inPlan.kind == msg_xfer_extra) {
		/* small message: fits the predefined recv buffer */
		hal_memcpy(recv->ipc.kw, msg->i.data, isize);
	}

	__atomic_store_n(&caller->ipc.bufsInit, 1U, __ATOMIC_RELEASE);

	hal_spinlockSet(&threads_common.spinlock, &sc);
	_sc_donate(caller, recv, caller->scActive);
	_setReplyChain(caller, recv);

	caller->ipc.msgPtr = msg;
	caller->state = BLOCKED_ON_REPLY;
	if (returnable != 0) {
		caller->callReturnable = 1;
	}
	recv->state = READY;

	TRACE_MSG_PROFILE_POINT(tid, &step, &currTsc, tscs);  // 6
	ctx = _threads_switchTo(recv);
	TRACE_MSG_PROFILE_POINT(tid, &step, &currTsc, tscs);  // 7

	LIB_ASSERT(_proc_current() == recv, "we are not recv?");
	LIB_ASSERT(_proc_current()->scActive != NULL, "proc current unschedulable?");

	TRACE_MSG_PROFILE_POINT(tid, &step, &currTsc, tscs);  // 8

	LIB_ASSERT(recv->refs > 0, "attempting to return to refs=0 rcv? port=%d caller tid=%d recv tid=%d refs: %d",
			p->linkage.id, proc_getTid(caller), proc_getTid(recv), recv->refs);

	TRACE_MSG_PROFILE_POINT(tid, &step, &currTsc, tscs);  // 9

	LIB_ASSERT(recv->exit == 0, "recv exit=%d", recv->exit);
	LIB_ASSERT(recv->ipc.msgPtr != NULL, "recv msg is null");

	TRACE_MSG_PROFILE_POINT(tid, &step, &currTsc, tscs);  // 10

	hal_memcpy(recv->ipc.msgPtr->i.raw, caller->ipc.rawBuf, MSG_RAW_SIZE);

	TRACE_MSG_PROFILE_POINT(tid, &step, &currTsc, tscs);  // 11

	recv->ipc.msgPtr->i.size = isize;
	recv->ipc.msgPtr->i.data = idata;
	recv->ipc.msgPtr->o.data = odata;
	recv->ipc.msgPtr->o.size = osize;

	recv->ipc.msgPtr->pid = (caller->process != NULL) ? process_getPid(caller->process) : 0;
	hal_memcpy(&recv->ipc.msgPtr->oid, &oid, sizeof(oid_t));
	recv->ipc.msgPtr->type = type;
	recv->ipc.msgPtr->priority = caller->priority;        /* ??? */
	TRACE_MSG_PROFILE_POINT(tid, &step, &currTsc, tscs);  // 12

	*recv->ipc.ridPtr = (msg_rid_t)((ptr_t)caller ^ threads_common.ridCookie);
	hal_cpuSetReturnValue(ctx, EOK);

	recv->fastpathExitCtx = ctx;

	LIB_ASSERT(_proc_current() == recv, "we should be recv here");
	LIB_ASSERT(recv->exit == 0, "recv wants to exit! TODO");

	trace_eventSyscallExit(recv->respondAndRecv ? syscall_msgRespondAndRecv : syscall_msgRecv, proc_getTid(recv));

	TRACE_MSG_PROFILE_EXIT_FUNC(tid, syscall_msgSend, &step, &currTsc, tscs);

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


static thread_t *_ridToReply(msg_rid_t rid, thread_t *current)
{
	void *reply = (void *)((ptr_t)rid ^ threads_common.ridCookie);
	thread_t *t;

	if (reply == NULL) {
		return NULL;
	}

#ifndef NOMMU
	if (pmap_belongs(&threads_common.kmap->pmap, reply) == 0) {
		return NULL;
	}
#else
	if (((ptr_t)reply < (ptr_t)threads_common.kmap->start) ||
			((ptr_t)reply > (ptr_t)threads_common.kmap->stop - sizeof(thread_t))) {
		return NULL;
	}
#endif

	t = (thread_t *)reply;
	if (t->called != current) {
		LIB_ASSERT(0, "unmatched reply for %p: response from %p, but reply called %p\n", reply, current, t->called);
		return NULL;
	}

	return t;
}


int proc_forward(u32 port, msg_t *msg, msg_rid_t rid)
{
	port_t *p;
	thread_t *caller, *recv, *forward;
	spinlock_ctx_t sc, tsc;

	recv = proc_current();
	LIB_ASSERT(recv != NULL, "recv is null???");

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

	/* returns with `forward` detached from the port */
	int err = _portPrioWait(p, recv, &forward, &sc);
	if (err < 0) {
		hal_spinlockClear(&p->spinlock, &sc);
		port_put(p, 0);
		return err;
	}

	hal_spinlockSet(&threads_common.spinlock, &tsc);
	caller = _ridToReply(rid, recv);
	if (caller == NULL) {
		LIB_ASSERT(0, "TODO: commodify the respond paths");
	}

	LIB_ASSERT(forward != NULL, "forward null");
	LIB_ASSERT(recv != NULL, "recv is null");

	if (forward->process != recv->process) {
		LIB_ASSERT(0, "he");
		/* not forwarding after all - put the receiver back */
		_portAddReceiver(p, forward);
		(void)_proc_threadWakeupPrio(&p->queue);
		hal_spinlockClear(&threads_common.spinlock, &tsc);
		hal_spinlockClear(&p->spinlock, &sc);
		port_put(p, 0);
		return -EINVAL;
	}

	recv->interruptible = 1;
	hal_spinlockClear(&threads_common.spinlock, &tsc);
	hal_spinlockClear(&p->spinlock, &sc);

	/* same aspace, we can copy directly */
	hal_memcpy(forward->ipc.msgPtr, msg, sizeof(*msg));

	*forward->ipc.ridPtr = rid;
	forward->fastpathExitCtx = _getUserContext(forward);
	trace_eventSyscallExit(forward->respondAndRecv ? syscall_msgRespondAndRecv : syscall_msgRecv, proc_getTid(forward));

	hal_cpuSetReturnValue(forward->fastpathExitCtx, EOK);

	hal_memcpy(&forward->ipc.ibl, &recv->ipc.ibl, sizeof(recv->ipc.ibl));
	hal_memcpy(&forward->ipc.obl, &recv->ipc.obl, sizeof(recv->ipc.obl));

	__atomic_store_n(&forward->ipc.bufsInit, 1, __ATOMIC_RELEASE);
	__atomic_store_n(&recv->ipc.bufsInit, 0, __ATOMIC_RELEASE);

	hal_spinlockSet(&threads_common.spinlock, &tsc);

	sched_context_t *donated_sc = _sc_ofDonor(recv, caller);

	/* TODO: optimize these */
	caller->called = NULL;
	_sc_return(recv, caller, donated_sc);
	recv->reply = NULL;
	_sc_donate(caller, forward, donated_sc);
	_setReplyChain(caller, forward);
	caller->state = BLOCKED_ON_REPLY;
	forward->state = READY;

	/* could have changed as part of _sc_return */
	threads_common.currentSc[hal_cpuGetID()] = recv->scActive;
	threads_common.currentThread[hal_cpuGetID()] = recv;

	_readyAdd(forward);

	if (forward->priority < recv->priority) {
		hal_cpuReschedule(&threads_common.spinlock, &tsc);
	}
	else {
		hal_spinlockClear(&threads_common.spinlock, &tsc);
	}

	/* TODO: potentially unnecessary */
	hal_memset(&recv->ipc.ibl, 0, sizeof(recv->ipc.ibl));
	hal_memset(&recv->ipc.obl, 0, sizeof(recv->ipc.obl));

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

	queue->nonempty &= ~(1u << prio);
	return 0;
}


int proc_threadBroadcastPrio(prio_queue_t *queue)
{
	int ret = 0;
	spinlock_ctx_t sc;
	size_t prio;

	hal_spinlockSet(&threads_common.spinlock, &sc);

	for (prio = 0; prio < NPRIOS; prio++) {
		ret += _proc_threadBroadcast(&queue->pq[prio]);
	}

	hal_spinlockClear(&threads_common.spinlock, &sc);

	return ret;
}


/* TODO: move this queue to lib */
void proc_threadPrioQueueInit(prio_queue_t *queue)
{
	hal_memset(queue->pq, 0, sizeof(queue->pq));
	queue->nonempty = 0;
}


static int _postPassiveWakeup(port_t *p, thread_t *recv)
{
	int err;

	port_put(p, 0);

	if ((recv->flags & IPC_PULSED) != 0) {
		recv->flags &= (~(int)IPC_PULSED);
		recv->ipc.msgPtr->o.pulse = recv->ipc.pulse;
		recv->ipc.msgPtr->o.err = EOK;
		err = -EPULSE;
	}
	else if (recv->reply == NULL) {
		err = -EINTR;
	}
	else {
		*recv->ipc.ridPtr = (msg_rid_t)((ptr_t)recv->reply ^ threads_common.ridCookie);
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

	_sc_setActive(recv, NULL);
	recv->passive = 1;
	recv->flags &= (~(int)IPC_PULSED);

	_portAddReceiver(p, recv);
	/* TODO: direct hand-off to highest prio client */
	(void)_proc_threadWakeupPrio(&p->queue);
	hal_spinlockClear(&threads_common.spinlock, &tsc);

	hal_spinlockClear(&p->spinlock, sc);

	hal_cpuReschedule(NULL, NULL);

	/* WARN: won't be reached if recv is woken in fastpath proc_send switch */
	return _postPassiveWakeup(p, recv);
}


/* assumes aspace of recv */
int _returnWithPulse(thread_t *recv, port_t *p, spinlock_ctx_t *sc)
{
	recv->ipc.msgPtr->o.pulse = p->pulse;
	recv->ipc.msgPtr->o.err = EOK;
	p->flags = 0;
	hal_spinlockClear(&p->spinlock, sc);
	port_put(p, 0);
	return -EPULSE;
}


int proc_recv_ex(port_t *p, msg_t *msg, msg_rid_t *rid, int rr)
{
	spinlock_ctx_t sc;
	thread_t *recv;

	recv = proc_current();
	recv->ipc.ridPtr = rid;
	recv->ipc.msgPtr = msg;

	hal_spinlockSet(&p->spinlock, &sc);
	if (p->closed != 0) {
		hal_spinlockClear(&p->spinlock, &sc);
		port_put(p, 0);
		return -EINVAL;
	}

	if ((p->flags & IPC_PULSED) != 0) {
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
	spinlock_ctx_t sc, tsc;
	thread_t *recv;

	p = proc_portGet(port);
	if (p == NULL) {
		return -EINVAL;
	}

	hal_spinlockSet(&p->spinlock, &sc);
	hal_spinlockSet(&threads_common.spinlock, &tsc);
	recv = p->threads;

	if (recv != NULL) {
		LIB_ASSERT(recv->state != READY, "how is recv ready while on port queue?");

		_wakePassive(recv);

		recv->ipc.pulse = pulse;
		recv->flags |= IPC_PULSED;
		hal_spinlockClear(&threads_common.spinlock, &tsc);

		if (recv->priority < proc_current()->priority) {
			hal_cpuReschedule(&p->spinlock, &sc);
		}
		else {
			hal_spinlockClear(&p->spinlock, &sc);
		}
	}
	else {
		hal_spinlockClear(&threads_common.spinlock, &tsc);

		/* stick the pulse to port for late receivers */
		p->pulse = pulse;
		p->flags |= IPC_PULSED;
		hal_spinlockClear(&p->spinlock, &sc);
	}

	port_put(p, 0);

	return EOK;
}


static int proc_respond_ex(port_t *p, msg_t *msg, msg_rid_t rid)
{
	spinlock_ctx_t sc, tsc;
	thread_t *caller, *recv;
	int err = EOK;

	hal_spinlockSet(&p->spinlock, &sc);
	if (p->closed != 0) {
		hal_spinlockClear(&p->spinlock, &sc);
		return -EINVAL;
	}

	hal_spinlockSet(&threads_common.spinlock, &tsc);
	recv = _proc_current();
	caller = _ridToReply(rid, recv);
	if (caller == NULL) {
		hal_spinlockClear(&threads_common.spinlock, &tsc);
		hal_spinlockClear(&p->spinlock, &sc);
		return -EINVAL;
	}

	do {
		/* clear called already to prevent races on SMP */
		caller->called = NULL;

		if (caller->exit != 0) {
			/* caller is dying, don't respond */
			LIB_ASSERT(recv->passive == 1, "recv not passive?");

			sched_context_t *donated_sc = _sc_ofDonor(recv, caller);
			_sc_return(recv, caller, donated_sc);

			caller->state = GHOST;
			LIST_ADD(&threads_common.ghosts, caller->scActive);
			_proc_threadWakeup(&threads_common.reaper);

			recv->reply = NULL;
			if (recv->scDonated == NULL) {
				/* this is our SC */
				recv->passive = 0;
			}
			_sc_recalculate(recv);

			threads_common.currentSc[hal_cpuGetID()] = recv->scActive;
			threads_common.currentThread[hal_cpuGetID()] = recv;

			LIB_ASSERT(recv->state == READY, "recv not ready?");
			LIB_ASSERT(recv->scActive->t == recv, "badly linked sched context");

			err = -EINVAL;
			break;
		}
	} while (0);

	hal_spinlockClear(&threads_common.spinlock, &tsc);
	hal_spinlockClear(&p->spinlock, &sc);

	if (err < 0) {
		return err;
	}

	xfer_finalize(recv, caller, msg);

	/*
	 * OPTIMIZATION: defer the copy of the msg to _threads_switchToThread() where
	 * we switch aspaces anyways.
	 * We *could* do it here, but would need to switch to caller aspace and back.
	 * Delegation saves us two pmap switches (potential TLB flushes) per respond fastpath
	 */
	hal_memcpy(&caller->ipc.msgDefer.o, &msg->o, sizeof(msg->o));
	caller->ipc.msgDefer.i.size = msg->i.size;
	caller->ipc.defer = recv;

	threads_releaseXferBufs(caller);
	xfer_ipcBufRelease(recv);

	hal_spinlockSet(&threads_common.spinlock, &sc);

	_setCallerMsgReturn(recv, caller, EOK);

	threads_common.currentSc[hal_cpuGetID()] = recv->scActive;
	threads_common.currentThread[hal_cpuGetID()] = recv;

	LIB_ASSERT(recv->state == READY, "recv should be ready!");

	if (caller->priority < recv->priority) {
		hal_cpuReschedule(&threads_common.spinlock, &sc);
	}
	else {
		hal_spinlockClear(&threads_common.spinlock, &sc);
	}

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
	thread_t *reply = _ridToReply(saved_rid, _proc_current());
	if (reply == NULL) {
		respond = 0;
	}
	hal_spinlockClear(&threads_common.spinlock, &sc);

	if (respond != 0) {
		err = proc_respond_ex(p, msg, saved_rid);
		if (err < 0) {
			port_put(p, 0);
			return err;
		}
	}

	return proc_recv_ex(p, msg, rid, 1);
}


void threads_releaseXferBufs(thread_t *thread)
{
	u8 bufsInit = __atomic_exchange_n(&thread->ipc.bufsInit, 0U, __ATOMIC_ACQ_REL);
	if (bufsInit == 0) {
		return;
	}
	xfer_bufRelease(&thread->ipc.ibl);
	xfer_bufRelease(&thread->ipc.obl);
}
