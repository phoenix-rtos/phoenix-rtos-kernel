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


const struct lockAttr proc_lockAttrDefault = { .type = PH_LOCK_NORMAL };

/* Special empty queue value used to wakeup next enqueued thread. This is used to implement sticky conditions */
static thread_t *const wakeupPending = (void *)-1;

/* clang-format off */
enum { event_scheduling, event_enqueued, event_waking, event_preempted };
/* clang-format on */

struct {
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

	sched_context_t *volatile ghosts;
	thread_t *reaper;

	/* Debug */
	unsigned char stackCanary[16];
	time_t prev;
} threads_common;


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

	LIB_ASSERT_ALWAYS(t->sched != NULL, "attempted to update unschedulable thread (type=%d)", type);

	if (type == event_waking || type == event_preempted) {
		t->sched->readyTime = now;
	}
	else if (type == event_scheduling) {
		wait = now - t->sched->readyTime;

		if (t->sched->maxWait < wait)
			t->sched->maxWait = wait;
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


static void _threads_updateWakeup(time_t now, thread_t *min)
{
	thread_t *t;
	time_t wakeup;

	if (min != NULL)
		t = min;
	else
		t = lib_treeof(thread_t, sleeplinkage, lib_rbMinimum(threads_common.sleeping.root));

	if (t != NULL) {
		if (now >= t->wakeup)
			wakeup = 1;
		else
			wakeup = t->wakeup - now;
	}
	else {
		wakeup = SYSTICK_INTERVAL;
	}

	if (wakeup > SYSTICK_INTERVAL + SYSTICK_INTERVAL / 8)
		wakeup = SYSTICK_INTERVAL;

	hal_timerSetWakeup(wakeup);
}


int threads_timeintr(unsigned int n, cpu_context_t *context, void *arg)
{
	thread_t *t;
	time_t now;
	spinlock_ctx_t sc;

	if (hal_cpuGetID() != 0) {
		/* Invoke scheduler */
		return 1;
	}

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


static void proc_lockUnlock(lock_t *lock);


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


static void unbindFromAddedTo(thread_t *t)
{
	port_t *p;
	spinlock_ctx_t sc;

	if (t->addedTo != NULL) {
		p = t->addedTo;
		hal_spinlockSet(&p->spinlock, &sc);
		LIST_REMOVE_EX(&p->fpThreads, t, tnext, tprev);
		/*
		 * TODO: clear refcount, but cant use port_put here as it potentially
		 * sets threads_common.spinlock...
		 */
		hal_spinlockClear(&p->spinlock, &sc);
		t->addedTo = NULL;
	}
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
		proc_lockUnlock(thread->locks);
	}

	unbindFromAddedTo(thread);
	proc_freeUtcb(thread);
	threads_releaseIpcBuffers(thread);

	/* REVISIT: guard with threads spinlock needed? called may hold a reference to us */
	hal_spinlockSet(&threads_common.spinlock, &sc);
	if (thread->called != NULL) {
		LIB_ASSERT(thread->called->reply == thread, "thread->called->reply != thread");
		thread->called->reply = NULL;
	}

	LIB_ASSERT(thread->inherited == NULL || thread->inherited == thread->inherited->next, "heh, inherited");

	if (thread->sched != NULL) {
		if (thread->reply != NULL) {
			LIB_ASSERT(thread->reply != thread, "thread replies to itself????");
			reply = thread->reply;

			LIB_ASSERT(reply->sched == NULL, "reply has... sched?");
			reply->sched = thread->sched;
			reply->sched->t = reply;
			reply->called = NULL;

			LIB_ASSERT(reply->exit == 0, "reply thread exiting?");

			if (reply->callReturnable == 0) {
				reply->context = _getUserContext(reply);
				hal_cpuSetReturnValue(reply->context, (void *)(ptr_t)-EINVAL);
			}
			else {
				reply->callReturnable = 0;
			}
			// #ifndef NOMMU
			// 			LIB_ASSERT((ptr_t)hal_cpuGetIP(reply->context) < VADDR_KERNEL, "dest ip in kernel - ip: 0x%p tid: %d", hal_cpuGetIP(reply->context), proc_getTid(reply));
			// #endif
			_proc_threadDequeue(reply);
			hal_spinlockClear(&threads_common.spinlock, &sc);
		}
		else {
			hal_spinlockClear(&threads_common.spinlock, &sc);
			vm_kfree(thread->sched);
		}
	}
	else {
		hal_spinlockClear(&threads_common.spinlock, &sc);
	}
	vm_kfree(thread->kstack);

	process = thread->process;
	if (process != NULL) {
		hal_spinlockSet(&threads_common.spinlock, &sc);

		LIST_REMOVE_EX(&process->threads, thread, procnext, procprev);
		LIST_ADD_EX(&process->ghosts, thread, procnext, procprev);
		_proc_threadBroadcast(&process->reaper);

		hal_spinlockClear(&threads_common.spinlock, &sc);
		proc_put(process);
	}
	else {
		vm_kfree(thread);
	}
}


thread_t *threads_findThread(int tid)
{
	thread_t *t;

	proc_lockSet(&threads_common.lock);
	t = lib_idtreeof(thread_t, idlinkage, lib_idtreeFind(&threads_common.id, tid));
	if (t != NULL) {
		++t->refs;
	}
	proc_lockClear(&threads_common.lock);

	return t;
}


void threads_put(thread_t *thread)
{
	int refs;

	proc_lockSet(&threads_common.lock);
	refs = --thread->refs;
	if (refs <= 0) {
		lib_idtreeRemove(&threads_common.id, &thread->idlinkage);
	}
	proc_lockClear(&threads_common.lock);

	if (refs <= 0) {
		thread_destroy(thread);
	}
}


static void _threads_cpuTimeCalc(thread_t *current, thread_t *selected)
{
	time_t now = _proc_gettimeRaw();

	if (current != NULL && current->sched != NULL) {
		current->sched->cpuTime += now - current->sched->lastTime;
		current->sched->lastTime = now;
	}

	if (selected != NULL && current != selected) {
		LIB_ASSERT(selected->sched != NULL, "selected thread is unschedulable?");
		selected->sched->lastTime = now;
	}
}


__attribute__((noreturn)) void proc_longjmp(cpu_context_t *ctx)
{
	spinlock_ctx_t sc;
	thread_t *current;

	hal_spinlockSet(&threads_common.spinlock, &sc);
	current = _proc_current();
	current->longjmpctx = ctx;
	hal_cpuReschedule(&threads_common.spinlock, &sc);
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


static void _threads_switchToThread(cpu_context_t *context, thread_t *selected)
{
	process_t *proc;
	cpu_context_t *signalCtx, *selCtx;

	threads_common.current[hal_cpuGetID()] = selected->sched;
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

	if (selected->longjmpctx != NULL) {
		selCtx = selected->longjmpctx;
		selected->longjmpctx = NULL;
	}

	if (selected->tls.tls_base != NULL) {
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


/* TODO: replace manual refcnt decremenatations with appropriate calls to destroys */


static void _threads_switchSchedContexts(thread_t *from, thread_t *to, int reply)
{
	sched_context_t *sched = from->sched;

	LIB_ASSERT(sched != NULL, "sched null");
	LIB_ASSERT(to->sched == NULL,
			"dest sched not null (prio=%d, from=%d, dest=%d, reply=%d, sched owner tid=%d)",
			to->priority, proc_getTid(from), proc_getTid(to), reply, proc_getTid(to->sched->owner));

	/* TODO: if sched is always donated from `from`, just use it instead of explicit passing*/
	from->sched = NULL;
	if (reply != 0) {
		/* replying - going back in SC chain */
		LIB_ASSERT(from->reply != NULL, "reply null (initial from->state=%d)", from->state);

		/* TODO: this is tricky to track in case of msgRecv - the server could have
		 * several replies pending, so to doesnt neces.oily need to be from->reply */
		LIB_ASSERT(from->reply == to, "replying to bad thread: from: %d from->reply: %d to: %d", proc_getTid(from), proc_getTid(from->reply), proc_getTid(to));

		from->state = BLOCKED_ON_RECV;

		LIB_ASSERT(to->state == BLOCKED_ON_REPLY, "dest thread not blocked on reply? state=%d", to->state);
		from->reply = NULL;
		to->called = NULL;
	}
	else {
		/* calling - going deeper in SC chain */
		from->state = BLOCKED_ON_REPLY;

		LIB_ASSERT(to->state == BLOCKED_ON_RECV, "dest thread not blocked on recv? state=%d", to->state);
		to->reply = from;
		from->called = to;
	}
	sched->t = to;
	to->sched = sched;
	to->state = READY;
}


static cpu_context_t *_threads_switchTo(thread_t *dest, int reply)
{
	process_t *proc;
	cpu_context_t *ctx;
	thread_t *from = _proc_current();

	LIB_ASSERT(dest != NULL, "dest null");
	LIB_ASSERT(dest->exit == 0, "exit=%d", dest->exit);
	LIB_ASSERT(dest->state != GHOST, "dest is a ghost");

	_threads_switchSchedContexts(from, dest, reply);

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

	if (dest->tls.tls_base != NULL) {
		hal_cpuTlsSet(&dest->tls, ctx);
	}

	_threads_scheduling(dest);

	LIB_ASSERT(dest->sched != NULL, "dest shed is null");

	return ctx;
}


cpu_context_t *threads_switchTo(thread_t *dest, int reply)
{
	spinlock_ctx_t sc;
	cpu_context_t *ctx;

	hal_spinlockSet(&threads_common.spinlock, &sc);
	ctx = _threads_switchTo(dest, reply);
	hal_spinlockClear(&threads_common.spinlock, &sc);

	return ctx;
}


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

			/* see note in proc_call */
			if (current->saveCtxInReply != 0) {
				LIB_ASSERT(current->reply != NULL, "reply null?");
				current->reply->context = context;
				current->saveCtxInReply = 0;
			}
		}

		/* Move thread to the end of queue */
		if (current->state == READY) {
			LIB_ASSERT(current->sched != NULL, "READY but unschedulable? tid: %d, pc=%p, ra=%p", proc_getTid(current), current->context->sepc, current->context->ra);

			LIST_ADD(&threads_common.ready[current->priority], current->sched);
			_threads_preempted(current);
		}
	}

	/* Get next thread */
	for (i = 0; i < sizeof(threads_common.ready) / sizeof(thread_t *);) {
		sched = threads_common.ready[i];

		if (sched == NULL) {
			i++;
			continue;
		}

		LIB_ASSERT(sched->t != NULL, "dangling scheduling context");

		LIST_REMOVE(&threads_common.ready[i], sched);

		if (sched->t->state != READY) {
			/* lazy update */
			continue;
		}

		LIB_ASSERT(sched->t->sched != NULL, "sched points to unschedulable thread");

		selected = sched->t;

		if (selected->exit == 0) {
			break;
		}

		if ((hal_cpuSupervisorMode(selected->context) != 0) && (selected->exit < THREAD_END_NOW)) {
			break;
		}

		selected->state = GHOST;
		LIST_ADD(&threads_common.ghosts, sched);
		_proc_threadWakeup(&threads_common.reaper);
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
	LIB_ASSERT(sched == NULL || sched->t != NULL, "sched with no thread?");
	return sched == NULL ? NULL : sched->t;
}


thread_t *proc_current(void)
{
	thread_t *current;
	spinlock_ctx_t sc;

	hal_spinlockSet(&threads_common.spinlock, &sc);
	current = _proc_current();
	hal_spinlockClear(&threads_common.spinlock, &sc);

	return current;
}


static int thread_alloc(thread_t *thread)
{
	int id;

	proc_lockSet(&threads_common.lock);
	id = lib_idtreeAlloc(&threads_common.id, &thread->idlinkage, threads_common.idcounter);
	if (id < 0) {
		/* Try from the start */
		threads_common.idcounter = 0;
		id = lib_idtreeAlloc(&threads_common.id, &thread->idlinkage, threads_common.idcounter);
	}

	if (id >= 0) {
		if (threads_common.idcounter == MAX_TID) {
			threads_common.idcounter = 0;
		}
		else {
			threads_common.idcounter++;
		}
	}
	proc_lockClear(&threads_common.lock);

	return id;
}


void threads_canaryInit(thread_t *t, void *ustack)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&threads_common.spinlock, &sc);

	if ((t->ustack = ustack) != NULL)
		hal_memcpy(t->ustack, threads_common.stackCanary, sizeof(threads_common.stackCanary));

	hal_spinlockClear(&threads_common.spinlock, &sc);
}


int proc_threadCreate(process_t *process, void (*start)(void *), int *id, unsigned int priority, size_t kstacksz, void *stack, size_t stacksz, void *arg)
{
	thread_t *t;
	spinlock_ctx_t sc;
	int err;

	if (priority >= sizeof(threads_common.ready) / sizeof(thread_t *)) {
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
	t->longjmpctx = NULL;
	hal_memset(&t->utcb.iil, 0, sizeof(t->utcb.iil));
	hal_memset(&t->utcb.oil, 0, sizeof(t->utcb.oil));
	t->utcb.msg = NULL;

	t->fastpathExitCtx = NULL;
	t->callReturnable = 0;
	t->saveCtxInReply = 0;

	t->priorityBase = priority;
	t->priority = priority;
	t->inherited = NULL;
	t->sched = vm_kmalloc(sizeof(sched_context_t));
	if (t->sched == NULL) {
		vm_kfree(t->kstack);
		vm_kfree(t);
		return -ENOMEM;
	}
	t->sched->cpuTime = 0;
	t->sched->maxWait = 0;
	t->sched->t = t;
	t->sched->next = NULL;
	t->sched->prev = NULL;
	proc_gettime(&t->sched->startTime, NULL);
	t->sched->lastTime = t->sched->startTime;
	t->sched->owner = t;

	t->reply = NULL;
	t->called = NULL;
	t->addedTo = NULL;
	t->flags = 0;

	// t->bufferStart = NULL;
	// t->bufferEnd = NULL;
	t->mappedTo = NULL;
	// t->mappedBase = NULL;

	if (thread_alloc(t) < 0) {
		vm_kfree(t->sched);
		vm_kfree(t->kstack);
		vm_kfree(t);
		return -ENOMEM;
	}

	if (process != NULL && (process->tls.tdata_sz != 0 || process->tls.tbss_sz != 0)) {
		err = process_tlsInit(&t->tls, &process->tls, process->mapp);
		if (err != EOK) {
			lib_idtreeRemove(&threads_common.id, &t->idlinkage);
			vm_kfree(t->sched);
			vm_kfree(t->kstack);
			vm_kfree(t);
			return err;
		}
	}
	else {
		t->tls.tls_base = NULL;
		t->tls.tdata_sz = 0;
		t->tls.tbss_sz = 0;
		t->tls.tls_sz = 0;
		t->tls.arm_m_tls = NULL;
	}

	if (id != NULL) {
		*id = proc_getTid(t);
	}

	/* Prepare initial stack */
	hal_cpuCreateContext(&t->context, start, t->kstack, t->kstacksz, (stack == NULL) ? NULL : (unsigned char *)stack + stacksz, arg, &t->tls);
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
	LIST_ADD(&threads_common.ready[priority], t->sched);

	hal_spinlockClear(&threads_common.spinlock, &sc);

	return EOK;
}


static unsigned int _proc_lockGetPriority(lock_t *lock)
{
	unsigned int priority = sizeof(threads_common.ready) / sizeof(threads_common.ready[0]) - 1;
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


static unsigned int _proc_threadGetLockPriority(thread_t *thread)
{
	unsigned int ret, priority = sizeof(threads_common.ready) / sizeof(threads_common.ready[0]) - 1;
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


static unsigned int _proc_threadGetPriority(thread_t *thread)
{
	unsigned int ret;

	ret = _proc_threadGetLockPriority(thread);

	return (ret < thread->priorityBase) ? ret : thread->priorityBase;
}


static void _proc_threadSetPriority(thread_t *thread, unsigned int priority)
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
			LIB_ASSERT(LIST_BELONGS(&threads_common.ready[thread->priority], thread->sched) != 0,
					"thread: 0x%p, tid: %d, priority: %d, is not on the ready list",
					thread, proc_getTid(thread), thread->priority);
			LIST_REMOVE(&threads_common.ready[thread->priority], thread->sched);
			LIST_ADD(&threads_common.ready[priority], thread->sched);
		}
	}

	thread->priority = priority;
	trace_eventThreadPriority(proc_getTid(thread), thread->priority);
}


int proc_threadPriority(int priority)
{
	thread_t *current;
	spinlock_ctx_t sc;
	int ret, reschedule = 0;

	if (priority < -1) {
		return -EINVAL;
	}

	if ((priority >= 0) && (priority >= sizeof(threads_common.ready) / sizeof(threads_common.ready[0]))) {
		return -EINVAL;
	}

	hal_spinlockSet(&threads_common.spinlock, &sc);

	current = _proc_current();

	/* NOTE: -1 is used to retrieve the current thread priority only */
	if (priority >= 0) {
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

		current->priorityBase = priority;
	}

	ret = current->priorityBase;

	if (reschedule != 0) {
		hal_cpuReschedule(&threads_common.spinlock, &sc);
	}
	else {
		hal_spinlockClear(&threads_common.spinlock, &sc);
	}

	trace_eventThreadPriority(proc_getTid(current), current->priority);

	return ret;
}


static void _wakePassive(thread_t *t)
{
	LIB_ASSERT(t->passive == 1, "t is not passive!");
	LIB_ASSERT(t->state == BLOCKED_ON_RECV, "t is passive and interruptible but not BLOCKED_ON_RECV? state=%d", t->state);
	/* TODO: move this to some generic passive reversing function? */

	/* TODO: we could also interrupt the server while its handling the message
	 * it would require to clean up the server as well as the client */

	LIB_ASSERT(t->inherited != NULL, "no inherited SC??");
	if (t->inherited->next == t->inherited) {
		/* this is ours SC */
		t->passive = 0;
	}

	t->sched = t->inherited;
	LIST_REMOVE(&t->inherited, t->sched);

	_proc_threadDequeue(t);
}


static void _thread_interrupt(thread_t *t)
{
	if (t->passive == 1) {
		_wakePassive(t);
		unbindFromAddedTo(t);
		t->utcb.msg->o.err = -EINTR;
		hal_cpuSetReturnValue(t->context, NULL);
	}
	else {
		_proc_threadDequeue(t);
		hal_cpuSetReturnValue(t->context, (void *)-EINTR);
	}
}


void proc_threadEnd(void)
{
	thread_t *t;
	int cpu;
	spinlock_ctx_t sc;

	hal_spinlockSet(&threads_common.spinlock, &sc);

	cpu = hal_cpuGetID();
	t = threads_common.current[cpu]->t;
	threads_common.current[cpu] = NULL;
	t->state = GHOST;
	LIB_ASSERT(t->sched != NULL, "null sched? maybe ok but must be handled");
	LIST_ADD(&threads_common.ghosts, t->sched);
	_proc_threadWakeup(&threads_common.reaper);

	hal_cpuReschedule(&threads_common.spinlock, &sc);
}


static void _proc_threadExit(thread_t *t)
{
	t->exit = THREAD_END;
	if (t->interruptible)
		_thread_interrupt(t);
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
	if ((t = *threads) != NULL) {
		do {
			if (t != except) {
				_proc_threadExit(t);
			}
		} while ((t = t->procnext) != *threads);
	}
	hal_spinlockClear(&threads_common.spinlock, &sc);
}


void proc_reap(void)
{
	sched_context_t *ghost;
	spinlock_ctx_t sc;

	hal_spinlockSet(&threads_common.spinlock, &sc);
	while (threads_common.ghosts == NULL) {
		_proc_threadWait(&threads_common.reaper, 0, &sc);
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

	LIB_ASSERT(t->sched != NULL, "dequeueing unschedulable thread! tid: %d", proc_getTid(t));

	_threads_waking(t);

	if (t->wait != NULL) {
		LIST_REMOVE_EX(t->wait, t, qnext, qprev);
	}

	if (t->wakeup) {
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
		LIST_ADD(&threads_common.ready[t->priority], t->sched);
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


static void _proc_threadEnqueue(thread_t **queue, time_t timeout, int interruptible)
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

	if (*queue == NULL)
		return EOK;

	err = hal_cpuReschedule(&threads_common.spinlock, scp);
	hal_spinlockSet(&threads_common.spinlock, scp);

	return err;
}


int proc_threadSleep(time_t us)
{
	thread_t *current;
	int err;
	time_t now;
	spinlock_ctx_t sc;

	hal_spinlockSet(&threads_common.spinlock, &sc);

	/* Handle usleep(0) (yield) */
	if (us != 0) {
		now = _proc_gettimeRaw();

		current = _proc_current();
		current->state = SLEEP;
		current->wait = NULL;
		current->wakeup = now + us;
		current->interruptible = 1;

		lib_rbInsert(&threads_common.sleeping, &current->sleeplinkage);

		_threads_enqueued(current);
		_threads_updateWakeup(now, NULL);
	}

	err = hal_cpuReschedule(&threads_common.spinlock, &sc);
	if (err == -ETIME) {
		err = EOK;
	}

	return err;
}


static int proc_threadWaitEx(thread_t **queue, spinlock_t *spinlock, time_t timeout, int interruptible, spinlock_ctx_t *scp)
{
	int err;
	spinlock_ctx_t tsc;

	hal_spinlockSet(&threads_common.spinlock, &tsc);
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
	return proc_threadWaitEx(queue, spinlock, timeout, 0, scp);
}


int proc_threadWaitInterruptible(thread_t **queue, spinlock_t *spinlock, time_t timeout, spinlock_ctx_t *scp)
{
	return proc_threadWaitEx(queue, spinlock, timeout, 1, scp);
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
		hal_cpuReschedule(&threads_common.spinlock, &sc);
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
		hal_cpuReschedule(&threads_common.spinlock, &sc);
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
		while ((ghost = process->ghosts) == NULL) {
			err = _proc_threadWait(&process->reaper, abstimeout, &sc);
			if (err == -EINTR || err == -ETIME) {
				break;
			}
		}
	}

	if (ghost != NULL) {
		LIST_REMOVE_EX(&process->ghosts, ghost, procnext, procprev);
		id = proc_getTid(ghost);
	}
	hal_spinlockClear(&threads_common.spinlock, &sc);

	if ((ghost != NULL) && (ghost->tls.tls_sz != 0)) {
		process_tlsDestroy(&ghost->tls, process->mapp);
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
		if (now >= thread->wakeup)
			wakeup = 0;
		else
			wakeup = thread->wakeup - now;
	}

	return wakeup;
}


/*
 * Signals
 */


int threads_sigpost(process_t *process, thread_t *thread, int sig)
{
	int sigbit = 1 << sig;
	spinlock_ctx_t sc;

	switch (sig) {
		case signal_segv:
		case signal_illegal:
			if (process->sighandler != NULL) {
				break;
			}

		/* passthrough */
		case signal_kill:
			proc_kill(process);
			return EOK;

		case signal_cancel:
			proc_threadDestroy(thread);
			return EOK;

		case 0:
			return EOK;

		default:
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
				if (sigbit & ~thread->sigmask) {
					if (thread->interruptible) {
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

	hal_cpuReschedule(&threads_common.spinlock, &sc);

	return EOK;
}


static int _threads_checkSignal(thread_t *selected, process_t *proc, cpu_context_t *signalCtx, unsigned int oldmask, const int src)
{
#ifndef KERNEL_SIGNALS_DISABLE
	LIB_ASSERT(proc != NULL, "proc is null");

	unsigned int sig;

	sig = (selected->sigpend | proc->sigpend) & ~selected->sigmask;
	if ((sig != 0) && (proc->sighandler != NULL)) {
		sig = hal_cpuGetLastBit(sig);

		if (hal_cpuPushSignal(selected->kstack + selected->kstacksz, proc->sighandler, signalCtx, sig, oldmask, src) == 0) {
			selected->sigpend &= ~(1 << sig);
			proc->sigpend &= ~(1 << sig);
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
		f = thread->process->sighandler;
		hal_spinlockClear(&threads_common.spinlock, &sc);
		hal_jmp(f, kstackTop, hal_cpuGetUserSP(signalCtx), 0, NULL);
		/* no return */
	}

	/* Sleep forever (atomic lock release), interruptible */
	thread_t *tqueue = NULL;
	_proc_threadEnqueue(&tqueue, 0, 1);
	hal_cpuReschedule(&threads_common.spinlock, &sc);
	/* after wakeup */

	/* check for pending signals before restoring the old mask */
	hal_spinlockSet(&threads_common.spinlock, &sc);
	if (_threads_checkSignal(thread, thread->process, signalCtx, oldmask, SIG_SRC_SCALL) == 0) {
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


static int _proc_lockSet(lock_t *lock, int interruptible, spinlock_ctx_t *scp)
{
	thread_t *current;
	spinlock_ctx_t sc;
	int ret = EOK, tid;

	hal_spinlockSet(&threads_common.spinlock, &sc);

	current = _proc_current();
	tid = proc_getTid(current);

	_trace_eventLockSetEnter(lock, tid);

	do {
		if ((lock->attr.type == PH_LOCK_ERRORCHECK) && (lock->owner == current)) {
			hal_spinlockClear(&threads_common.spinlock, &sc);
			ret = -EDEADLK;
			break;
		}

		if ((lock->attr.type == PH_LOCK_RECURSIVE) && (lock->owner == current)) {
			if ((lock->depth + 1) == 0) {
				ret = -EAGAIN;
			}
			else {
				lock->depth++;
				ret = EOK;
			}

			hal_spinlockClear(&threads_common.spinlock, &sc);
			break;
		}

		LIB_ASSERT(lock->owner != current, "lock: %s, pid: %d, tid: %d, deadlock on itself",
				lock->name, (current->process != NULL) ? process_getPid(current->process) : 0, proc_getTid(current));

		if (_proc_lockTry(current, lock) < 0) {
			/* Lock owner might inherit our priority */

			if (current->priority < lock->owner->priority) {
				_proc_threadSetPriority(lock->owner, current->priority);
			}

			hal_spinlockClear(&threads_common.spinlock, &sc);

			do {
				/* _proc_lockUnlock will give us a lock by it's own */
				if (proc_threadWaitEx(&lock->queue, &lock->spinlock, 0, interruptible, scp) == -EINTR) {
					/* Can happen when thread_destroy is called on lock owner and current */
					if (lock->owner == NULL) {
						ret = -EINTR;
						break;
					}
					/* Don't return EINTR if we got lock anyway */
					if (lock->owner != current) {
						hal_spinlockSet(&threads_common.spinlock, &sc);

						/* Recalculate lock owner priority (it might have been inherited from the current thread) */
						_proc_threadSetPriority(lock->owner, _proc_threadGetPriority(lock->owner));

						hal_spinlockClear(&threads_common.spinlock, &sc);

						ret = -EINTR;
						break;
					}
				}
			} while (lock->owner != current);
		}
		else {
			hal_spinlockClear(&threads_common.spinlock, &sc);
		}

		if (ret == EOK) {
			lock->depth = 1;
		}
	} while (0);

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

	err = _proc_lockSet(lock, 0, &sc);

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

	err = _proc_lockSet(lock, 1, &sc);

	hal_spinlockClear(&lock->spinlock, &sc);

	return err;
}


static int _proc_lockUnlock(lock_t *lock)
{
	thread_t *owner = lock->owner, *current;
	spinlock_ctx_t sc;
	int ret = 0, lockPriority;

	hal_spinlockSet(&threads_common.spinlock, &sc);

	current = _proc_current();

	_trace_eventLockClear(lock, proc_getTid(current));

	LIB_ASSERT(LIST_BELONGS(&owner->locks, lock) != 0, "lock: %s, owner pid: %d, owner tid: %d, lock is not on the list",
			lock->name, (owner->process != NULL) ? process_getPid(owner->process) : 0, proc_getTid(owner));

	if ((lock->attr.type == PH_LOCK_ERRORCHECK) || (lock->attr.type == PH_LOCK_RECURSIVE)) {
		if (lock->owner != current) {
			hal_spinlockClear(&threads_common.spinlock, &sc);
			return -EPERM;
		}
	}

	if ((lock->attr.type == PH_LOCK_RECURSIVE) && (lock->depth > 0)) {
		lock->depth--;
		if (lock->depth != 0) {
			hal_spinlockClear(&threads_common.spinlock, &sc);
			return 0;
		}
	}

	LIST_REMOVE(&owner->locks, lock);
	if (lock->queue != NULL) {
		/* Calculate appropriate priority, wakeup waiting thread and give it a lock */
		lock->owner = lock->queue;
		lockPriority = _proc_lockGetPriority(lock);
		if (lockPriority < lock->owner->priority) {
			_proc_threadSetPriority(lock->queue, lockPriority);
		}
		_proc_threadDequeue(lock->owner);
		LIST_ADD(&lock->owner->locks, lock);
		ret = 1;
	}
	else {
		lock->owner = NULL;
	}

	/* Restore previous owner priority */
	_proc_threadSetPriority(owner, _proc_threadGetPriority(owner));

	LIB_ASSERT(current->priority <= current->priorityBase, "pid: %d, tid: %d, basePrio: %d, priority degraded (%d)",
			(current->process != NULL) ? process_getPid(current->process) : 0, proc_getTid(current), current->priorityBase,
			current->priority);

	hal_spinlockClear(&threads_common.spinlock, &sc);

	return ret;
}


static void proc_lockUnlock(lock_t *lock)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&lock->spinlock, &sc);

	if (_proc_lockUnlock(lock) > 0) {
		hal_spinlockClear(&lock->spinlock, &sc);
		hal_cpuReschedule(NULL, NULL);
	}
	else {
		hal_spinlockClear(&lock->spinlock, &sc);
	}
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

	return _proc_lockUnlock(lock);
}


int proc_lockClear(lock_t *lock)
{
	spinlock_ctx_t sc;
	int err;

	if (hal_started() == 0) {
		return -EINVAL;
	}

	hal_spinlockSet(&lock->spinlock, &sc);

	err = _proc_lockClear(lock);
	if (err > 0) {
		hal_spinlockClear(&lock->spinlock, &sc);
		hal_cpuReschedule(NULL, NULL);
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
		proc_lockClear(l1);
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
		err = proc_threadWaitEx(queue, &lock->spinlock, timeout, 1, &sc);
		if (err != -EINTR) {
			_proc_lockSet(lock, 0, &sc);
		}
	}

	hal_spinlockClear(&lock->spinlock, &sc);

	return err;
}

int proc_lockDone(lock_t *lock)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&lock->spinlock, &sc);

	if (lock->owner != NULL) {
		_proc_lockUnlock(lock);
	}

	hal_spinlockClear(&lock->spinlock, &sc);
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

		hal_spinlockSet(&threads_common.spinlock, &sc);
		wakeup = _proc_nextWakeup();

		if (wakeup > (2 * SYSTICK_INTERVAL)) {
			hal_cpuLowPower(wakeup, &threads_common.spinlock, &sc);
		}
		else {
			hal_spinlockClear(&threads_common.spinlock, &sc);
			hal_cpuHalt();
		}
	}
}


void proc_threadsDump(unsigned int priority)
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

		if (sched == NULL)
			break;

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

	proc_lockSet(&threads_common.lock);

	t = lib_treeof(thread_t, idlinkage, lib_rbMinimum(threads_common.id.root));

	while (i < n && t != NULL) {
		if (t->process != NULL) {
			tinfo.pid = process_getPid(t->process);
			// tinfo.ppid = t->process->parent != NULL ? t->process->parent->id : 0;
			tinfo.ppid = 0;
		}
		else {
			tinfo.pid = 0;
			tinfo.ppid = 0;
		}

		hal_spinlockSet(&threads_common.spinlock, &sc);
		tinfo.tid = proc_getTid(t);
		tinfo.state = t->state;
		if (t->sched != NULL) {
			tinfo.priority = t->priorityBase;
			now = _proc_gettimeRaw();
			if (now != t->sched->startTime) {
				tinfo.load = (t->sched->cpuTime * 1000) / (now - t->sched->startTime);
			}
			else {
				tinfo.load = 0;
			}
			tinfo.cpuTime = t->sched->cpuTime;

			if (t->state == READY && t->sched->maxWait < now - t->sched->readyTime) {
				tinfo.wait = now - t->sched->readyTime;
			}
			else {
				tinfo.wait = t->sched->maxWait;
			}
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
		if ((t->process != NULL) && (entry = t->process->entries) != NULL) {
			do {
				tinfo.vmem += entry->size;
				entry = entry->next;
			} while (entry != t->process->entries);
		}
		else
#endif
				if (map != NULL) {
			proc_lockSet(&map->lock);
			entry = lib_treeof(map_entry_t, linkage, lib_rbMinimum(map->tree.root));

			while (entry != NULL) {
				tinfo.vmem += entry->size;
				entry = lib_treeof(map_entry_t, linkage, lib_rbNext(&entry->linkage));
			}
			proc_lockClear(&map->lock);
		}

		cb(arg, i, &tinfo);

		++i;
		t = lib_idtreeof(thread_t, idlinkage, lib_idtreeNext(&t->idlinkage.linkage));
	}

	proc_lockClear(&threads_common.lock);

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


int proc_threadsList(int n, threadinfo_t *tinfos)
{
	return proc_threadsIter(n, proc_threadsListCb, tinfos);
}


int _threads_init(vm_map_t *kmap, vm_object_t *kernel)
{
	unsigned int i;
	threads_common.kmap = kmap;
	threads_common.ghosts = NULL;
	threads_common.reaper = NULL;
	threads_common.utcoffs = 0;
	threads_common.idcounter = 0;
	threads_common.prev = 0;

	proc_lockInit(&threads_common.lock, &proc_lockAttrDefault, "threads.common");

	for (i = 0; i < sizeof(threads_common.stackCanary); ++i)
		threads_common.stackCanary[i] = (i & 1) ? 0xaa : 0x55;

	/* Initiaizlie scheduler queue */
	for (i = 0; i < sizeof(threads_common.ready) / sizeof(thread_t *); i++)
		threads_common.ready[i] = NULL;

	lib_rbInit(&threads_common.sleeping, threads_sleepcmp, NULL);
	lib_idtreeInit(&threads_common.id);

	lib_printf("proc: Initializing thread scheduler, priorities=%d\n", sizeof(threads_common.ready) / sizeof(thread_t *));

	hal_spinlockCreate(&threads_common.spinlock, "threads.spinlock");

	/* Allocate and initialize current threads array */
	threads_common.current = (sched_context_t **)vm_kmalloc(sizeof(sched_context_t *) * hal_cpuGetCount());
	if (threads_common.current == NULL)
		return -ENOMEM;

	/* Run idle thread on every cpu */
	for (i = 0; i < hal_cpuGetCount(); i++) {
		threads_common.current[i] = NULL;
		proc_threadCreate(NULL, threads_idlethr, NULL, sizeof(threads_common.ready) / sizeof(thread_t *) - 1, SIZE_KSTACK, NULL, 0, NULL);
	}

	/* Install scheduler on clock interrupt */
#ifdef PENDSV_IRQ
	hal_memset(&threads_common.pendsvHandler, NULL, sizeof(threads_common.pendsvHandler));
	threads_common.pendsvHandler.f = threads_schedule;
	threads_common.pendsvHandler.n = PENDSV_IRQ;
	hal_interruptsSetHandler(&threads_common.pendsvHandler);
#endif

	hal_memset(&threads_common.timeintrHandler, NULL, sizeof(threads_common.timeintrHandler));
	hal_timerRegister(threads_timeintr, NULL, &threads_common.timeintrHandler);

	return EOK;
}


// #define log_debug(fmt, ...) lib_printf(fmt "\n", ##__VA_ARGS__)
// #define log_err(fmt, ...)   lib_printf(fmt "\n", ##__VA_ARGS__)
#define log_debug(fmt, ...)
#define log_err(fmt, ...)


#if 0
/* verbose reason */
static inline int _mustSlowCall(port_t *p, thread_t *caller)
{
	if (p->fpThreads == NULL) {
		return 1;
	}
	if (p->fpThreads->sched != NULL) {
		return 2;
	}
	if (threads_getHighestPrio(caller->priority) != caller->priority) {
		return 3;
	}

	return 0;
}
#else
static inline int _mustSlowCall(port_t *p, thread_t *caller)
{
	LIB_ASSERT(p != NULL, "p null??");

#if 0
	/* TODO: purely for investigation purposes, if will never happen, remove this
	 * */
	thread_t *thread = p->fpThreads;

	if (thread != NULL) {
		do {
			if (thread->exit != 0) {
				LIB_ASSERT_ALWAYS(0, "happeeeeeeens");
			}
			thread = thread->tnext;
		} while (thread != p->fpThreads);
	}
#endif

	return p->fpThreads == NULL ||
			// p->caller != NULL ||
			/* if recv is an active server, check priority */
			(p->fpThreads->sched != NULL) ||
			// (p->threads->sched != NULL && caller->priority < p->threads->priority) ||
			threads_getHighestPrio(caller->priority) != caller->priority;
}
#endif

#define VERBOSE 0

static int proc_setupSharedBuffer(thread_t *t, thread_t *recv, void *buf, size_t bufsz, ipc_buf_layout_t *il, void **rbuf);


static void _portEnqueue(port_t *p, thread_t *t)
{
	LIST_ADD_EX(&p->fpThreads, t, tnext, tprev);
	t->addedTo = p;
}


static void _portDequeue(port_t *p, thread_t *t)
{
	LIB_ASSERT(t->addedTo == p, "thread not added to this port");
	LIST_REMOVE_EX(&p->fpThreads, t, tnext, tprev);
	t->addedTo = NULL;
}


/* TODO: crude API, fix */
void threads_ipcBufferRelease(ipc_buf_layout_t *il)
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
		vm_munmap(il->map, il->w, il->sz);
		il->w = NULL;
		il->sz = 0;
		il->map = NULL;
	}
}


static void _threads_copyShadowPages(ipc_buf_layout_t *il, size_t size)
{
	if (il->bp != NULL) {
		hal_memcpy(il->bvaddr + il->boffs, il->w + il->boffs, min(SIZE_PAGE - il->boffs, size));
	}
	if (il->eoffs != 0) {
		hal_memcpy(il->evaddr, il->w + il->boffs + size - il->eoffs, il->eoffs);
	}
}


static void _threads_copyShmBuffers(thread_t *from, thread_t *to, msg_t *msg)
{
	if (to->mappedTo == from) {
		/* TODO copying i may be unnecessary */
		_threads_copyShadowPages(&to->utcb.iil, msg->i.size);
		_threads_copyShadowPages(&to->utcb.oil, msg->o.size);
	}
}

/* assuming aspace of `to` */
static void _threads_copyMsgBufResponse(thread_t *from, thread_t *to, msg_t *msg)
{
	to->utcb.msg->o.size = msg->o.size;

	hal_memcpy(to->utcb.msg->o.raw, msg->o.raw, sizeof(msg->o.raw));
	to->utcb.msg->o.err = msg->o.err;

	/* TODO: handle pulse as well */
}


static int proc_call_ex(u32 port, msg_t *msg, int returnable)
{
	port_t *p;
	thread_t *caller, *recv;
	spinlock_ctx_t sc;
	int err;
	cpu_context_t *ctx;

	p = proc_portGet(port);
	if (p == NULL) {
		return -EINVAL;
	}

	caller = proc_current();

	hal_spinlockSet(&p->spinlock, &sc);

	if (p->closed != 0) {
		hal_spinlockClear(&p->spinlock, &sc);
		port_put(p, 0);
		return -EINVAL;
	}

	LIB_ASSERT(p->threads == NULL, "call but pending old recv! tid=%d", proc_getTid(p->threads));

	while ((err = _mustSlowCall(p, caller)) != 0) {
		p->queue.nonempty |= 1;
		err = proc_threadWaitInterruptible(&p->queue.pq[caller->priority], &p->spinlock, 0, &sc);
		if (err < 0) {
			hal_spinlockClear(&p->spinlock, &sc);
			port_put(p, 0);
			LIB_ASSERT_ALWAYS(0, "FAIL");
			return err;
		}
		/* TODO: abort on server fault/port closure */
	}

	/* commit to fastpath - point of no return */

	recv = p->fpThreads;
	LIST_REMOVE_EX(&p->fpThreads, recv, tnext, tprev);
	recv->addedTo = NULL;

	spinlock_ctx_t tsc;

	hal_spinlockSet(&threads_common.spinlock, &tsc);
	if (returnable != 0) {
		caller->callReturnable = 1;
	}

	caller->utcb.msg = msg;

	size_t smallBufSize;
	// if (recv->utcb.msg->edata != NULL) {
	// 	LIB_ASSERT(0, "edata %d", port);
	// 	/* recv has supplied a buffer to which to copy the message to */
	// 	smallBufSize = min(sizeof(caller->utcb.msgbuf), recv->utcb.msg->esize);
	// }
	// else {
	smallBufSize = sizeof(msg->i.raw);
	// }

	void *imap = NULL, *omap = NULL;
	size_t isize = 0, osize = 0;

	/* TODO: add rawsz to msg_t */
	size_t rawSz = sizeof(msg->i.raw);
	hal_memcpy(caller->utcb.msgbuf, msg->i.raw, rawSz);
	caller->utcb.msglen = rawSz;

	if (msg->i.size > 0) {
		if (rawSz + msg->i.size <= smallBufSize) {
			/* small message: fits the kernel buf and the predefined recv buffer */
			hal_memcpy(caller->utcb.msgbuf + rawSz, msg->i.data, msg->i.size);
			caller->utcb.msglen += msg->i.size;

			LIB_ASSERT(0, "happens");
		}
		else {
			imap = (void *)msg->i.data;
			isize = msg->i.size;
		}
	}

	// if (msg->o.size <= smallBufSize) {
	// 	/* small reply */
	// 	recv->utcb.msg->o.data = NULL;
	// 	recv->utcb.msg->o.size = 0;
	// }
	// else if (msg->o.size > 0) {
	omap = msg->o.data;
	osize = msg->o.size;
	// }


	oid_t oid;
	int type;

	hal_memcpy(&oid, &msg->oid, sizeof(oid_t));
	type = msg->type;

	ctx = _threads_switchTo(recv, 0);

	hal_spinlockClear(&threads_common.spinlock, &tsc);

	/* TODO: bump refcnt on recv? */
	hal_spinlockClear(&p->spinlock, &sc);

	/* FIXME: safety: recv could get interrupted */

	/*
	 * NOTE: assumes port spinlock is set so that this spinlockSet is nested and tsc
	 * context has interrupts disabled
	 */

	LIB_ASSERT(recv != NULL, "recv is null");
	LIB_ASSERT(caller != NULL, "null caller");
	LIB_ASSERT(recv->exit == 0, "recv exit=%d", recv->exit);
	LIB_ASSERT(recv->refs > 0, "attempting to return to refs=0 rcv? port=%d caller tid=%d recv tid=%d refs: %d",
			p->linkage.id, proc_getTid(caller), proc_getTid(recv), recv->refs);
	LIB_ASSERT(recv->utcb.msg != NULL, "recv msg is null");

	/* message transfer */

	hal_memcpy(recv->utcb.msg->i.raw, caller->utcb.msgbuf, rawSz);

	if (imap != NULL) {
		/* TODO: permissions, incoming data doesnt need to be writable */
		if (proc_setupSharedBuffer(caller, recv, imap, isize, &caller->utcb.iil, (void *)&recv->utcb.msg->i.data) < 0) {
			port_put(p, 0);
			LIB_ASSERT(0, "enomem");
			return -ENOMEM;
		}
		recv->utcb.msg->i.size = isize;
	}
	else {
		threads_ipcBufferRelease(&caller->utcb.iil);
		caller->mappedTo = NULL;

		if (caller->utcb.msglen > rawSz) {
			hal_memcpy((void *)recv->utcb.msg->i.data, caller->utcb.msgbuf + rawSz, caller->utcb.msglen - rawSz);
		}

		/* Update the recv i.size to reflect the msg size. Not sure how clean that is.
		 * May need to rework this. */
		recv->utcb.msg->i.size = caller->utcb.msglen - rawSz;
	}

	if (omap != NULL) {
		if (proc_setupSharedBuffer(caller, recv, omap, osize, &caller->utcb.oil, &recv->utcb.msg->o.data) < 0) {
			port_put(p, 0);
			LIB_ASSERT(0, "enomem");
			return -ENOMEM;
		}
	}
	else {
		threads_ipcBufferRelease(&caller->utcb.oil);
		caller->mappedTo = NULL;
	}

	recv->utcb.msg->o.size = osize;
	recv->utcb.msg->pid = (caller->process != NULL) ? process_getPid(caller->process) : 0;
	hal_memcpy(&recv->utcb.msg->oid, &oid, sizeof(oid_t));
	recv->utcb.msg->type = type;
	recv->utcb.msg->priority = caller->priority; /* ??? */

	/* msg transfer should be done by now */

	recv->interruptible = 0;

	*recv->utcb.ridPtr = (msg_rid_t)caller;
	hal_cpuSetReturnValue(ctx, EOK);

	hal_spinlockSet(&threads_common.spinlock, &sc);

	recv->fastpathExitCtx = ctx;

	LIB_ASSERT(_proc_current() == recv, "we should be recv here");

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
	}

	port_put(p, 0);

	return EOK;
}


int proc_send(u32 port, msg_t *msg)
{
	return proc_call_ex(port, msg, 0);
}


int proc_send_returnable(u32 port, msg_t *msg)
{
	int err = proc_call_ex(port, msg, 1);
	// LIB_ASSERT(err >= 0, "port=%d err=%d", port, err);

	return err;
}


#if 0
static thread_t *_proc_getMinPrioQueue(prio_queue_t *queue)
{
	size_t prio;
	thread_t *q;
	for (prio = 0; prio < MAX_PRIO; prio++) {
		q = queue->pq[prio];
		if (q != NULL && q != wakeupPending) {
			return q;
		}
	}
	queue->nonempty = 0;
	return NULL;
}
#endif


static int _proc_threadWakeupPrio(prio_queue_t *queue)
{
	size_t prio;
	for (prio = 0; prio < MAX_PRIO; prio++) {
		if (_proc_threadWakeup(&queue->pq[prio]) != 0) {
			return 1;
		}
	}
	queue->nonempty = 0;
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

static int _becomePassive(port_t *p, thread_t *recv, msg_t *msg, msg_rid_t *rid, spinlock_ctx_t *sc)
{
	spinlock_ctx_t tsc;
	int err;

	hal_spinlockSet(&threads_common.spinlock, &tsc);

	recv->utcb.ridPtr = rid;
	recv->utcb.msg = msg;

	log_err("passive %d", proc_getTid(recv));
	_portEnqueue(p, recv);

	if (recv->sched != NULL) {
		/* invariant: first element of the list is the receiver's original SC */
		LIST_ADD(&recv->inherited, recv->sched);
		recv->sched = NULL;
	}

	(void)_proc_threadWakeupPrio(&p->queue);

	hal_spinlockClear(&p->spinlock, &tsc);

	recv->state = BLOCKED_ON_RECV;
	recv->passive = 1;
	recv->interruptible = 1;
	recv->flags &= (~(int)THREAD_PULSED);

	/*
	 * the port is not put, because the passive thread still uses the port
	 * the port will get destroyed on proc_destroy
	 */

	hal_cpuReschedule(&threads_common.spinlock, sc);

	if ((recv->flags & THREAD_PULSED) != 0) {
		// LIB_ASSERT(0, "pulse happens");
		recv->flags &= (~(int)THREAD_PULSED);

		msg->o.pulse = recv->utcb.pulse;
		msg->o.err = EOK;
		err = -EPULSE;
	}
	else if (recv->reply == NULL) {
		err = -EINTR;
	}
	else {
		*recv->utcb.ridPtr = (msg_rid_t)recv->reply;
		err = EOK;
	}

	port_put(p, 0);
	return err;
}

/* assumes aspace of recv */
int _returnWithPulse(thread_t *recv, port_t *p, spinlock_ctx_t *sc)
{
	LIB_ASSERT(0, "return happens");
	recv->utcb.msg->o.pulse = p->pulse;
	recv->utcb.msg->o.err = EOK;
	p->pulse = 0;
	hal_spinlockClear(&p->spinlock, sc);
	return -EPULSE;
}

/* I actually like this data-size + hdr->odata approach.
A server is then in control of the performance of the transfer. If it wants to
support fast transfers and knows the payload size beforehand, then it will allocate a large `data`.
Otherwise, it can simply put data=NULL and just depend on kernel-allocated payloads
BTW: large `data` is simply a way of expressing proc_registerMsgBuffer ! :)
*/

/* TODO: error passing */
int proc_recv(u32 port, msg_t *msg, msg_rid_t *rid)
{
	port_t *p;
	spinlock_ctx_t sc;
	thread_t *recv;

	recv = proc_current();

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

	if (p->pulse != 0) {
		LIB_ASSERT(0, "return happens");
		return _returnWithPulse(recv, p, &sc);
	}

	LIB_ASSERT(p->kmessages == NULL, "kmessages on new port??");

	return _becomePassive(p, recv, msg, rid, &sc);
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
	recv = p->fpThreads;

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


int proc_respond(u32 port, msg_t *msg, msg_rid_t rid)
{
	port_t *p;
	spinlock_ctx_t sc;
	thread_t *caller, *recv;

	/* TODO: make nicer */
	void *reply = (void *)rid;

	recv = proc_current();

	lib_debug_printf("proc_respond2 port=%d (%d)\n", port, proc_getTid(recv));

	/* TODO: ensure reply->called reference is not user-exploitable */
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

	caller = reply;
	if (caller->called != recv) {
		LIB_ASSERT(0, "unmatched reply = %p (expected: %p)\n", reply, caller->called);
		hal_spinlockClear(&p->spinlock, &sc);
		port_put(p, 0);
		return -EINVAL;
	}

	if (caller->exit != 0) {
		/* caller is dying, don't respond */

		hal_spinlockSet(&threads_common.spinlock, &sc);

		LIB_ASSERT(recv->passive == 1, "recv not passive?");
		recv->state = READY;

		LIB_ASSERT(recv->inherited != NULL, "there should be an inherited SC");
		recv->sched = recv->inherited;
		LIST_REMOVE(&recv->inherited, recv->sched);

		if (caller != recv->reply) {
			recv->passive = 0;
		}
		LIB_ASSERT(recv->sched != NULL, "recv sched null?");

		threads_common.current[hal_cpuGetID()] = recv->sched;

		LIB_ASSERT(recv->sched->t == recv, "badly linked sched context");
		hal_spinlockClear(&threads_common.spinlock, &sc);
		hal_spinlockClear(&p->spinlock, &sc);
		port_put(p, 0);

		return -EINVAL;
	}

	hal_spinlockClear(&p->spinlock, &sc);
	port_put(p, 0);

	LIB_ASSERT(caller != NULL, "caller null!");

	/* UGLY: but for now lets make the msg_t working */
	msg_t msgCopy = *msg;

	/*
	 * Must hold the spinlock across pmap_switch + copy to prevent preemption.
	 * If recv is preempted after pmap_switch(caller) and resumed later, the
	 * scheduler restores recv's address space, causing the response copy to
	 * write through the wrong page tables (recv's instead of caller's).
	 */
	hal_spinlockSet(&threads_common.spinlock, &sc);
	_threads_copyShmBuffers(recv, caller, &msgCopy);
	if ((caller->process != NULL) && (caller->process->pmapp != NULL)) {
		pmap_switch(caller->process->pmapp);
	}
	else {
		pmap_switch(&threads_common.kmap->pmap);
	}
	_threads_copyMsgBufResponse(recv, caller, &msgCopy);
	if ((recv->process != NULL) && (recv->process->pmapp != NULL)) {
		pmap_switch(recv->process->pmapp);
	}
	else {
		pmap_switch(&threads_common.kmap->pmap);
	}
	/*
	 * FIXME: workaround for assertion in _threads_switchSchedContexts
	 * reply == 1 branch. The _threads_switchSchedContexts currently assumes
	 * that IPC threads are only in a path-like dependency chains, while it's
	 * possible that a single (active) server receives multiple messages and
	 * responds to them in arbitrary order.
	 *
	 * This is probably where the proper BWI is needed, as the active server
	 * will process the messages on client's SC, but currently it's unclear which
	 * and not tracked as it should be.
	 */
	int multiple_callers = 0;
	thread_t *og_reply = recv->reply;
	if (caller != og_reply) {
		multiple_callers = 1;
		recv->reply = caller;
	}

	/*
	 * FIXME: currently nothing stops SCs from different
	 * clients to end up swapped (i.e. client A gets SC of client B after IPC)
	 */

	_threads_switchSchedContexts(recv, caller, 1);
	LIST_ADD(&threads_common.ready[caller->priority], caller->sched);

	if (multiple_callers != 0) {
		recv->reply = og_reply;
	}

	lib_debug_printf("proc_respond2 context addr (pre): %p sp=%p\n", caller->context, caller->context->sp);
	if (caller->callReturnable == 0) {
		caller->context = _getUserContext(caller);
		hal_cpuSetReturnValue(caller->context, (void *)(ptr_t)EOK);
	}
	else {
		caller->callReturnable = 0;
	}

	lib_debug_printf("proc_respond2 context addr (post): %p sp=%p\n", caller->context, caller->context->sp);

	LIB_ASSERT(recv->passive == 1, "recv not passive?");
	recv->state = READY; /* FIXME: above sets to BLOCKED_ON_RECV, but we have our own SC */

	LIB_ASSERT(recv->inherited != NULL, "there should be an inherited SC");
	recv->sched = recv->inherited;
	LIST_REMOVE(&recv->inherited, recv->sched);

	if (multiple_callers == 0) {
		recv->passive = 0;
	}
	LIB_ASSERT(recv->sched != NULL, "recv sched null?");

	threads_common.current[hal_cpuGetID()] = recv->sched;

	LIB_ASSERT(recv->sched->t == recv, "badly linked sched context");

	if (caller->priority < recv->priority) {
		/* client is ignorant of IPCP and more critical than server for strange reason, reschedule */
		/* TODO: enforce priority ceiling on msgCall attempts */
		hal_cpuReschedule(&threads_common.spinlock, &sc);
	}
	else {
		hal_spinlockClear(&threads_common.spinlock, &sc);
	}

	return EOK;
}


int proc_respondAndRecv(u32 port, msg_t *msg, msg_rid_t *rid)
{
	port_t *p;
	spinlock_ctx_t sc, tsc;
	cpu_context_t *ctx;
	thread_t *caller, *recv;
	int responding;

	recv = proc_current();

	p = proc_portGet(port);
	if (p == NULL) {
		return -EINVAL;
	}

	/* recv SC is actually caller's SC */
	/* TODO: this should probably be accounted in better way */
	if (threads_getHighestPrio(recv->priority) != recv->priority) {
		/* TODO: dead code? */
		LIB_ASSERT(0, "dead code not dead");
	}

	hal_spinlockSet(&p->spinlock, &sc);
	if (p->closed != 0) {
		hal_spinlockClear(&p->spinlock, &sc);
		port_put(p, 0);
		return -EINVAL;
	}

	LIB_ASSERT(p->kmessages == NULL, "kmessages on new port??");

	responding = recv->reply != NULL ? 1 : 0;

	if (responding == 0) {
		if (p->pulse != 0) {
			return _returnWithPulse(recv, p, &sc);
		}

		return _becomePassive(p, recv, msg, rid, &sc);
	}

	/* wake next caller if exists */
	hal_spinlockSet(&threads_common.spinlock, &tsc);
	(void)_proc_threadWakeupPrio(&p->queue);
	hal_spinlockClear(&threads_common.spinlock, &tsc);

	hal_spinlockClear(&p->spinlock, &sc);

	port_put(p, 0);

	/* TODO: handle caller faults */

	/*
	 * noone to reply, doing a fastpath and switching to caller thread
	 * point of no return
	 */

	log_err("fast (%d, %d)", proc_getTid(recv), recv->priority);

	hal_spinlockSet(&threads_common.spinlock, &sc);
	caller = recv->reply;

	/* TODO should exit != 0 be treated in any special way? if we return back to
	 * the exiting client, it will get reaped anyway  */
	LIB_ASSERT(caller != NULL, "caller null!");
	LIB_ASSERT(caller->exit == 0, "exit=%d", caller->exit);
	LIB_ASSERT(caller->state != GHOST, "huh");

	_portEnqueue(p, recv);

	msg_t msgCopy = *msg;

	ctx = _threads_switchTo(caller, 1);

	_threads_copyShmBuffers(recv, caller, &msgCopy);
	_threads_copyMsgBufResponse(recv, caller, &msgCopy);

	if (caller->callReturnable == 0) {
		hal_cpuSetReturnValue(ctx, EOK);
		caller->fastpathExitCtx = ctx;
		hal_spinlockClear(&threads_common.spinlock, &sc);
	}
	else {
		caller->fastpathExitCtx = caller->context;
		caller->callReturnable = 0;
		hal_cpuReschedule(&threads_common.spinlock, &sc);
	}

	return EOK;
}


int proc_registerMsgBuffer(void *buf, size_t bufsz)
{
	// thread_t *t = proc_current();
	//
	// if (((ptr_t)buf & (SIZE_PAGE - 1)) != 0 || (bufsz & (SIZE_PAGE - 1)) != 0) {
	// 	return -EINVAL;
	// }

	LIB_ASSERT(0, "todo");

	// t->bufferStart = buf;
	// t->bufferEnd = (void *)((ptr_t)buf + bufsz);

	return 0;
}


/* make these as lib macros please... */
#define FLOOR(x) ((x) & ~(SIZE_PAGE - 1))
#define CEIL(x)  (((x) + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1))


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

	srcmap = (from == NULL || from->mapp == NULL) ? threads_common.kmap : from->mapp;
	dstmap = (to == NULL || to->mapp == NULL) ? threads_common.kmap : to->mapp;

	if ((srcmap == dstmap) && (pmap_belongs(&dstmap->pmap, data) != 0)) {
		return data;
	}

	size_t sz = (((boffs != 0) ? 1 : 0) + ((eoffs != 0) ? 1 : 0) + n) * SIZE_PAGE;
	w = vm_mapFind(dstmap, NULL, sz, MAP_NOINHERIT, prot);
	il->w = w;
	if (w == NULL) {
		return NULL;
	}
	il->sz = sz;
	il->map = dstmap;

	if (pmap_belongs(&srcmap->pmap, data) != 0) {
		flags = vm_mapFlags(srcmap, data);
	}
	else {
		flags = vm_mapFlags(threads_common.kmap, data);
	}

	if (flags < 0) {
		return NULL;
	}

	attr |= vm_flagsToAttr(flags);

	if (boffs > 0) {
		il->boffs = boffs;
		bpa = pmap_resolve(&srcmap->pmap, data) & ~(SIZE_PAGE - 1);
		if (bpa == NULL) {
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
		if (epa == NULL) {
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
	mappedTo = thread->mappedTo;
	hal_spinlockClear(&threads_common.spinlock, &sc);

	if (mappedTo != NULL) {
		threads_ipcBufferRelease(&thread->utcb.iil);
		threads_ipcBufferRelease(&thread->utcb.oil);
	}
}


#define REUSE_BUFFER 0

/*
 * TODO: Regarding msgBuf_t I wonder if it's not better pass buffers in syscall
 * signature (like QNX) and make the kernel copy the contents to msgBuf_t if
 * it fits.
 * If it doesn't, it maps shm
 *
 * This must be done anyways (now it's done by the userspace), but in current state it
 * heavily clutters the user API.
 */

static int proc_setupSharedBuffer(thread_t *t, thread_t *recv, void *buf, size_t bufsz, ipc_buf_layout_t *il, void **rbuf)
{
	void *w;

	LIB_ASSERT(buf != NULL && bufsz > 0, "bad args");

#if REUSE_BUFFER
	if (t->bufferStart <= buf && (void *)((ptr_t)buf + bufsz) <= t->bufferEnd) {
		if (t->mappedTo != recv->process) {
			// regBufsz = (ptr_t)t->bufferEnd - (ptr_t)t->bufferStart;
			if (t->mappedBase != NULL) {
				/* remap from another process to recv */
				// _unmapBuffer(t->mappedBase, regBufsz, t->mappedTo);
				(void)_unmapBuffer;
				(void)_mapBuffer;
				_bufferRelease(&t->utcb.il);
			}

			/* buffer not mapped in any other process, map to recv */
			// w = _mapBuffer(t->bufferStart, regBufsz, t->process, recv->process);
			w = _mapBufferUnaligned(buf, bufsz, t->process, recv->process, &t->utcb.il);
			if (w == NULL) {
				return -ENOMEM;
			}

			t->mappedBase = w;
			t->mappedTo = recv->process;
		}
	}
	else {
#endif
		threads_ipcBufferRelease(il);

		w = _mapBufferUnaligned(buf, bufsz, t->process, recv->process, il);
		if (w == NULL) {
			LIB_ASSERT(0, "ENOMEM!");
			return -ENOMEM;
		}

		t->mappedTo = recv;

		il->mappedBase = w;
		il->bufferStart = buf;
		il->bufferEnd = buf + bufsz;
#if REUSE_BUFFER
	}
#endif

	*rbuf = il->mappedBase + (buf - il->bufferStart);

	return EOK;
}


void proc_freeUtcb(thread_t *t)
{
}
