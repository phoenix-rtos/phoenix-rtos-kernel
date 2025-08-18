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

/* clang-format off */
enum { event_scheduling, event_enqueued, event_waking, event_preempted };
/* clang-format on */

const struct lockAttr proc_lockAttrDefault = { .type = PH_LOCK_NORMAL };

/* Special empty queue value used to wakeup next enqueued thread. This is used to implement sticky conditions */
static thread_t *const wakeupPending = (void *)-1;

/* Signal default actions */
enum {
	SIGNAL_TERMINATE = 0,
	SIGNAL_TERMINATE_THREAD,
	SIGNAL_IGNORE,
};

static struct {
	vm_map_t *kmap;
	spinlock_t spinlock;
	lock_t lock;
	thread_t *ready[8];
	thread_t **current;
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

	thread_t *ghosts;
	thread_t *reaper;

	/* Debug */
	unsigned char stackCanary[16];
	time_t prev;
} threads_common;


_Static_assert(sizeof(threads_common.ready) / sizeof(threads_common.ready[0]) <= (u8)-1, "queue size must fit into priority type");

#define MAX_PRIO ((u8)(sizeof(threads_common.ready) / sizeof(threads_common.ready[0])) - 1U)


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

	if (type == event_waking || type == event_preempted) {
		t->readyTime = now;
	}
	else if (type == event_scheduling) {
		wait = now - t->readyTime;

		if (t->maxWait < wait) {
			t->maxWait = wait;
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


static void proc_lockUnlock(lock_t *lock);


static void thread_destroy(thread_t *thread)
{
	process_t *process;
	spinlock_ctx_t sc;

	trace_eventThreadEnd(thread);

	/* No need to protect thread->locks access with threads_common.spinlock */
	/* The destroyed thread is a ghost and no thread (except for the current one) can access it */
	while (thread->locks != NULL) {
		proc_lockUnlock(thread->locks);
	}

	if (thread->execdata != NULL) {
		thread->kstack = thread->execkstack;
		proc_vforkedDied(thread, FORKED);
	}
	vm_kfree(thread->kstack);

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

	if (current != NULL) {
		current->cpuTime += now - current->lastTime;
		current->lastTime = now;
	}

	if (selected != NULL && current != selected) {
		selected->lastTime = now;
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


/* parasoft-suppress-next-line MISRAC2012-RULE_8_4 "Function is used externally within assembler code" */
int _threads_schedule(unsigned int n, cpu_context_t *context, void *arg)
{
	thread_t *current, *selected;
	unsigned int i;
	process_t *proc;
	cpu_context_t *signalCtx, *selCtx;
	unsigned int cpuId = hal_cpuGetID();

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
			LIST_ADD(&threads_common.ready[current->priority], current);
			_threads_preempted(current);
		}
	}

	/* Get next thread */
	i = 0;
	while (i < sizeof(threads_common.ready) / sizeof(thread_t *)) {
		selected = threads_common.ready[i];
		if (selected == NULL) {
			i++;
			continue;
		}

		LIST_REMOVE(&threads_common.ready[i], selected);

		if (selected->exit == 0U) {
			break;
		}

		if ((hal_cpuSupervisorMode(selected->context) != 0) && (selected->exit < THREAD_END_NOW)) {
			break;
		}

		selected->state = GHOST;
		LIST_ADD(&threads_common.ghosts, selected);
		(void)_proc_threadWakeup(&threads_common.reaper);
	}

	LIB_ASSERT(selected != NULL, "no threads to schedule");

	if (selected != NULL) {
		threads_common.current[hal_cpuGetID()] = selected;
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

		if (selected->tls.tls_base != 0U) {
			hal_cpuTlsSet(&selected->tls, selCtx);
		}

		_threads_scheduling(selected);
		hal_cpuRestore(context, selCtx);

#if defined(STACK_CANARY) || !defined(NDEBUG)
		if ((selected->execkstack == NULL) && (selected->context == selCtx)) {
			LIB_ASSERT_ALWAYS((char *)selCtx > ((char *)selected->kstack + selected->kstacksz - 9U * selected->kstacksz / 10U),
					"pid: %d, tid: %d, kstack: 0x%p, context: 0x%p, kernel stack limit exceeded",
					(selected->process != NULL) ? process_getPid(selected->process) : 0, proc_getTid(selected),
					selected->kstack, selCtx);
		}

		LIB_ASSERT_ALWAYS((selected->process == NULL) || (selected->ustack == NULL) ||
						(hal_memcmp(selected->ustack, threads_common.stackCanary, sizeof(threads_common.stackCanary)) == 0),
				"pid: %d, tid: %d, path: %s, user stack corrupted",
				process_getPid(selected->process), proc_getTid(selected), selected->process->path);
#endif
	}

	/* Update CPU usage */
	_threads_cpuTimeCalc(current, selected);

	trace_eventSchedExit(cpuId);

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
	thread_t *current;

	current = threads_common.current[hal_cpuGetID()];

	return current;
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
	t->exit = 0;
	t->execdata = NULL;
	t->wait = NULL;
	t->locks = NULL;
	t->stick = 0;
	t->utick = 0;
	t->priorityBase = priority;
	t->priority = priority;
	t->cpuTime = 0;
	t->maxWait = 0;
	proc_gettime(&t->startTime, NULL);
	t->lastTime = t->startTime;
	t->longjmpctx = NULL;

	if (thread_alloc(t) < 0) {
		vm_kfree(t->kstack);
		vm_kfree(t);
		return -ENOMEM;
	}

	if (process != NULL && (process->tls.tdata_sz != 0U || process->tls.tbss_sz != 0U)) {
		err = process_tlsInit(&t->tls, &process->tls, process->mapp);
		if (err != EOK) {
			lib_idtreeRemove(&threads_common.id, &t->idlinkage);
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
	LIST_ADD(&threads_common.ready[priority], t);

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
			thread = thread->next;
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
	u8 ret = _proc_threadGetLockPriority(thread);
	return (ret < thread->priorityBase) ? ret : thread->priorityBase;
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
			if (thread == threads_common.current[i]) {
				break;
			}
		}

		if (i == hal_cpuGetCount()) {
			LIB_ASSERT(LIST_BELONGS(&threads_common.ready[thread->priority], thread) != 0,
					"thread: 0x%p, tid: %d, priority: %d, is not on the ready list",
					thread, proc_getTid(thread), thread->priority);
			LIST_REMOVE(&threads_common.ready[thread->priority], thread);
			LIST_ADD(&threads_common.ready[priority], thread);
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
	}

	ret = (int)current->priorityBase;

	if (reschedule != 0) {
		(void)hal_cpuReschedule(&threads_common.spinlock, &sc);
	}
	else {
		(void)hal_spinlockClear(&threads_common.spinlock, &sc);
	}

	trace_eventThreadPriority(proc_getTid(current), current->priority);

	return ret;
}


static void _thread_interrupt(thread_t *t)
{
	_proc_threadDequeue(t);
	hal_cpuSetReturnValue(t->context, (void *)-EINTR);
}


__attribute__((noreturn)) void proc_threadEnd(void)
{
	thread_t *t;
	int cpu;
	spinlock_ctx_t sc;

	(void)hal_spinlockSet(&threads_common.spinlock, &sc);

	cpu = (int)hal_cpuGetID();
	t = threads_common.current[cpu];
	threads_common.current[cpu] = NULL;
	t->state = GHOST;
	LIST_ADD(&threads_common.ghosts, t);
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


static void _proc_threadsDestroy(thread_t **threads, const thread_t *except)
{
	thread_t *t;

	t = *threads;
	if (t != NULL) {
		do {
			if (t != except) {
				_proc_threadExit(t);
			}
			/* parasoft-suppress-next-line MISRAC2012-DIR_4_1 "procnext is never NULL, and *threads is checked earlier" */
			t = t->procnext;
		} while (t != *threads);
	}
}


void proc_threadsDestroy(thread_t **threads, const thread_t *except)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&threads_common.spinlock, &sc);
	_proc_threadsDestroy(threads, except);
	hal_spinlockClear(&threads_common.spinlock, &sc);
}


void proc_reap(void)
{
	thread_t *ghost;
	spinlock_ctx_t sc;

	hal_spinlockSet(&threads_common.spinlock, &sc);
	while (threads_common.ghosts == NULL) {
		(void)_proc_threadWait(&threads_common.reaper, 0, &sc);
	}
	ghost = threads_common.ghosts;
	LIST_REMOVE(&threads_common.ghosts, ghost);
	hal_spinlockClear(&threads_common.spinlock, &sc);

	threads_put(ghost);
}


void proc_kill(process_t *proc)
{
	spinlock_ctx_t sc;
	hal_spinlockSet(&threads_common.spinlock, &sc);
	_proc_threadsDestroy(&proc->threads, NULL);
	hal_spinlockClear(&threads_common.spinlock, &sc);
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

	_threads_waking(t);

	if (t->wait != NULL) {
		LIST_REMOVE(t->wait, t);
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
		if (t == threads_common.current[i]) {
			break;
		}
	}

	if (i == hal_cpuGetCount()) {
		LIST_ADD(&threads_common.ready[t->priority], t);
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

	LIST_ADD(queue, current);

	current->state = SLEEP;
	current->wakeup = 0;
	current->wait = queue;
	current->interruptible = interruptible & 0x1U;

	if (timeout != 0) {
		current->wakeup = timeout;
		(void)lib_rbInsert(&threads_common.sleeping, &current->sleeplinkage);
		_threads_updateWakeup(_proc_gettimeRaw(), NULL);
	}

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


static int threads_sigmutable(int sig)
{
	switch (sig) {
		/* POSIX: SIGKILL and SIGSTOP cannot be caught or ignored */
		case SIGKILL:
		case SIGSTOP:
		case PH_SIGCANCEL:
		case SIGNULL:
			return 0;
		default:
			return ((sig < 0) || (sig > NSIG)) ? 0 : 1;
	}
}


static int _threads_sigdefault(process_t *process, thread_t *thread, int sig)
{
	switch (sig) {
		case SIGHUP:
		case SIGINT:
		case SIGQUIT:
		case SIGILL:
		case SIGTRAP:
		case SIGABRT: /* And SIGIOT */
		case SIGEMT:
		case SIGFPE:
		case SIGBUS:
		case SIGSEGV:
		case SIGSYS:
		case SIGPIPE:
		case SIGALRM:
		case SIGTERM:
		case SIGIO:
		case SIGXCPU:
		case SIGXFSZ:
		case SIGVTALRM:
		case SIGPROF:
		case SIGUSR1:
		case SIGUSR2:
		case SIGKILL:
			process->exit = sig * (int)(1UL << 8U);
			_proc_threadsDestroy(&process->threads, NULL);
			return SIGNAL_TERMINATE;

		case SIGURG:
		case SIGCHLD:
		case SIGWINCH:
		case SIGINFO:
		case SIGCONT: /* TODO: Continue process. */
		case SIGTSTP: /* TODO: Stop process. */
		case SIGTTIN: /* TODO: Stop process. */
		case SIGTTOU: /* TODO: Stop process. */
		case SIGSTOP: /* TODO: Stop process. */
		case SIGNULL:
			return SIGNAL_IGNORE;

		case PH_SIGCANCEL:
			if (thread != NULL) {
				_proc_threadExit(thread);
			}
			return SIGNAL_TERMINATE_THREAD;

		default:
			return -EINVAL;
	}
}


int threads_sigpost(process_t *process, thread_t *thread, int sig)
{
	u32 sigbit = (u32)1U << (unsigned int)sig;

	spinlock_ctx_t sc;

	hal_spinlockSet(&threads_common.spinlock, &sc);

	if ((sig < 0) || (sig > NSIG)) {
		hal_spinlockClear(&threads_common.spinlock, &sc);
		return -EINVAL;
	}

	if (sig == PH_SIGCANCEL || sig == SIGNULL) {
		(void)_threads_sigdefault(process, thread, sig);
		hal_spinlockClear(&threads_common.spinlock, &sc);
		return EOK;
	}

	/* parasoft-suppress-next-line MISRAC2012-RULE_11_1-a "POSIX compliant definition" */
	if ((process->sigactions != NULL) && (process->sigactions[sig - 1].sa_handler == SIG_IGN)) {
		hal_spinlockClear(&threads_common.spinlock, &sc);
		return EOK;
	}

	if (thread != NULL) {
		thread->sigpend |= sigbit;
	}
	else {
		process->sigpend |= sigbit;
		thread = process->threads;

		if (thread != NULL) {
			do {
				if ((sigbit & ~thread->sigmask) != 0U) {
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

	if ((sigbit & ~thread->sigmask) != 0U) {
		if (thread->interruptible != 0U) {
			_thread_interrupt(thread);
		}

		if ((process->sigactions == NULL) || (process->sigactions[sig - 1].sa_handler == SIG_DFL)) {
			(void)_threads_sigdefault(process, thread, sig);
			thread->sigpend &= ~sigbit;
			process->sigpend &= ~sigbit;
		}
	}

	(void)hal_cpuReschedule(&threads_common.spinlock, &sc);

	return EOK;
}


static int _threads_checkSignal(thread_t *selected, process_t *proc, cpu_context_t *signalCtx, unsigned int oldmask, const int src)
{
#ifndef KERNEL_SIGNALS_DISABLE

	unsigned int sig;
	sighandler_t handler;
	int defaultAction;

	sig = (selected->sigpend | proc->sigpend) & ~selected->sigmask;
	while (sig != 0U) {
		sig = hal_cpuGetLastBit(sig);
		handler = (proc->sigactions == NULL) ? SIG_DFL : proc->sigactions[sig - 1U].sa_handler;

		if (handler == SIG_DFL) {
			defaultAction = _threads_sigdefault(proc, selected, (int)sig);
		}
		else {
			defaultAction = -1;
		}

		if ((defaultAction == SIGNAL_TERMINATE) || (defaultAction == SIGNAL_TERMINATE_THREAD)) {
			return -1;
		}

		/* parasoft-suppress-next-line MISRAC2012-RULE_11_1-a "POSIX compliant definition" */
		if ((handler == SIG_IGN) || (defaultAction == SIGNAL_IGNORE) || (defaultAction == -EINVAL)) {
			selected->sigpend &= ~(u32)(1UL << sig);
			proc->sigpend &= ~(u32)(1UL << sig);

			/* Check for other signals */
			sig = (selected->sigpend | proc->sigpend) & ~selected->sigmask;
			continue;
		}

		/* POSIX: sa_mask should be ORed with current process signal mask */
		selected->sigmask |= proc->sigactions[sig - 1U].sa_mask;
		if (((unsigned int)proc->sigactions[sig - 1U].sa_flags & SA_NODEFER) == 0U) {
			selected->sigmask |= (u32)(1UL << sig);
		}
		/* TODO: Handle other sa_flags */

		if (hal_cpuPushSignal(selected->kstack + selected->kstacksz, proc->sigtrampoline, handler, signalCtx, (int)sig, oldmask, src) == 0) {
			selected->sigpend &= ~(u32)(1UL << sig);
			proc->sigpend &= ~(u32)(1UL << sig);
			return 0;
		}
	}

#endif

	return -1;
}


int threads_setSigaction(int sig, sigtrampolineFn_t trampoline, const struct sigaction *act, struct sigaction *old)
{
	process_t *process;
	struct sigaction *sa;
	spinlock_ctx_t sc;

	if ((sig <= 0) || (sig > NSIG)) {
		return -EINVAL;
	}

	if ((act != NULL) && (threads_sigmutable(sig) == 0)) {
		return -EINVAL;
	}

	hal_spinlockSet(&threads_common.spinlock, &sc);
	process = _proc_current()->process;

	/* allocate sigactions array if required */
	if ((act != NULL) && (process->sigactions == NULL) && (act->sa_handler != SIG_DFL)) {
		hal_spinlockClear(&threads_common.spinlock, &sc);
		sa = vm_kmalloc(sizeof(struct sigaction) * (u8)(NSIG - 1));
		if (sa == NULL) {
			return -ENOMEM;
		}

		hal_spinlockSet(&threads_common.spinlock, &sc);
		/* for a running process this array should never get freed, but allocation race can happen here */
		if (process->sigactions == NULL) {
			hal_memset(sa, 0, sizeof(struct sigaction) * (u8)(NSIG - 1));
			process->sigactions = sa;
		}
		else {
			hal_spinlockClear(&threads_common.spinlock, &sc);
			vm_kfree(sa);
			hal_spinlockSet(&threads_common.spinlock, &sc);
		}
	}

	if (old != NULL) {
		/* sigactions can be null if act.sa_handler == SIG_DFL */
		if (process->sigactions == NULL) {
			old->sa_handler = SIG_DFL;
			old->sa_flags = 0;
			old->sa_mask = 0;
		}
		else {
			hal_memcpy(old, &process->sigactions[sig - 1], sizeof(struct sigaction));
		}
	}

	/* sigactions can be null if act.sa_handler == SIG_DFL */
	if ((act != NULL) && (process->sigactions != NULL)) {
		hal_memcpy(&process->sigactions[sig - 1], act, sizeof(struct sigaction));
	}

	process->sigtrampoline = trampoline;

	hal_spinlockClear(&threads_common.spinlock, &sc);
	return 0;
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
		/* parasoft-suppress-next-line MISRAC2012-RULE_11_1 "f is passed to function hal_jmp which need void * type" */
		f = thread->process->sigtrampoline;
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
		/* parasoft-suppress-next-line MISRAC2012-RULE_11_1 "f is passed to function hal_jmp which need void * type" */
		f = thread->process->sigtrampoline;
		hal_spinlockClear(&threads_common.spinlock, &sc);
		hal_jmp(f, kstackTop, hal_cpuGetUserSP(signalCtx), 0, NULL);
		/* no return */
	}

	/* check if thread wasn't killed by signal */
	if (thread->exit != 0U) {
		hal_spinlockClear(&threads_common.spinlock, &sc);
		proc_threadEnd();
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
		f = thread->process->sigtrampoline;
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
					_trace_eventLockSetExit(lock, tid, ret);
					return ret;
				}
				/* Don't return EINTR if we got lock anyway */
				if (lock->owner != current) {
					hal_spinlockSet(&threads_common.spinlock, &sc);

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


static int _proc_lockUnlock(lock_t *lock)
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

	if ((lock->attr.type == PH_LOCK_ERRORCHECK) || (lock->attr.type == PH_LOCK_RECURSIVE)) {
		if (lock->owner != current) {
			hal_spinlockClear(&threads_common.spinlock, &sc);
			return -EPERM;
		}
	}

	if ((lock->attr.type == PH_LOCK_RECURSIVE) && (lock->depth > 0U)) {
		lock->depth--;
		if (lock->depth != 0U) {
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
		(void)hal_cpuReschedule(NULL, NULL);
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
	spinlock_ctx_t sc;

	hal_spinlockSet(&lock->spinlock, &sc);

	if (lock->owner != NULL) {
		(void)_proc_lockUnlock(lock);
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
	thread_t *t;
	spinlock_ctx_t sc;

	/* Strictly needed - no lock can be taken
	 * while threads_common.spinlock is being
	 * held! */
	log_disable();

	lib_printf("threads: ");
	hal_spinlockSet(&threads_common.spinlock, &sc);

	t = threads_common.ready[priority];
	do {
		lib_printf("[%p] ", t);

		if (t == NULL) {
			break;
		}

		t = t->next;
	} while (t != threads_common.ready[priority]);
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
		if (now != t->startTime) {
			tinfo.load = (int)((t->cpuTime * 1000) / (now - t->startTime));
		}
		else {
			tinfo.load = 0;
		}
		tinfo.cpuTime = t->cpuTime;

		if (t->state == READY && t->maxWait < now - t->readyTime) {
			tinfo.wait = now - t->readyTime;
		}
		else {
			tinfo.wait = t->maxWait;
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
	threads_common.current = (thread_t **)vm_kmalloc(sizeof(thread_t *) * hal_cpuGetCount());
	if (threads_common.current == NULL) {
		return -ENOMEM;
	}

	/* Run idle thread on every cpu */
	for (i = 0; i < hal_cpuGetCount(); i++) {
		threads_common.current[i] = NULL;
		(void)proc_threadCreate(NULL, threads_idlethr, NULL, MAX_PRIO, (size_t)SIZE_KSTACK, NULL, 0, NULL);
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
