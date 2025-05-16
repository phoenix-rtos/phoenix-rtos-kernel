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


const struct lockAttr proc_lockAttrDefault = { .type = PH_LOCK_NORMAL };

/* Special empty queue value used to wakeup next enqueued thread. This is used to implement sticky conditions */
static thread_t *const wakeupPending = (void *)-1;

struct {
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

	thread_t *volatile ghosts;
	thread_t *reaper;

	int perfGather;
	time_t perfLastTimestamp;
	cbuffer_t perfBuffer;
	page_t *perfPages;

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


static unsigned perf_idpack(unsigned id)
{
	return id >> 8;
}


/* Note: always called with threads_common.spinlock set */
static void _perf_event(thread_t *t, int type)
{
	perf_event_t ev;
	time_t now = 0, wait;

	now = _proc_gettimeRaw();

	if (type == perf_evWaking || type == perf_evPreempted) {
		t->readyTime = now;
	}
	else if (type == perf_evScheduling) {
		wait = now - t->readyTime;

		if (t->maxWait < wait)
			t->maxWait = wait;
	}

	if (!threads_common.perfGather)
		return;

	ev.type = type;

	ev.deltaTimestamp = now - threads_common.perfLastTimestamp;
	threads_common.perfLastTimestamp = now;
	ev.tid = perf_idpack(proc_getTid(t));

	_cbuffer_write(&threads_common.perfBuffer, &ev, sizeof(ev));
}


static void _perf_scheduling(thread_t *t)
{
	_perf_event(t, perf_evScheduling);
}


static void _perf_preempted(thread_t *t)
{
	_perf_event(t, perf_evPreempted);
}


static void _perf_enqueued(thread_t *t)
{
	_perf_event(t, perf_evEnqueued);
}


static void _perf_waking(thread_t *t)
{
	_perf_event(t, perf_evWaking);
}


static void _perf_begin(thread_t *t)
{
	perf_levent_begin_t ev;
	time_t now;

	if (!threads_common.perfGather)
		return;

	ev.sbz = 0;
	ev.type = perf_levBegin;
	ev.prio = t->priority;
	ev.tid = perf_idpack(proc_getTid(t));
	ev.pid = t->process != NULL ? perf_idpack(process_getPid(t->process)) : -1;

	now = _proc_gettimeRaw();
	ev.deltaTimestamp = now - threads_common.perfLastTimestamp;
	threads_common.perfLastTimestamp = now;

	_cbuffer_write(&threads_common.perfBuffer, &ev, sizeof(ev));
}


void perf_end(thread_t *t)
{
	perf_levent_end_t ev;
	time_t now;
	spinlock_ctx_t sc;

	if (!threads_common.perfGather)
		return;

	hal_spinlockSet(&threads_common.spinlock, &sc);
	ev.sbz = 0;
	ev.type = perf_levEnd;
	ev.tid = perf_idpack(proc_getTid(t));

	now = _proc_gettimeRaw();
	ev.deltaTimestamp = now - threads_common.perfLastTimestamp;
	threads_common.perfLastTimestamp = now;

	_cbuffer_write(&threads_common.perfBuffer, &ev, sizeof(ev));
	hal_spinlockClear(&threads_common.spinlock, &sc);
}


void perf_fork(process_t *p)
{
	perf_levent_fork_t ev;
	time_t now;
	spinlock_ctx_t sc;

	if (!threads_common.perfGather)
		return;

	hal_spinlockSet(&threads_common.spinlock, &sc);
	ev.sbz = 0;
	ev.type = perf_levFork;
	ev.pid = perf_idpack(process_getPid(p));
	// ev.ppid = p->parent != NULL ? perf_idpack(p->parent->id) : -1;
	ev.tid = perf_idpack(proc_getTid(_proc_current()));

	now = _proc_gettimeRaw();
	ev.deltaTimestamp = now - threads_common.perfLastTimestamp;
	threads_common.perfLastTimestamp = now;

	_cbuffer_write(&threads_common.perfBuffer, &ev, sizeof(ev));
	hal_spinlockClear(&threads_common.spinlock, &sc);
}


void perf_kill(process_t *p)
{
	perf_levent_kill_t ev;
	time_t now;
	spinlock_ctx_t sc;

	if (!threads_common.perfGather)
		return;

	hal_spinlockSet(&threads_common.spinlock, &sc);
	ev.sbz = 0;
	ev.type = perf_levKill;
	ev.pid = perf_idpack(process_getPid(p));
	ev.tid = perf_idpack(proc_getTid(_proc_current()));

	now = _proc_gettimeRaw();
	ev.deltaTimestamp = now - threads_common.perfLastTimestamp;
	threads_common.perfLastTimestamp = now;

	_cbuffer_write(&threads_common.perfBuffer, &ev, sizeof(ev));
	hal_spinlockClear(&threads_common.spinlock, &sc);
}


void perf_exec(process_t *p, char *path)
{
	perf_levent_exec_t ev;
	time_t now;
	int plen;
	spinlock_ctx_t sc;

	if (!threads_common.perfGather)
		return;

	hal_spinlockSet(&threads_common.spinlock, &sc);
	ev.sbz = 0;
	ev.type = perf_levExec;
	ev.tid = perf_idpack(proc_getTid(_proc_current()));

	plen = hal_strlen(path);
	plen = min(plen, sizeof(ev.path) - 1);
	hal_memcpy(ev.path, path, plen);
	ev.path[plen] = 0;

	now = _proc_gettimeRaw();
	ev.deltaTimestamp = now - threads_common.perfLastTimestamp;
	threads_common.perfLastTimestamp = now;

	_cbuffer_write(&threads_common.perfBuffer, &ev, sizeof(ev) - sizeof(ev.path) + plen + 1);
	hal_spinlockClear(&threads_common.spinlock, &sc);
}


static void perf_bufferFree(void *data, page_t **pages)
{
	size_t sz = 0;
	page_t *p;

	while ((p = *pages) != NULL) {
		*pages = p->next;
		vm_pageFree(p);
		sz += SIZE_PAGE;
	}

	vm_munmap(threads_common.kmap, data, sz);
}


static void *perf_bufferAlloc(page_t **pages, size_t sz)
{
	page_t *p;
	void *v, *data;

	*pages = NULL;
	data = vm_mapFind(threads_common.kmap, NULL, sz, MAP_NONE, PROT_READ | PROT_WRITE);

	if (data == NULL)
		return NULL;

	for (v = data; v < data + sz; v += SIZE_PAGE) {
		p = vm_pageAlloc(SIZE_PAGE, PAGE_OWNER_APP);

		if (p == NULL) {
			perf_bufferFree(data, pages);
			return NULL;
		}

		p->next = *pages;
		*pages = p;
		page_map(&threads_common.kmap->pmap, v, p->addr, PGHD_PRESENT | PGHD_WRITE | PGHD_READ);
	}

	return data;
}


int perf_start(unsigned pid)
{
	void *data;
	spinlock_ctx_t sc;

	if (!pid)
		return -EINVAL;

	if (threads_common.perfGather)
		return -EINVAL;

	/* Allocate 4M for events */
	data = perf_bufferAlloc(&threads_common.perfPages, 4 << 20);

	if (data == NULL)
		return -ENOMEM;

	_cbuffer_init(&threads_common.perfBuffer, data, 4 << 20);

	/* Start gathering events */
	hal_spinlockSet(&threads_common.spinlock, &sc);
	threads_common.perfGather = 1;
	threads_common.perfLastTimestamp = _proc_gettimeRaw();
	hal_spinlockClear(&threads_common.spinlock, &sc);

	return EOK;
}


int perf_read(void *buffer, size_t bufsz)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&threads_common.spinlock, &sc);
	bufsz = _cbuffer_read(&threads_common.perfBuffer, buffer, bufsz);
	hal_spinlockClear(&threads_common.spinlock, &sc);

	return bufsz;
}


int perf_finish()
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&threads_common.spinlock, &sc);
	if (threads_common.perfGather) {
		threads_common.perfGather = 0;
		hal_spinlockClear(&threads_common.spinlock, &sc);

		perf_bufferFree(threads_common.perfBuffer.data, &threads_common.perfPages);
	}
	else
		hal_spinlockClear(&threads_common.spinlock, &sc);

	return EOK;
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


static void thread_destroy(thread_t *thread)
{
	process_t *process;
	spinlock_ctx_t sc;

	perf_end(thread);

	/* No need to protect thread->locks access with threads_common.spinlock */
	/* The destroyed thread is a ghost and no thread (except for the current one) can access it */
	while (thread->locks != NULL) {
		proc_lockUnlock(thread->locks);
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

	if (current != NULL) {
		current->cpuTime += now - current->lastTime;
		current->lastTime = now;
	}

	if (selected != NULL && current != selected)
		selected->lastTime = now;
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


int _threads_schedule(unsigned int n, cpu_context_t *context, void *arg)
{
	thread_t *current, *selected;
	unsigned int i;
	process_t *proc;
	cpu_context_t *signalCtx, *selCtx;

	(void)arg;
	(void)n;
	hal_lockScheduler();

	current = _proc_current();
	threads_common.current[hal_cpuGetID()] = NULL;

	/* Save current thread context */
	if (current != NULL) {
		current->context = context;

		/* Move thread to the end of queue */
		if (current->state == READY) {
			LIST_ADD(&threads_common.ready[current->priority], current);
			_perf_preempted(current);
		}
	}

	/* Get next thread */
	for (i = 0; i < sizeof(threads_common.ready) / sizeof(thread_t *);) {
		if ((selected = threads_common.ready[i]) == NULL) {
			i++;
			continue;
		}

		LIST_REMOVE(&threads_common.ready[i], selected);

		if (selected->exit == 0) {
			break;
		}

		if ((hal_cpuSupervisorMode(selected->context) != 0) && (selected->exit < THREAD_END_NOW)) {
			break;
		}

		selected->state = GHOST;
		LIST_ADD(&threads_common.ghosts, selected);
		_proc_threadWakeup(&threads_common.reaper);
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

		if (selected->tls.tls_base != NULL) {
			hal_cpuTlsSet(&selected->tls, selCtx);
		}

		_perf_scheduling(selected);
		hal_cpuRestore(context, selCtx);

#if defined(STACK_CANARY) || !defined(NDEBUG)
		if ((selected->execkstack == NULL) && (selected->context == selCtx)) {
			LIB_ASSERT_ALWAYS((char *)selCtx > ((char *)selected->kstack + selected->kstacksz - 9 * selected->kstacksz / 10),
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

	if (process != NULL && (process->tls.tdata_sz != 0 || process->tls.tbss_sz != 0)) {
		err = process_tlsInit(&t->tls, &process->tls, process->mapp);
		if (err != EOK) {
			lib_idtreeRemove(&threads_common.id, &t->idlinkage);
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
	/* Insert thread to scheduler queue */

	_perf_begin(t);
	_perf_waking(t);
	LIST_ADD(&threads_common.ready[priority], t);

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
			thread = thread->next;
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
}


int proc_threadPriority(int priority)
{
	thread_t *current;
	spinlock_ctx_t sc;
	int ret;

	if (priority < -1) {
		return -EINVAL;
	}

	if ((priority >= 0) && (priority >= sizeof(threads_common.ready) / sizeof(threads_common.ready[0]))) {
		return -EINVAL;
	}

	hal_spinlockSet(&threads_common.spinlock, &sc);

	current = _proc_current();
	if (priority >= 0) {
		if ((priority < current->priority) || (current->locks == NULL)) {
			current->priority = priority;
		}
		current->priorityBase = priority;
	}
	ret = current->priorityBase;

	hal_spinlockClear(&threads_common.spinlock, &sc);

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
	t = threads_common.current[cpu];
	threads_common.current[cpu] = NULL;
	t->state = GHOST;
	LIST_ADD(&threads_common.ghosts, t);
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


void proc_threadsDestroy(thread_t **threads)
{
	thread_t *t;
	spinlock_ctx_t sc;

	hal_spinlockSet(&threads_common.spinlock, &sc);
	if ((t = *threads) != NULL) {
		do
			_proc_threadExit(t);
		while ((t = t->procnext) != *threads);
	}
	hal_spinlockClear(&threads_common.spinlock, &sc);
}


void proc_reap(void)
{
	thread_t *ghost;
	spinlock_ctx_t sc;

	hal_spinlockSet(&threads_common.spinlock, &sc);
	while (threads_common.ghosts == NULL) {
		_proc_threadWait(&threads_common.reaper, 0, &sc);
	}
	ghost = threads_common.ghosts;
	LIST_REMOVE(&threads_common.ghosts, ghost);
	hal_spinlockClear(&threads_common.spinlock, &sc);

	threads_put(ghost);
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

	_perf_waking(t);

	if (t->wait != NULL) {
		LIST_REMOVE(t->wait, t);
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
		if (t == threads_common.current[i]) {
			break;
		}
	}

	if (i == hal_cpuGetCount()) {
		LIST_ADD(&threads_common.ready[t->priority], t);
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

	LIST_ADD(queue, current);

	current->state = SLEEP;
	current->wakeup = 0;
	current->wait = queue;
	current->interruptible = interruptible;

	if (timeout) {
		current->wakeup = timeout;
		lib_rbInsert(&threads_common.sleeping, &current->sleeplinkage);
		_threads_updateWakeup(_proc_gettimeRaw(), NULL);
	}

	_perf_enqueued(current);
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

		_perf_enqueued(current);
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
	int ret;

	hal_spinlockSet(&threads_common.spinlock, &sc);

	current = _proc_current();

	if ((lock->attr.type == PH_LOCK_ERRORCHECK) && (lock->owner == current)) {
		hal_spinlockClear(&threads_common.spinlock, &sc);
		return -EDEADLK;
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
					return -EINTR;
				}
				/* Don't return EINTR if we got lock anyway */
				if (lock->owner != current) {
					hal_spinlockSet(&threads_common.spinlock, &sc);

					/* Recalculate lock owner priority (it might have been inherited from the current thread) */
					_proc_threadSetPriority(lock->owner, _proc_threadGetPriority(lock->owner));

					hal_spinlockClear(&threads_common.spinlock, &sc);

					return -EINTR;
				}
			}
		} while (lock->owner != current);
	}
	else {
		hal_spinlockClear(&threads_common.spinlock, &sc);
	}

	lock->depth = 1;

	return EOK;
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

	hal_memcpy(&lock->attr, attr, sizeof(struct lockAttr));

	return EOK;
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

		if (t == NULL)
			break;

		t = t->next;
	} while (t != threads_common.ready[priority]);
	hal_spinlockClear(&threads_common.spinlock, &sc);

	lib_printf("\n");


	return;
}


int proc_threadsList(int n, threadinfo_t *info)
{
	int i = 0, len, argc, space;
	thread_t *t;
	map_entry_t *entry;
	vm_map_t *map;
	time_t now;
	char *name;
	spinlock_ctx_t sc;

	proc_lockSet(&threads_common.lock);

	t = lib_treeof(thread_t, idlinkage, lib_rbMinimum(threads_common.id.root));

	while (i < n && t != NULL) {
		if (t->process != NULL) {
			info[i].pid = process_getPid(t->process);
			// info[i].ppid = t->process->parent != NULL ? t->process->parent->id : 0;
			info[i].ppid = 0;
		}
		else {
			info[i].pid = 0;
			info[i].ppid = 0;
		}

		hal_spinlockSet(&threads_common.spinlock, &sc);
		info[i].tid = proc_getTid(t);
		info[i].priority = t->priorityBase;
		info[i].state = t->state;

		now = _proc_gettimeRaw();
		if (now != t->startTime)
			info[i].load = (t->cpuTime * 1000) / (now - t->startTime);
		else
			info[i].load = 0;
		info[i].cpuTime = t->cpuTime;

		if (t->state == READY && t->maxWait < now - t->readyTime)
			info[i].wait = now - t->readyTime;
		else
			info[i].wait = t->maxWait;
		hal_spinlockClear(&threads_common.spinlock, &sc);

		if (t->process != NULL) {
			map = t->process->mapp;

			if (t->process->path != NULL) {
				space = sizeof(info[i].name);
				name = info[i].name;

				if (t->process->argv != NULL) {
					for (argc = 0; t->process->argv[argc] != NULL && space > 0; ++argc) {
						len = min(hal_strlen(t->process->argv[argc]) + 1, space);
						hal_memcpy(name, t->process->argv[argc], len);
						name[len - 1] = ' ';
						name += len;
						space -= len;
					}
					*(name - 1) = 0;
				}
				else {
					len = hal_strlen(t->process->path) + 1;
					hal_memcpy(info[i].name, t->process->path, min(space, len));
				}

				info[i].name[sizeof(info[i].name) - 1] = 0;
			}
			else {
				info[i].name[0] = 0;
			}
		}
		else {
			map = threads_common.kmap;
			hal_memcpy(info[i].name, "[idle]", sizeof("[idle]"));
		}

		info[i].vmem = 0;

#ifdef NOMMU
		if ((t->process != NULL) && (entry = t->process->entries) != NULL) {
			do {
				info[i].vmem += entry->size;
				entry = entry->next;
			} while (entry != t->process->entries);
		}
		else
#endif
		if (map != NULL) {
			proc_lockSet(&map->lock);
			entry = lib_treeof(map_entry_t, linkage, lib_rbMinimum(map->tree.root));

			while (entry != NULL) {
				info[i].vmem += entry->size;
				entry = lib_treeof(map_entry_t, linkage, lib_rbNext(&entry->linkage));
			}
			proc_lockClear(&map->lock);
		}

		++i;
		t = lib_idtreeof(thread_t, idlinkage, lib_idtreeNext(&t->idlinkage.linkage));
	}

	proc_lockClear(&threads_common.lock);

	return i;
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

	threads_common.perfGather = 0;

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
	if ((threads_common.current = (thread_t **)vm_kmalloc(sizeof(thread_t *) * hal_cpuGetCount())) == NULL)
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


cpu_context_t *_threads_userContext(thread_t *thread)
{
	if (hal_cpuSupervisorMode(thread->context) != 0) {
		return (cpu_context_t *)((char *)(thread->kstack + thread->kstacksz) - sizeof(cpu_context_t));
	}
	return thread->context;
}
