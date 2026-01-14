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

#include "syscalls.h"


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
	rbtree_t passive;

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
	return (cpu_context_t *)((char *)thread->kstack + thread->kstacksz - sizeof(cpu_context_t));
}


static void thread_destroy(thread_t *thread)
{
	process_t *process;
	spinlock_ctx_t sc;
	thread_t *reply;
	port_t *p;
	// int ref;

	trace_eventThreadEnd(thread);

	/* No need to protect thread->locks access with threads_common.spinlock */
	/* The destroyed thread is a ghost and no thread (except for the current one) can access it */
	while (thread->locks != NULL) {
		proc_lockUnlock(thread->locks);
	}

	if (thread->addedTo != NULL) {
		p = thread->addedTo;
		hal_spinlockSet(&p->spinlock, &sc);
		LIST_REMOVE_EX(&p->fpThreads, thread, tnext, tprev);
		LIB_ASSERT(0, "happens");
		hal_spinlockClear(&p->spinlock, &sc);
	}

	/* REVISIT: guard with threads spinlock needed? called may hold a reference to us */
	hal_spinlockSet(&threads_common.spinlock, &sc);
	if (thread->called != NULL) {
		LIB_ASSERT(thread->called->reply == thread, "thread->called->reply != thread");
		thread->called->reply = NULL;
	}

	if (thread->passive) {
		lib_rbRemove(&threads_common.passive, &thread->sleeplinkage);
	}

	if (thread->sched != NULL) {
		if (thread->reply != NULL) {
			LIB_ASSERT(thread->reply != thread, "thread replies to itself????");
			reply = thread->reply;
			reply->sched = thread->sched;
			reply->sched->t = reply;
			reply->called = NULL;

			reply->utcb.kw->err = -EINTR; /* TODO: custom errno? */

			LIB_ASSERT(reply->exit == 0, "reply thread exiting?");

			reply->context = _getUserContext(reply);
#ifndef NOMMU
			LIB_ASSERT((ptr_t)hal_cpuGetIP(reply->context) < VADDR_KERNEL, "dest ip in kernel - ip: 0x%p tid: %d", hal_cpuGetIP(reply->context), proc_getTid(reply));
#endif
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

	proc_freeUtcb(thread);

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


static cpu_context_t *_threads_switchTo(thread_t *dest, int reply)
{
	sched_context_t *sched;
	thread_t *from;
	process_t *proc;
	cpu_context_t *ctx;

	sched = _sched_current();
	LIB_ASSERT(sched != NULL, "sched null");
	LIB_ASSERT(dest != NULL, "dest null");

	LIB_ASSERT(dest->exit == 0, "exit=%d", dest->exit);
	LIB_ASSERT(dest->state != GHOST, "dest is a ghost");

	from = sched->t;

	LIB_ASSERT(dest->sched == NULL,
			"dest sched not null (prio=%d, from=%d, dest=%d, reply=%d, sched owner tid=%d)",
			dest->priority, proc_getTid(from), proc_getTid(dest), reply, proc_getTid(dest->sched->owner));

	from->sched = NULL;
	if (reply != 0) {
		/* replying - going back in SC chain */
		LIB_ASSERT(from->reply != NULL, "reply null (initial from->state=%d)", from->state);
		LIB_ASSERT(from->reply == dest, "WHAT");

		from->state = BLOCKED_ON_RECV;

		LIB_ASSERT(dest->state == BLOCKED_ON_REPLY, "dest thread not blocked on reply? state=%d", dest->state);
		from->reply = NULL;
		dest->called = NULL;
	}
	else {
		/* calling - going deeper in SC chain */
		from->state = BLOCKED_ON_REPLY;

		LIB_ASSERT(dest->state == BLOCKED_ON_RECV, "dest thread not blocked on recv? state=%d", dest->state);
		dest->reply = from;
		from->called = dest;
	}
	sched->t = dest;
	dest->sched = sched;
	dest->state = READY;

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
#ifndef NOMMU
	LIB_ASSERT((ptr_t)hal_cpuGetIP(ctx) < VADDR_KERNEL, "dest ip in kernel - ip: 0x%p tid: %d reply: %d", hal_cpuGetIP(ctx), proc_getTid(dest), reply);
#endif

	if (dest->tls.tls_base != NULL) {
		hal_cpuTlsSet(&dest->tls, ctx);
	}

	dest->context = ctx;

	_threads_scheduling(dest);

	LIB_ASSERT(dest->sched != NULL, "dest shed is null");

	hal_cpuSetReturnValue(ctx, 0);

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

	(void)arg;
	(void)n;
	hal_lockScheduler();

	trace_eventSchedEnter(cpuId);

	current = _proc_current();
	threads_common.current[cpuId] = NULL;

	/* Save current thread context */
	if (current != NULL) {
		current->context = context;

		/* Move thread to the end of queue */
		if (current->state == READY) {
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

	trace_eventSchedExit(cpuId);

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
	t->utcb.kw = NULL;
	t->utcb.w = NULL;
	t->utcb.p = NULL;

	t->priorityBase = priority;
	t->priority = priority;
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


static void _thread_interrupt(thread_t *t)
{
	_proc_threadDequeue(t);
	hal_cpuSetReturnValue(t->context, (void *)-EINTR);
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


static void _proc_threadEnqueue(thread_t **queue, time_t timeout, int interruptible)
{
	thread_t *current;

	if (*queue == wakeupPending) {
		(*queue) = NULL;
		return;
	}

	current = _proc_current();

	LIST_ADD_EX(queue, current, qnext, qprev);

	current->state = SLEEP;
	current->wakeup = 0;
	current->wait = queue;
	current->interruptible = interruptible;

	if (timeout) {
		current->wakeup = timeout;
		lib_rbInsert(&threads_common.sleeping, &current->sleeplinkage);
		_threads_updateWakeup(_proc_gettimeRaw(), NULL);
	}

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
	cpu_context_t *signalCtx;
	void *f;
	void *kstackTop;
	thread_t *thread;

	hal_spinlockSet(&threads_common.spinlock, &sc);
	thread = _proc_current();

	kstackTop = thread->kstack + thread->kstacksz;
	signalCtx = (void *)((char *)hal_cpuGetUserSP(ctx) - sizeof(*signalCtx));
	hal_cpuSetReturnValue(ctx, retval);

	if (_threads_checkSignal(thread, thread->process, signalCtx, thread->sigmask, SIG_SRC_SCALL) == 0) {
		f = thread->process->sighandler;
		hal_spinlockClear(&threads_common.spinlock, &sc);
		hal_jmp(f, kstackTop, hal_cpuGetUserSP(signalCtx), 0, NULL);
		/* no return */
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
	lib_rbInit(&threads_common.passive, threads_sleepcmp, NULL);
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
	if (p->threads == NULL) {
		return 1;
	}
	if (p->caller != NULL) {
		return 2;
	}
	if (p->threads->sched != NULL) {
		return 3;
	}
	if (threads_getHighestPrio(caller->priority) != caller->priority) {
		return 4;
	}

	return 0;
}
#else
static inline int _mustSlowCall(port_t *p, thread_t *caller)
{
	LIB_ASSERT(p != NULL, "p null??");


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

	return p->fpThreads == NULL ||
			// p->caller != NULL ||
			/* if recv is an active server, check priority */
			(p->fpThreads->sched != NULL) ||
			// (p->threads->sched != NULL && caller->priority < p->threads->priority) ||
			threads_getHighestPrio(caller->priority) != caller->priority;
}
#endif


int proc_call(u32 port, msg_t *msg)
{
	port_t *p;
	thread_t *caller;
	spinlock_ctx_t sc;
	int err;

	trace_eventIPCEnter();

	if (msg == NULL) {
		return -EINVAL;
	}

	p = proc_portGet(port);
	if (p == NULL) {
		return -EINVAL;
	}

	caller = proc_current();

	if (caller->utcb.kw == NULL) {
		return -EINVAL;
	}

	hal_spinlockSet(&p->spinlock, &sc);

	if (p->closed != 0) {
		hal_spinlockClear(&p->spinlock, &sc);
		port_put(p, 0);
		LIB_ASSERT(0, "happens here");
		return -EINVAL;
	}

	while ((err = _mustSlowCall(p, caller)) != 0) {
		log_err("sleep (%d)\n", err);
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

	// log_err("fast (prio=%d, hprio=%d)", caller->priority, threads_getHighestPrio(caller->priority));
	cpu_context_t *ctx;
	spinlock_ctx_t tsc;

	/*
	 * NOTE: assumes port spinlock is set so that this spinlockSet is nested and tsc
	 * context has interrupts disabled
	 */
	hal_spinlockSet(&threads_common.spinlock, &tsc);

	thread_t *recv = p->fpThreads;

	LIB_ASSERT(recv != NULL, "recv is null");
	LIB_ASSERT(caller != NULL, "null caller");
	LIB_ASSERT(recv->exit == 0, "recv exit=%d", recv->exit);
	LIB_ASSERT(recv->refs > 0, "attempting to return to refs=0 rcv? port=%d caller tid=%d recv tid=%d refs: %d",
			p->linkage.id, proc_getTid(caller), proc_getTid(recv), recv->refs);
	LIB_ASSERT(recv->utcb.kw != NULL, "what?? port=%d caller tid=%d recv tid=%d refs: %d",
			p->linkage.id, proc_getTid(caller), proc_getTid(recv), recv->refs);

	recv->utcb.kw->size = min(caller->utcb.kw->size, sizeof(recv->utcb.kw->raw));
	hal_memcpy(recv->utcb.kw->raw, caller->utcb.kw->raw, recv->utcb.kw->size);

	LIST_REMOVE_EX(&p->fpThreads, recv, tnext, tprev);

	recv->addedTo = NULL;

	/* FIXME: very fragile to refer to syscalls as hardcoded IDs */
	ctx = _threads_switchTo(recv, 0);

	trace_eventSyscallExit(104, proc_getTid(recv)); /* msgRespondAndRecv */

	hal_spinlockClear(&threads_common.spinlock, &tsc);

	/*
	 * intentionally restore tsc again to keep interrupts disabled to the end of
	 * the syscall - necessary to guarantee the atomicity of control switch
	 */
	hal_spinlockClear(&p->spinlock, &tsc);

	port_put(p, 0);

	hal_endSyscall(ctx, &sc);

	/* unreachable */
}

static int _proc_threadWakeupPrio(prio_queue_t *queue)
{
	size_t prio;
	for (prio = 0; prio < MAX_PRIO; prio++) {
		if (_proc_threadWakeup(&queue->pq[prio]) != 0) {
			return 1;
		}
	}

	/* noone to wakeup (TODO: use this value? is it needed in prio queues?) */
	queue->wakeupPending = 1;
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
}


int proc_respondAndRecv(u32 port, msg_t *msg, msg_rid_t *rid)
{
	port_t *p;
	spinlock_ctx_t sc, tsc;
	cpu_context_t *ctx;
	thread_t *caller, *recv;
	int responding;

	recv = proc_current();
	if (recv->utcb.kw == NULL) {
		return -EINVAL;
	}

	p = proc_portGet(port);
	if (p == NULL) {
		return -EINVAL;
	}

	log_debug("utcb buf type %d", proc_current()->utcb.kw[0]);

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

	responding = recv->reply != NULL ? 1 : 0;

	if (responding == 0) {
		log_err("passive %d", proc_getTid(recv));
		LIST_ADD_EX(&p->fpThreads, recv, tnext, tprev);
		recv->addedTo = p;

		/*
		 * REVISIT: kfree done under port spinlock/interrupts disabled to guarantee
		 * the recv gets inserted into p->threads once it becomes
		 * passive
		 * had kfree be done outside, the recv could get preempted and never come
		 * back as it would be unschedulable
		 */
		vm_kfree(recv->sched);
		recv->sched = NULL;

		hal_spinlockSet(&threads_common.spinlock, &sc);
		(void)_proc_threadWakeupPrio(&p->queue);
		hal_spinlockClear(&p->spinlock, &sc);

		/*
		 * the port is not put, because the passive thread still uses the port
		 * the port will get destroyed on proc_destroy
		 */

		threads_common.current[hal_cpuGetID()] = NULL;

		recv->state = BLOCKED_ON_RECV;
		recv->passive = 1;
		lib_rbInsert(&threads_common.passive, &recv->sleeplinkage);
		hal_cpuReschedule(&threads_common.spinlock, &sc);

		LIB_ASSERT_ALWAYS(0, "unreachable is reachable");
		__builtin_unreachable();
	}

	/* wake next caller if exists */
	log_err("wake next (prio=%d)", p->queue->priority);
	(void)_proc_threadWakeupPrio(&p->queue);

	/* TODO: handle caller faults */

	/*
	 * noone to reply, doing a fastpath and switching to caller thread
	 * point of no return
	 */

	log_err("fast (%d, %d)", proc_getTid(recv), recv->priority);

	hal_spinlockSet(&threads_common.spinlock, &tsc);

	/* TODO: not everything must be done under threads spinlock
	 * this was done only to catch trivial races, now we can try doing this in a
	 * smarter way
	 */

	caller = recv->reply;

	/* TODO should exit != 0 be treated in any special way? if we return back to
	 * the exiting client, it will get reaped anyway  */
	LIB_ASSERT(caller != NULL, "caller null!");
	LIB_ASSERT(caller->exit == 0, "exit=%d", caller->exit);
	LIB_ASSERT(caller->state != GHOST, "huh");


	/* TODO: pass small data via registers */
	caller->utcb.kw->size = min(recv->utcb.kw->size, sizeof(caller->utcb.kw->raw));
	hal_memcpy(caller->utcb.kw->raw, recv->utcb.kw->raw, caller->utcb.kw->size);

	LIST_ADD_EX(&p->fpThreads, recv, tnext, tprev);
	recv->addedTo = p;

	ctx = _threads_switchTo(caller, 1);

	trace_eventSyscallExit(103, proc_getTid(caller)); /* msgCall */

	hal_spinlockClear(&threads_common.spinlock, &tsc);

	/*
	 * intentionally restore tsc again to keep interrupts disabled (see note in _proc_call)
	 * the actual sc is restored in endSyscall
	 */
	hal_spinlockClear(&p->spinlock, &tsc);

	port_put(p, 0);

	trace_eventIPCExit();

	hal_endSyscall(ctx, &sc);

	/* unreachable */
}
