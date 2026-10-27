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

/* Special empty queue value used to wakeup next enqueued thread. This is used to implement sticky conditions */
static thread_t *const wakeupPending = (void *)-1;

/* Guards the ready queues, and the port queues (ports.h) and SC donations (sc.h) too. */
spinlock_t threads_spinlock;

static struct {
	vm_map_t *kmap;
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

	thread_t *ghosts;
	port_t *ghostPorts;

	thread_t *reaper;
	thread_t *reaperThread; /* the thread running proc_reap(), NULL until it starts */

	/* Debug */
	unsigned char stackCanary[16];
	time_t prev;
} threads_common;


_Static_assert(MAX_PRIO <= (u8)-1, "MAX_PRIO must fit into priority type");


static int _proc_threadWait(thread_t **queue, time_t timeout, spinlock_ctx_t *scp);


time_t _proc_gettimeRaw(void)
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


/* Note: always called with threads_spinlock set */
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


void _threads_enqueued(thread_t *t)
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

	hal_spinlockSet(&threads_spinlock, &sc);
	now = _proc_gettimeRaw();

	for (;;) {
		t = lib_treeof(thread_t, sleeplinkage, lib_rbMinimum(threads_common.sleeping.root));

		if (t == NULL || t->wakeup > now) {
			break;
		}

		if (t->state == BLOCKED_ON_RECV) {
			_wakeReceiver(t);
		}
		else {
			_sc_reclaim(t);
			_proc_threadDequeue(t);
		}
		hal_cpuSetReturnValue(t->context, (void *)-ETIME);
	}

	_threads_updateWakeup(now, t);

	hal_spinlockClear(&threads_spinlock, &sc);

	/* Invoke scheduler */
	return 1;
}


/*
 * Threads management
 */


static void proc_lockForceUnlock(lock_t *lock, int doYield);
static void _proc_threadSetPriority(thread_t *thread, u8 priority);


cpu_context_t *_threads_getUserContext(thread_t *thread)
{
	if (thread->process != NULL) {
		// if (hal_cpuSupervisorMode(thread->context) == 0) {
		return (cpu_context_t *)((char *)thread->kstack + thread->kstacksz - sizeof(cpu_context_t));
	}
	else {
		return thread->context;
	}
}


void _threads_setCurrent(thread_t *t)
{
	threads_common.currentSc[hal_cpuGetID()] = t->scActive;
	threads_common.currentThread[hal_cpuGetID()] = t;
}


void _threads_readyAdd(thread_t *t)
{
	unsigned int prio = t->priority;

	/*
	 * LIST_ADD() of a NULL element is a silent no-op, so this would set the
	 * priority's readyNonempty bit while queueing nothing - and the scheduler
	 * would then pick a priority whose list is empty.
	 */
	LIB_ASSERT(t->scActive != NULL, "queueing an unschedulable thread: tid=%d st=%u",
			proc_getTid(t), t->state);
	LIB_ASSERT(t->onReady == 0U, "queueing a thread that is queued already: tid=%d st=%u",
			proc_getTid(t), t->state);

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
}


static void _readyRemove(thread_t *t)
{
	/*
	 * onReady has to be cleared for t, not for t->scActive->t: while a donation
	 * chain is being updated an SC can already name its next holder while t
	 * still runs it and is still queued (see _sc_pop()).
	 */
	_readyRemoveSc(t->scActive, t->priority);
	t->onReady = 0;
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
		_threads_readyAdd(t);
	}
}


static thread_t *_threads_kernelCaller(thread_t *t)
{
	thread_t *r = t->reply;

	if (r != NULL) {
		do {
			if (r->callReturnable != 0U) {
				return r;
			}
			r = r->replyNext;
		} while (r != t->reply);
	}

	return NULL;
}


/*
 * A ghost never runs again, so the threads waiting on it wait for whoever
 * destroys it. Blocking it on that thread donates their SCs there.
 */
static void _threads_ghostBlockOn(thread_t *ghost, thread_t *destroyer)
{
	_sc_reclaim(ghost);

	if ((destroyer != NULL) && (_sc_reaches(destroyer, ghost) == 0)) {
		_thread_setBlocked(ghost, WAIT_REAP, destroyer);
		_sc_donate(ghost, destroyer, _sc_best(ghost));
	}
}


/*
 * The destroyer blocking on a lock can close a cycle through a ghost waiting
 * for it. The ghost stops waiting then, so the cycle never forms.
 */
static void _threads_ghostBreakCycle(thread_t *owner, const thread_t *current)
{
	thread_t *t = owner, *ghost = NULL;
	unsigned int depth;

	for (depth = 0; (depth < BWI_MAX_CHAIN_DEPTH) && (t != NULL) && (t != current); depth++) {
		if (t->blockedKind == WAIT_REAP) {
			ghost = t;
		}
		t = _sc_blockedOn(t);
	}

	if ((t == current) && (ghost != NULL)) {
		_sc_reclaim(ghost);
	}
}


void _threads_ghostAdd(thread_t *t)
{
	thread_t *r;

	/* A call the kernel made to t cannot wait until t is reaped */
	while ((r = _threads_kernelCaller(t)) != NULL) {
		_proc_clearReplyChain(r);
		_proc_setCallerMsgReturn(t, r, -EPIPE);
	}

	t->state = GHOST;

	LIB_ASSERT(t->scActive != NULL, "ghosting a thread with no SC");
	LIB_ASSERT(t->blockedKind == WAIT_NOTHING, "ghosting a blocked thread, kind %u", t->blockedKind);

	(void)_readyUnlink(t);
	LIST_ADD_EX(&threads_common.ghosts, t, gnext, gprev);

	/* t needs nothing itself any more, only the threads waiting on it do */
	t->scOwn->priority = MAX_PRIO;
	_threads_ghostBlockOn(t, threads_common.reaperThread);

	(void)_proc_threadWakeup(&threads_common.reaper);
}


static u8 _readyMin(void)
{
	return (threads_common.readyNonempty != 0ULL) ? __builtin_ctzll(threads_common.readyNonempty) : NPRIOS;
}


void _proc_setCallerMsgReturn(thread_t *recv, thread_t *caller, int retval)
{
	LIB_ASSERT(_thread_called(caller) == NULL, "reply chain not cleared before returning a message");

	_sc_return(recv, caller);
	caller->state = READY;

	if (caller->onReady == 0U) {
		_threads_readyAdd(caller);
	}

	if (caller->callReturnable == 0) {
		trace_eventSyscallExit(syscall_msgSend, proc_getTid(caller));
		caller->context = _threads_getUserContext(caller);
	}
	else {
		/* the kernel context saved in proc_send_ex(), its hal_cpuReschedule() returns retval */
		caller->callReturnable = 0;
	}
	hal_cpuSetReturnValue(caller->context, (void *)(ptr_t)retval);

	/* recv is blocked if it is a ghost answering from thread_destroy() */
	LIB_ASSERT((recv->scActive != NULL) || (recv->scPushed != NULL), "recv sched null?");
	LIB_ASSERT(caller->state == READY, "caller should be ready!");
	LIB_ASSERT((recv->scActive == NULL) || (recv->scActive->t == recv), "badly linked sched context");
}


__attribute__((noreturn)) void threads_halt(void)
{
	spinlock_ctx_t sc;

	/*
	 * Take the threads spinlock and not release it - this is an attempt to stop other cores in SMP from
	 * continuing to execute code (they should eventually hang trying to run the scheduler).
	 */
	hal_spinlockSet(&threads_spinlock, &sc);
	for (;;) {
		hal_cpuHalt();
	}

	__builtin_unreachable();
}


static void thread_destroy(thread_t *thread)
{
	process_t *process;
	spinlock_ctx_t sc;
	thread_t *reply, *current;

	trace_eventThreadEnd(thread);

	hal_spinlockSet(&threads_spinlock, &sc);
	current = _proc_current();

	if (_thread_called(thread) != NULL) {
		_sc_reclaim(thread);
		_proc_clearReplyChain(thread);
	}

	/* the last reference may be dropped by someone other than the reaper */
	_threads_ghostBlockOn(thread, current);
	_threads_setCurrent(current);

	hal_spinlockClear(&threads_spinlock, &sc);

	/* No need to protect thread->locks access with threads_spinlock */
	/* The destroyed thread is a ghost and no thread (except for the current one) can access it */
	while (thread->locks != NULL) {
		proc_lockForceUnlock(thread->locks, UNLOCK_DO_YIELD);
	}

	xfer_abort(thread);

	/* REVISIT: guard with threads spinlock needed? called may hold a reference to us */
	hal_spinlockSet(&threads_spinlock, &sc);

	/* may still be sleeping on a port's queue */
	_proc_threadUnlink(thread);

	/* SCs donated by waiting senders belong to threads that still wait */
	_portDropBoosts(thread);
	_portBusyRemove(thread);

	if ((thread->granted != NULL) && (thread->granted->busyPort != NULL)) {
		/* killed before using the receiver it was given, give it to someone else */
		_portReturnReceiver(thread->granted->busyPort, thread->granted);
	}
	thread->granted = NULL;

	/* every caller we still owe a response gets -EPIPE */
	while (thread->reply != NULL) {
		reply = thread->reply;

		LIB_ASSERT(reply != thread, "thread replies to itself?");
		LIB_ASSERT(reply->scActive == NULL, "reply has an active sc?");

		hal_spinlockClear(&threads_spinlock, &sc);

		/*
		 * Release reply buffers before waking the reply. Safe to
		 * be done without spinlock when done before _setCallerMsgReturn().
		 * The reply's SC reaches us through the ghost meanwhile.
		 */
		xfer_abort(reply);

		hal_spinlockSet(&threads_spinlock, &sc);

		_proc_clearReplyChain(reply);
		_proc_setCallerMsgReturn(thread, reply, -EPIPE);
		_threads_setCurrent(current);

		if (reply->priority < current->priority) {
			(void)hal_cpuReschedule(&threads_spinlock, &sc);
			hal_spinlockSet(&threads_spinlock, &sc);
		}
	}

	/* scOwn is freed below, so it must not be left donated to someone */
	_sc_reclaim(thread);
	_threads_setCurrent(current);

	hal_spinlockClear(&threads_spinlock, &sc);


	LIB_ASSERT(thread->scDonated == NULL, "all donations should be reclaimed");
	LIB_ASSERT(thread->scOwn->t == thread, "own SC still donated?");
	vm_kfree(thread->scOwn);
	vm_kfree(thread->kstack);

	xfer_releaseIpcBuf(thread);

	process = thread->process;
	if (process != NULL) {
		hal_spinlockSet(&threads_spinlock, &sc);

		LIST_REMOVE_EX(&process->threads, thread, procnext, procprev);
		LIST_ADD_EX(&process->ghosts, thread, procnext, procprev);
		(void)_proc_threadBroadcast(&process->reaper);

		hal_spinlockClear(&threads_spinlock, &sc);
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

	hal_spinlockSet(&threads_spinlock, &sc);
	if (proc->threads == NULL) {
		tid = -1;
	}
	else {
		tid = proc_getTid(proc->threads);
	}
	hal_spinlockClear(&threads_spinlock, &sc);

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

	hal_spinlockSet(&threads_spinlock, &sc);
	current = _proc_current();
	current->longjmpctx = ctx;
	(void)hal_cpuReschedule(&threads_spinlock, &sc);
	for (;;) {
	}
}


static int _threads_checkSignal(thread_t *selected, process_t *proc, cpu_context_t *signalCtx, unsigned int oldmask, const int src);


/* assumes the aspace of t */
static void _threads_copyMsgBufResponse(thread_t *t)
{
	_xfer_copyOut(t);

	t->ipc.msgPtr->o.size = t->ipc.reply.size;
	hal_memcpy(t->ipc.msgPtr->o.raw, t->ipc.reply.raw, MSG_RAW_SIZE);
	t->ipc.msgPtr->o.err = t->ipc.reply.err;

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

	if (selected->ipc.call.replied != 0U) {
		_threads_copyMsgBufResponse(selected);
		selected->ipc.call.replied = 0U;
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


cpu_context_t *_threads_switchTo(thread_t *dest)
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

	/* pending signals are delivered when dest exits through ctx, see threads_setupUserReturn() */
	ctx = _threads_getUserContext(dest);

	if ((void *)dest->tls.tls_base != NULL) {
		hal_cpuTlsSet(&dest->tls, ctx);
	}

	LIB_ASSERT(dest->exit == 0, "switching to exiting thread");
	LIB_ASSERT(dest->scActive != NULL, "dest shed is null");

	threads_common.currentSc[hal_cpuGetID()] = dest->scActive;
	threads_common.currentThread[hal_cpuGetID()] = dest;

	LIB_ASSERT(_proc_current() == dest, "we are not recv?");
	LIB_ASSERT(_proc_current()->scActive != NULL, "proc current unschedulable?");

	_threads_scheduling(dest);

	return ctx;
}


static u8 _proc_threadCeilingPriority(thread_t *t)
{
	u8 prio = MAX_PRIO, c;
	lock_t *lock = t->locks;
	size_t locks = 0;

	while (locks < t->prioceilingLocks) {
		if (lock->attr.protocol == PH_LOCK_PROTO_PRIOCEILING) {
			c = __atomic_load_n(&lock->attr.prioceiling, __ATOMIC_RELAXED);
			if (c < prio) {
				prio = c;
			}
			locks++;
		}
		lock = lock->next;

		if (lock == t->locks) {
			break;
		}
	}

	LIB_ASSERT(locks == t->prioceilingLocks, "prioceilingLocks miscounted");

	return prio;
}


void _threads_setActiveSc(thread_t *t, sched_context_t *sc)
{
	int queued = _readyUnlink(t);
	int prio;

	/* it would go back on the ready queue with nothing to run, see _readyAdd() */
	LIB_ASSERT((sc != NULL) || (queued == 0), "clearing scActive of a queued thread: tid=%d st=%u ex=%u",
			proc_getTid(t), t->state, t->exit);

	t->scActive = sc;

	if (t->scActive != NULL) {
		t->scActive->t = t;

		prio = min((int)t->scActive->priority, (int)_proc_threadCeilingPriority(t));
		_portSenderRequeue(t, (u8)prio);

		/*
		 * WARN: t->priorityBase must never be derived from scActive->priorityBase.
		 * scActive can be a donated SC, and messing t's own permanent base with the
		 * donor's would leave t unable to revert to its original base.
		 */
		t->priority = (unsigned int)prio;
	}

	_readyRelink(t, queued);
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
			 * Current would exit the kernel with fastpathed ctx, so use it instead.
			 * This is an optimization - we could save the kernel context as
			 * current->context, but we know it is on the exiting path to switch to
			 * fastpathExitCtx.
			 */
			current->context = current->fastpathExitCtx;
			current->fastpathExitCtx = NULL;

			/* see note in proc_send_ex */
			if (current->saveCtxIn != NULL) {
				current->saveCtxIn->context = context;
				current->saveCtxIn = NULL;
			}
		}

		if (current->state == READY) {
			_threads_readyAdd(current);
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
		sched->t->onReady = 0;

		LIB_ASSERT(sched->t->scActive != NULL, "sched points to unschedulable thread");

		selected = sched->t;

		if (selected->exit == 0U) {
			break;
		}

		if ((hal_cpuSupervisorMode(selected->context) != 0) && (selected->exit < THREAD_END_NOW)) {
			break;
		}

		_threads_ghostAdd(selected);
		selected = NULL;

		/* that may have been the last thread of priority i */
		i = _readyMin();
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

	hal_spinlockSet(&threads_spinlock, &sc);
	ret = _threads_schedule(n, context, arg);
	hal_spinlockClear(&threads_spinlock, &sc);

	return ret;
}


thread_t *_proc_current(void)
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

	hal_spinlockSet(&threads_spinlock, &sc);

	t->ustack = ustack;
	if (t->ustack != NULL) {
		hal_memcpy(t->ustack, threads_common.stackCanary, sizeof(threads_common.stackCanary));
	}

	hal_spinlockClear(&threads_spinlock, &sc);
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
	t->exit = 0;
	t->execdata = NULL;
	t->wait = NULL;
	t->locks = NULL;
	t->prioceilingLocks = 0;
	t->blockedOn = NULL;
	t->blockedKind = WAIT_NOTHING;
	t->boosters = NULL;
	t->boostNext = NULL;
	t->boostPrev = NULL;
	t->scPushed = NULL;
	t->waitPort = NULL;
	t->busyPort = NULL;
	t->bnext = NULL;
	t->bprev = NULL;
	t->granted = NULL;
	t->longjmpctx = NULL;
	hal_memset(&t->ipc, 0, sizeof(t->ipc));

	t->fastpathExitCtx = NULL;
	t->callReturnable = 0;
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
	t->scOwn->priority = priority;
	t->scOwn->priorityBase = priority;
	t->scActive = t->scOwn;
	t->scDonated = NULL;

	t->reply = NULL;
	t->replyNext = NULL;
	t->replyPrev = NULL;
	t->saveCtxIn = NULL;
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
		hal_spinlockSet(&threads_spinlock, &sc);

		LIST_ADD_EX(&process->threads, t, procnext, procprev);
	}
	else {
		hal_spinlockSet(&threads_spinlock, &sc);
	}

	trace_eventThreadCreate(t);

	/* Insert thread to scheduler queue */

	_threads_waking(t);
	_threads_readyAdd(t);

	hal_spinlockClear(&threads_spinlock, &sc);

	return EOK;
}


static u8 _proc_threadGetPriority(thread_t *thread)
{
	u8 ceiling = _proc_threadCeilingPriority(thread);
	u8 scPrio = (thread->scActive != NULL) ? thread->scActive->priority : thread->priorityBase;
	return (ceiling < scPrio) ? ceiling : scPrio;
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
	_portSenderRequeue(thread, priority);
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

	hal_spinlockSet(&threads_spinlock, &sc);

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
			/*
			 * Set priority only if it won't degrade the priority boost (either from
			 * SC donation or from prio ceiling)
			 */
			if ((priority <= _proc_threadCeilingPriority(t)) && (t->scActive == t->scOwn)) {
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
		(void)hal_cpuReschedule(&threads_spinlock, &sc);
	}
	else {
		(void)hal_spinlockClear(&threads_spinlock, &sc);
	}

	trace_eventThreadPriority(proc_getTid(t), t->priority);

	return ret;
}


/* Assumes threads_spinlock is set. */
void _wakeReceiver(thread_t *t)
{
	LIB_ASSERT((t->state == BLOCKED_ON_RECV) && (t->waitPort != NULL), "not waiting for a message");
	_threads_setActiveSc(t, _sc_best(t));
	_proc_threadDequeue(t);
}


static void _thread_interrupt(thread_t *t)
{
	LIB_ASSERT(t->state != READY, "interrupting a ready thread?");

	if (t->state == BLOCKED_ON_RECV) {
		_wakeReceiver(t);
		t->ipcInterrupted = 1;
	}
	else {
		/* _proc_threadDequeue() needs a schedulable thread */
		_sc_reclaim(t);

		LIB_ASSERT(t->scActive != NULL, "interrupting an unschedulable thread?");
		_proc_threadDequeue(t);
	}

	hal_cpuSetReturnValue(t->context, (void *)-EINTR);
}


__attribute__((noreturn)) void proc_threadEnd(void)
{
	thread_t *t;
	int cpu;
	spinlock_ctx_t sc;

	(void)hal_spinlockSet(&threads_spinlock, &sc);

	cpu = (int)hal_cpuGetID();
	t = threads_common.currentThread[cpu];
	threads_common.currentSc[cpu] = NULL;
	threads_common.currentThread[cpu] = NULL;

	_threads_setActiveSc(t, t->scOwn);
	_threads_ghostAdd(t);

	(void)hal_cpuReschedule(&threads_spinlock, &sc);

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
		hal_spinlockSet(&threads_spinlock, &sc);
		_proc_threadExit(t);
		hal_spinlockClear(&threads_spinlock, &sc);
	}
}


void proc_threadsDestroy(thread_t **threads, const thread_t *except)
{
	thread_t *t;
	spinlock_ctx_t sc;

	hal_spinlockSet(&threads_spinlock, &sc);
	t = *threads;
	if (t != NULL) {
		do {
			if (t != except) {
				_proc_threadExit(t);
			}
			t = t->procnext;
		} while (t != *threads);
	}
	hal_spinlockClear(&threads_spinlock, &sc);
}


void proc_reap(void)
{
	spinlock_ctx_t sc;
	thread_t *t = NULL;
	port_t *p = NULL;

	hal_spinlockSet(&threads_spinlock, &sc);

	threads_common.reaperThread = _proc_current();

	while ((threads_common.ghosts == NULL) && (threads_common.ghostPorts == NULL)) {
		(void)_proc_threadWait(&threads_common.reaper, 0, &sc);
	}

	if (threads_common.ghosts != NULL) {
		t = threads_common.ghosts;
		LIST_REMOVE_EX(&threads_common.ghosts, t, gnext, gprev);
	}

	if (threads_common.ghostPorts != NULL) {
		p = threads_common.ghostPorts;
		LIST_REMOVE_EX(&threads_common.ghostPorts, p, gnext, gprev);
	}

	hal_spinlockClear(&threads_spinlock, &sc);

	if (p != NULL) {
		port_free(p);
	}

	if (t != NULL) {
		threads_put(t);
	}
}


void proc_changeMap(process_t *proc, vm_map_t *map, vm_map_t *imap, pmap_t *pmap)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&threads_spinlock, &sc);
	proc->mapp = map;
	proc->pmapp = pmap;
	proc->imapp = imap;
	hal_spinlockClear(&threads_spinlock, &sc);
}


/*
 * Sleeping and waiting
 */

/* Remove t from its wait queue and the sleeping tree without making it runnable. */
void _proc_threadUnlink(thread_t *t)
{
	port_t *p;

	if (t->wait != NULL) {
		LIST_REMOVE_EX(t->wait, t, qnext, qprev);
		t->wait = NULL;
	}

	/* waitPort must not outlive the queue, see _portSenderRequeue() */
	p = t->waitPort;
	if (p != NULL) {
		t->waitPort = NULL;
		if (port_release(p) == 0) {
			LIST_ADD_EX(&threads_common.ghostPorts, p, gnext, gprev);
			(void)_proc_threadBroadcast(&threads_common.reaper);
		}
	}

	if (t->wakeup != 0) {
		lib_rbRemove(&threads_common.sleeping, &t->sleeplinkage);
		t->wakeup = 0;
	}
}


void _proc_threadDequeue(thread_t *t)
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
		_threads_readyAdd(t);
	}
}


void _proc_threadEnqueueThread(thread_t *t, thread_t **queue, u8 state, time_t timeout, int interruptible)
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

	err = hal_cpuReschedule(&threads_spinlock, scp);
	(void)hal_spinlockSet(&threads_spinlock, scp);

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

	return hal_cpuReschedule(&threads_spinlock, sc);
}


static int _proc_threadSleep(time_t us, time_t now, spinlock_ctx_t *sc)
{
	return _proc_threadSleepAbs(now + us, now, sc);
}


int proc_threadSleep(time_t us)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&threads_spinlock, &sc);
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

	hal_spinlockSet(&threads_spinlock, &sc);

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

	hal_spinlockSet(&threads_spinlock, &tsc);

	if (((flags & THREAD_WAIT_INTERRUPTIBLE) != 0U) && (_proc_current()->exit != 0U)) {
		/* Waiting in this state can lead to becoming a hanging zombie */
		hal_spinlockClear(&threads_spinlock, &tsc);
		return -EINTR;
	}

	if (((flags & THREAD_WAIT_EXCLUSIVE) != 0U) && (*queue != NULL) && (*queue != wakeupPending)) {
		hal_spinlockClear(&threads_spinlock, &tsc);
		return -EBUSY;
	}

	_proc_threadEnqueue(queue, timeout, ((flags & THREAD_WAIT_INTERRUPTIBLE) != 0U) ? 1U : 0U);

	if (*queue == NULL) {
		hal_spinlockClear(&threads_spinlock, &tsc);
		return EOK;
	}

	if (spinlock != NULL) {
		/* tsc and scp are swapped intentionally, we need to enable interrupts */
		hal_spinlockClear(spinlock, &tsc);
	}
	err = hal_cpuReschedule(&threads_spinlock, rescheduleScp);
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

	hal_spinlockSet(&threads_spinlock, &sc);
	ret = _proc_threadWakeup(queue);
	hal_spinlockClear(&threads_spinlock, &sc);
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

	hal_spinlockSet(&threads_spinlock, &sc);
	ret = _proc_threadBroadcast(queue);
	hal_spinlockClear(&threads_spinlock, &sc);

	return ret;
}


void proc_threadWakeupYield(thread_t **queue)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&threads_spinlock, &sc);
	if (_proc_threadWakeup(queue) != 0) {
		(void)hal_cpuReschedule(&threads_spinlock, &sc);
	}
	else {
		hal_spinlockClear(&threads_spinlock, &sc);
	}
}


void proc_threadBroadcastYield(thread_t **queue)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&threads_spinlock, &sc);
	if (_proc_threadBroadcast(queue) != 0) {
		(void)hal_cpuReschedule(&threads_spinlock, &sc);
	}
	else {
		hal_spinlockClear(&threads_spinlock, &sc);
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

	hal_spinlockSet(&threads_spinlock, &sc);

	now = _proc_gettimeRaw();
	current = _proc_current();
	if (proc_getTid(current) == tid) {
		hal_spinlockClear(&threads_spinlock, &sc);
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
	hal_spinlockClear(&threads_spinlock, &sc);

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

	hal_spinlockSet(&threads_spinlock, &sc);
	if (raw != NULL) {
		(*raw) = _proc_gettimeRaw();
	}
	if (offs != NULL) {
		(*offs) = threads_common.utcoffs;
	}
	hal_spinlockClear(&threads_spinlock, &sc);
}


int proc_settime(time_t offs)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&threads_spinlock, &sc);
	threads_common.utcoffs = offs;
	hal_spinlockClear(&threads_spinlock, &sc);

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

	hal_spinlockSet(&threads_spinlock, &sc);

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
			hal_spinlockClear(&threads_spinlock, &sc);
			return -ESRCH;
		}
	}

	(void)hal_cpuReschedule(&threads_spinlock, &sc);

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

	hal_spinlockSet(&threads_spinlock, &sc);
	thread = _proc_current();
	if (thread->fastpathExitCtx == NULL) {
		kstackTop = thread->kstack + thread->kstacksz;
		signalCtx = (void *)((char *)hal_cpuGetUserSP(ctx) - sizeof(*signalCtx));
		hal_cpuSetReturnValue(ctx, retval);

		if (_threads_checkSignal(thread, thread->process, signalCtx, thread->sigmask, SIG_SRC_SCALL) == 0) {
			/* parasoft-suppress-next-line MISRAC2012-RULE_11_1 "f is passed to function hal_jmp which need void * type" */
			f = thread->process->sighandler;
			hal_spinlockClear(&threads_spinlock, &sc);
			hal_jmp(f, kstackTop, hal_cpuGetUserSP(signalCtx), 0, NULL);
			/* no return */
		}
	}
	else {
		fpCtx = thread->fastpathExitCtx;
		thread->fastpathExitCtx = NULL;

		/* thread is a receiver woken by the fastpath, it gets its signals here instead of in the scheduler */
		if (thread->process != NULL) {
			signalCtx = (void *)((char *)hal_cpuGetUserSP(fpCtx) - sizeof(*signalCtx));
			if (_threads_checkSignal(thread, thread->process, signalCtx, thread->sigmask, SIG_SRC_SCALL) == 0) {
				/* parasoft-suppress-next-line MISRAC2012-RULE_11_1 "f is passed to function hal_jmp which need void * type" */
				f = thread->process->sighandler;
				kstackTop = thread->kstack + thread->kstacksz;
				hal_spinlockClear(&threads_spinlock, &sc);
				hal_jmp(f, kstackTop, hal_cpuGetUserSP(signalCtx), 0, NULL);
				/* no return */
			}
		}

		hal_spinlockClear(&threads_spinlock, &sc);
		/* FIXME: race with sched is possible here */
		hal_endSyscall(fpCtx, &sc);
	}

	hal_spinlockClear(&threads_spinlock, &sc);
}


int threads_sigsuspend(unsigned int mask)
{
	thread_t *thread;
	spinlock_ctx_t sc;
	cpu_context_t *ctx, *signalCtx;
	void *kstackTop, *f;
	unsigned int oldmask;


	/* changing sigmask and sleep shall be atomic - do it under lock (sigpost is done also under threads_spinlock) */
	hal_spinlockSet(&threads_spinlock, &sc);
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
		hal_spinlockClear(&threads_spinlock, &sc);
		hal_jmp(f, kstackTop, hal_cpuGetUserSP(signalCtx), 0, NULL);
		/* no return */
	}

	/* Sleep forever (atomic lock release), interruptible */
	thread_t *tqueue = NULL;
	_proc_threadEnqueue(&tqueue, 0, 1);
	(void)hal_cpuReschedule(&threads_spinlock, &sc);
	/* after wakeup */

	/* check for pending signals before restoring the old mask */
	hal_spinlockSet(&threads_spinlock, &sc);
	if (_threads_checkSignal(thread, thread->process, signalCtx, oldmask, SIG_SRC_SCALL) == 0) {
		/* parasoft-suppress-next-line MISRAC2012-RULE_11_1 "f is passed to function hal_jmp which need void * type" */
		f = thread->process->sighandler;
		hal_spinlockClear(&threads_spinlock, &sc);
		hal_jmp(f, kstackTop, hal_cpuGetUserSP(signalCtx), 0, NULL);
		/* no return */
	}

	/* interrupted by signal but no sighandler installed */
	thread->sigmask = oldmask;
	hal_spinlockClear(&threads_spinlock, &sc);

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


/* Assumes lock->spinlock and threads_spinlock are set. */
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
	if (lock->attr.protocol == PH_LOCK_PROTO_PRIOCEILING) {
		current->prioceilingLocks++;
	}

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
	hal_spinlockSet(&threads_spinlock, &tcsc);

	current = _proc_current();

	err = _proc_lockTry(current, lock);

	hal_spinlockClear(&threads_spinlock, &tcsc);
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
			 * critical section of threads_spinlock to just the _proc_threadEnqueue()?
			 */
			hal_spinlockClear(&lock->spinlock, sc);
			err = hal_cpuReschedule(&threads_spinlock, scp);
			hal_spinlockSet(&lock->spinlock, scp);
			hal_spinlockSet(&threads_spinlock, sc);
		}

		if (lock->owner == current) {
			return EOK;
		}
	}
}


/* protocol-unaware variant of _proc_lockSet() */
static int _proc_lockSetRaw(lock_t *lock, u8 interruptible, spinlock_ctx_t *scp)
{
	thread_t *current;
	spinlock_ctx_t sc;
	int ret = EOK, tid;

	hal_spinlockSet(&threads_spinlock, &sc);

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

	hal_spinlockClear(&threads_spinlock, &sc);

	_trace_eventLockSetExit(lock, tid, ret);
	return ret;
}


static int _proc_lockSet(lock_t *lock, u8 interruptible, spinlock_ctx_t *scp)
{
	thread_t *current;
	spinlock_ctx_t sc;
	int ret = EOK, tid;

	hal_spinlockSet(&threads_spinlock, &sc);

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
			_threads_ghostBreakCycle(lock->owner, current);
			donated = current->scActive;
			_thread_setBlocked(current, WAIT_LOCK, lock);
			_sc_donate(current, lock->owner, donated);
			LIB_ASSERT(current->scActive == NULL, "?");
		}

		ret = _proc_lockWaitWake(lock, current, interruptible, &sc, scp);
		if (ret == EOK) {
			_thread_clearBlocked(current, WAIT_LOCK);
			ret = _proc_lockObtained(current, lock);
		}
		else if (_thread_waitingOn(current) != NULL) {
			/* Could happen if exit != 0 on entry to _proc_lockWaitWake() */
			_sc_reclaim(current);
		}
		else {
			/* No action required, _thread_interrupt() reclaimed the SC and cleared waitingOn */
		}
	}

	hal_spinlockClear(&threads_spinlock, &sc);

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


/* Assumes lock->spinlock and threads_spinlock are set. */
static int _proc_lockUnlock(lock_t *lock, int doForceUnlock)
{
	thread_t *owner = lock->owner, *current;
	int ret = 0;

	current = _proc_current();

	_trace_eventLockClear(lock, proc_getTid(current));

	LIB_ASSERT(LIST_BELONGS(&owner->locks, lock) != 0,
			"lock: %s, owner pid: %d, owner tid: %d, lock is not on the list "
			"(current pid: %d, current tid: %d, doForceUnlock: %d, queue empty: %d, "
			"owner scActive: %p, owner waitingOn: %p, current waitingOn: %p)",
			lock->name, (owner->process != NULL) ? process_getPid(owner->process) : 0, proc_getTid(owner),
			(current->process != NULL) ? process_getPid(current->process) : 0, proc_getTid(current), doForceUnlock,
			(lock->queue == NULL), (void *)owner->scActive, (void *)_thread_waitingOn(owner),
			(void *)_thread_waitingOn(current));

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

	if (lock->attr.protocol == PH_LOCK_PROTO_PRIOCEILING) {
		owner->prioceilingLocks--;
	}

	if (lock->queue != NULL) {
		/* Transfer lock to the first waiter */
		lock->owner = lock->queue;

		if (lock->attr.protocol == PH_LOCK_PROTO_INHERIT) {
			/*
			 * Must clear first: the new owner now waits for a lock it owns, so
			 * _sc_blockedOn() would answer with the thread itself.
			 */
			_thread_clearBlocked(lock->owner, WAIT_LOCK);

			_sc_return(owner, lock->owner);

			if (lock->queue->qnext != lock->queue) {
				/* Move donations to a new owner and recalculate new best SC */

				/* FIXME: this is not ideal, as it extends the critical section of spinlocks. Come up with a way to do this interruptibly */
				_sc_migrate(owner, lock->queue, lock->owner);
			}
		}

		/* Wake the new owner and add lock to its held-locks list */
		_proc_threadDequeue(lock->owner);
		LIST_ADD(&lock->owner->locks, lock);
		if (lock->attr.protocol == PH_LOCK_PROTO_PRIOCEILING) {
			lock->owner->prioceilingLocks++;
		}

		ret = 1;
	}
	else {
		lock->owner = NULL;
	}

	if (lock->attr.protocol == PH_LOCK_PROTO_PRIOCEILING) {
		_proc_threadSetPriority(owner, _proc_threadGetPriority(owner));
		if (ret == 1) {
			_proc_threadSetPriority(lock->owner, _proc_threadGetPriority(lock->owner));
		}
	}

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
		hal_spinlockSet(&threads_spinlock, &sc);
		ret = _proc_lockUnlock(lock, UNLOCK_FORCE);
		hal_spinlockClear(&threads_spinlock, &sc);
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

#ifndef NDEBUG
	thread_t *current = proc_current();
	/*
	 * Safe without threads_spinlock: every writer of lock->owner also
	 * requires lock->spinlock, already held by the caller.
	 */
	LIB_ASSERT(lock->owner != NULL, "lock: %s, pid: %d, tid: %d, unlock on not locked lock",
			lock->name, (current->process != NULL) ? process_getPid(current->process) : 0, proc_getTid(current));

	LIB_ASSERT(lock->owner == current, "lock: %s, pid: %d, tid: %d, owner pid: %d, owner tid: %d, unlocking someone else's lock",
			lock->name, (current->process != NULL) ? process_getPid(current->process) : 0, proc_getTid(current),
			(lock->owner->process != NULL) ? process_getPid(lock->owner->process) : 0, proc_getTid(lock->owner));
#endif

	if (lock->owner == NULL) {
		return -EPERM;
	}

	hal_spinlockSet(&threads_spinlock, &sc);
	ret = _proc_lockUnlock(lock, UNLOCK_TRY);
	hal_spinlockClear(&threads_spinlock, &sc);

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
			hal_spinlockSet(&threads_spinlock, &sc);
			wakeup = _proc_nextWakeup();

			if (wakeup > (2 * SYSTICK_INTERVAL)) {
				hal_cpuLowPower(wakeup, &threads_spinlock, &sc);
				continue;
			}
			hal_spinlockClear(&threads_spinlock, &sc);
		}
		hal_cpuHalt();
	}
}


void proc_threadsDump(u8 priority)
{
	sched_context_t *sched;
	spinlock_ctx_t sc;

	/* Strictly needed - no lock can be taken
	 * while threads_spinlock is being
	 * held! */
	log_disable();

	lib_printf("threads: ");
	hal_spinlockSet(&threads_spinlock, &sc);

	sched = threads_common.ready[priority];
	do {
		if (sched == NULL) {
			break;
		}

		lib_printf("[%p] ", sched->t);

		sched = sched->next;
	} while (sched != threads_common.ready[priority]);
	hal_spinlockClear(&threads_spinlock, &sc);

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

		hal_spinlockSet(&threads_spinlock, &sc);
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
		hal_spinlockClear(&threads_spinlock, &sc);

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

	hal_spinlockSet(&threads_spinlock, &sc);
	/* Assumes t is not NULL and belongs to a process */
	ret = (t->procnext != t) ? 1 : 0;
	hal_spinlockClear(&threads_spinlock, &sc);

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

	hal_spinlockSet(&threads_spinlock, &sc);
	priority = (int)t->priority;
	priorityBase = (int)t->priorityBase;
	hal_spinlockClear(&threads_spinlock, &sc);

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

	threads_common.kmap = kmap;
	threads_common.ghosts = NULL;
	threads_common.reaperThread = NULL;
	threads_common.ghostPorts = NULL;
	threads_common.reaper = NULL;
	threads_common.utcoffs = 0;
	threads_common.idcounter = 0;
	threads_common.prev = 0;

	(void)proc_lockInit(&threads_common.lock, &proc_lockAttrDefault, "threads.common");

	for (i = 0U; i < sizeof(threads_common.stackCanary); ++i) {
		threads_common.stackCanary[i] = ((i & 1U) != 0U) ? 0xaaU : 0x55U;
	}

	/* Initialize scheduler queues */
	for (i = 0; i < sizeof(threads_common.ready) / sizeof(thread_t *); i++) {
		threads_common.ready[i] = NULL;
	}
	threads_common.readyNonempty = 0;

	lib_rbInit(&threads_common.sleeping, threads_sleepcmp, NULL);
	lib_idtreeInit(&threads_common.id);

	lib_printf("proc: Initializing thread scheduler, priorities=%d\n", sizeof(threads_common.ready) / sizeof(thread_t *));

	hal_spinlockCreate(&threads_spinlock, "threads.spinlock");

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

	_msg_init(kmap, kernel);

	hal_memset(&threads_common.timeintrHandler, 0, sizeof(threads_common.timeintrHandler));
	(void)hal_timerRegister(threads_timeintr, NULL, &threads_common.timeintrHandler);

	return EOK;
}
