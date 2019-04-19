/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Thread manager
 *
 * Copyright 2012-2015, 2017, 2018 Phoenix Systems
 * Copyright 2001, 2005-2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk, Jacek Popko, Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include HAL
#include "../../include/errno.h"
#include "../../include/signal.h"
#include "threads.h"
#include "../lib/lib.h"
#include "../posix/posix.h"
#include "resource.h"
#include "msg.h"
#include "ports.h"


struct {
	vm_map_t *kmap;
	spinlock_t spinlock;
	lock_t lock;
	thread_t *ready[8];
	thread_t **current;
	volatile time_t jiffies;
	time_t utcoffs;

	unsigned int executions;

	/* Synchronized by spinlock */
	rbtree_t sleeping;

	/* Synchronized by mutex */
	unsigned int nextid;
	rbtree_t id;

#ifndef CPU_STM32
	cpu_load_t load;
#endif

	intr_handler_t timeintrHandler;
	intr_handler_t scheduleHandler;

#ifdef PENDSV_IRQ
	intr_handler_t pendsvHandler;
#endif

	thread_t *volatile ghosts;

	int perfGather;
	time_t perfLastTimestamp;
	cbuffer_t perfBuffer;
	page_t *perfPages;
} threads_common;


void proc_threadProtect() {}
void proc_threadUnprotect() {}


static int threads_sleepcmp(rbnode_t *n1, rbnode_t *n2)
{
	thread_t *t1 = lib_treeof(thread_t, sleeplinkage, n1);
	thread_t *t2 = lib_treeof(thread_t, sleeplinkage, n2);

	if (t1->wakeup > t2->wakeup)
		return 1;

	else if (t1->wakeup < t2->wakeup)
		return -1;

	else if (t1->id < t2->id)
		return -1;

	else if (t1->id > t2->id)
		return 1;

	return 0;
}


static int threads_idcmp(rbnode_t *n1, rbnode_t *n2)
{
	thread_t *t1 = lib_treeof(thread_t, idlinkage, n1);
	thread_t *t2 = lib_treeof(thread_t, idlinkage, n2);

	if (t1->id < t2->id)
		return -1;

	else if (t1->id > t2->id)
		return 1;

	return 0;
}


/*
 * Thread monitoring
 */

static inline time_t _threads_getTimer(void);
static void _proc_threadWakeup(thread_t **queue);
static int _proc_threadWait(thread_t **queue, time_t timeout);
static thread_t *_proc_current(void);


static unsigned perf_idpack(unsigned id)
{
	return id >> 8;
}


/* Note: always called with threads_common.spinlock set */
static void _perf_event(thread_t *t, int type)
{
	perf_event_t ev;
	time_t now = 0, wait;

	now = TIMER_CYC2US(_threads_getTimer());

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
	ev.tid = perf_idpack(t->id);

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
	ev.tid = perf_idpack(t->id);
	ev.pid = t->process != NULL ? perf_idpack(t->process->id) : -1;

	now = TIMER_CYC2US(_threads_getTimer());
	ev.deltaTimestamp = now - threads_common.perfLastTimestamp;
	threads_common.perfLastTimestamp = now;

	_cbuffer_write(&threads_common.perfBuffer, &ev, sizeof(ev));
}


void _perf_end(thread_t *t)
{
	perf_levent_end_t ev;
	time_t now;

	if (!threads_common.perfGather)
		return;

	ev.sbz = 0;
	ev.type = perf_levEnd;
	ev.tid = perf_idpack(t->id);

	now = TIMER_CYC2US(_threads_getTimer());
	ev.deltaTimestamp = now - threads_common.perfLastTimestamp;
	threads_common.perfLastTimestamp = now;

	_cbuffer_write(&threads_common.perfBuffer, &ev, sizeof(ev));
}


void perf_fork(process_t *p)
{
	perf_levent_fork_t ev;
	time_t now;

	if (!threads_common.perfGather)
		return;

	hal_spinlockSet(&threads_common.spinlock);
	ev.sbz = 0;
	ev.type = perf_levFork;
	ev.pid = perf_idpack(p->id);
	// ev.ppid = p->parent != NULL ? perf_idpack(p->parent->id) : -1;
	ev.tid = perf_idpack(_proc_current()->id);

	now = TIMER_CYC2US(_threads_getTimer());
	ev.deltaTimestamp = now - threads_common.perfLastTimestamp;
	threads_common.perfLastTimestamp = now;

	_cbuffer_write(&threads_common.perfBuffer, &ev, sizeof(ev));
	hal_spinlockClear(&threads_common.spinlock);
}


void perf_kill(process_t *p)
{
	perf_levent_kill_t ev;
	time_t now;

	if (!threads_common.perfGather)
		return;

	hal_spinlockSet(&threads_common.spinlock);
	ev.sbz = 0;
	ev.type = perf_levKill;
	ev.pid = perf_idpack(p->id);
	ev.tid = perf_idpack(_proc_current()->id);

	now = TIMER_CYC2US(_threads_getTimer());
	ev.deltaTimestamp = now - threads_common.perfLastTimestamp;
	threads_common.perfLastTimestamp = now;

	_cbuffer_write(&threads_common.perfBuffer, &ev, sizeof(ev));
	hal_spinlockClear(&threads_common.spinlock);
}


void perf_exec(process_t *p, char *path)
{
	perf_levent_exec_t ev;
	time_t now;
	int plen;

	if (!threads_common.perfGather)
		return;

	hal_spinlockSet(&threads_common.spinlock);
	ev.sbz = 0;
	ev.type = perf_levExec;
	ev.tid = perf_idpack(_proc_current()->id);

	plen = hal_strlen(path);
	plen = min(plen, sizeof(ev.path) - 1);
	hal_memcpy(ev.path, path, plen);
	ev.path[plen] = 0;

	now = TIMER_CYC2US(_threads_getTimer());
	ev.deltaTimestamp = now - threads_common.perfLastTimestamp;
	threads_common.perfLastTimestamp = now;

	_cbuffer_write(&threads_common.perfBuffer, &ev, sizeof(ev) - sizeof(ev.path) + plen + 1);
	hal_spinlockClear(&threads_common.spinlock);
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
		page_map(&threads_common.kmap->pmap, v, p->addr, PGHD_PRESENT | PGHD_WRITE);
	}

	return data;
}


int perf_start(unsigned pid)
{
	void *data;

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
	hal_spinlockSet(&threads_common.spinlock);
	threads_common.perfGather = 1;
	threads_common.perfLastTimestamp = TIMER_CYC2US(_threads_getTimer());
	hal_spinlockClear(&threads_common.spinlock);

	return EOK;
}


int perf_read(void *buffer, size_t bufsz)
{
	hal_spinlockSet(&threads_common.spinlock);
	bufsz = _cbuffer_read(&threads_common.perfBuffer, buffer, bufsz);
	hal_spinlockClear(&threads_common.spinlock);

	return bufsz;
}


int perf_finish()
{
	hal_spinlockSet(&threads_common.spinlock);
	if (threads_common.perfGather) {
		threads_common.perfGather = 0;
		hal_spinlockClear(&threads_common.spinlock);

		perf_bufferFree(threads_common.perfBuffer.data, &threads_common.perfPages);
	}
	else {
		hal_spinlockClear(&threads_common.spinlock);
	}

	return EOK;
}


/*
 * Time management
 */


static void _threads_updateWakeup(time_t now, thread_t *min)
{
#ifdef HPTIMER_IRQ
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
		wakeup = TIMER_US2CYC(SYSTICK_INTERVAL);
	}

	if (wakeup > TIMER_US2CYC(SYSTICK_INTERVAL + SYSTICK_INTERVAL / 8))
		wakeup = TIMER_US2CYC(SYSTICK_INTERVAL);

	hal_setWakeup(wakeup);
#endif
}


static inline time_t _threads_getTimer(void)
{
#ifdef HPTIMER_IRQ
	return hal_getTimer();
#else
	return threads_common.jiffies;
#endif
}


int threads_timeintr(unsigned int n, cpu_context_t *context, void *arg)
{
	thread_t *t;
	unsigned int i = 0;
	time_t now;

	hal_spinlockSet(&threads_common.spinlock);

#ifdef HPTIMER_IRQ
	now = threads_common.jiffies = hal_getTimer();
#else
	now = threads_common.jiffies += TIMER_US2CYC(SYSTICK_INTERVAL);
#endif

	for (;; i++) {
		t = lib_treeof(thread_t, sleeplinkage, lib_rbMinimum(threads_common.sleeping.root));

		if (t == NULL || t->wakeup > now)
			break;

		_perf_waking(t);

		lib_rbRemove(&threads_common.sleeping, &t->sleeplinkage);
		t->wakeup = 0;
		hal_cpuSetReturnValue(t->context, -ETIME);

		t->state = READY;

		if (t->wait != NULL) {
			LIST_REMOVE(t->wait, t);
			t->wait = NULL;
		}

		/* MOD - test presence for all cores */
		if (t != threads_common.current[hal_cpuGetID()])
			LIST_ADD(&threads_common.ready[t->priority], t);

	}

	_threads_updateWakeup(now, t);

	hal_spinlockClear(&threads_common.spinlock);

	return EOK;
}


/*
 * Threads management
 */


static void thread_destroy(thread_t *t)
{
	hal_spinlockDestroy(&t->execwaitsl);
	vm_kfree(t->kstack);
	vm_kfree(t);
}


thread_t *threads_findThread(int tid)
{
	thread_t *r, t;
	t.id = tid;

	proc_lockSet(&threads_common.lock);
	if ((r = lib_treeof(thread_t, idlinkage, lib_rbFind(&threads_common.id, &t.idlinkage))) != NULL)
		r->refs++;
	proc_lockClear(&threads_common.lock);

	return r;
}


void threads_put(thread_t *t)
{
	int remaining;

	proc_lockSet(&threads_common.lock);
	if (!(remaining = --t->refs))
		lib_rbRemove(&threads_common.id, &t->idlinkage);
	proc_lockClear(&threads_common.lock);

	if (!remaining)
		thread_destroy(t);
}






/*
 * Scheduler
 */

#ifndef CPU_STM32
static void threads_findCurrBucket(cpu_load_t *load, time_t jiffies)
{
	int steps, i;
	const time_t step = TIMER_US2CYC(1000);

	steps = (jiffies - load->jiffiesptr) / step;
	if (steps)
		load->jiffiesptr = jiffies;
	if (steps >= (sizeof(load->cycl) / sizeof(load->cycl[0])))
		hal_memset(load->cycl, 0, sizeof(load->cycl));
	steps %= sizeof(load->cycl) / sizeof(load->cycl[0]);
	for (i = 0; i < steps; ++i) {
		load->cyclptr = (load->cyclptr + 1) % (sizeof(load->cycl) / sizeof(load->cycl[0]));
		load->cycl[load->cyclptr] = 0;
	}
}


int threads_getCpuTime(thread_t *t)
{
	int i;
	u64 curr = 0, tot = 0;

	hal_spinlockSet(&threads_common.spinlock);
	threads_findCurrBucket(&threads_common.load, threads_common.jiffies);
	threads_findCurrBucket(&t->load, threads_common.jiffies);

	if (t != NULL) {
		for (i = 0; i < sizeof(t->load.cycl) / sizeof(t->load.cycl[0]); ++i) {
			curr += t->load.cycl[i];
			tot += threads_common.load.cycl[i];
		}

		if (tot != 0)
			curr = (curr * 1000) / tot;
		else
			curr = 0;
	}
	hal_spinlockClear(&threads_common.spinlock);

	return (int)curr;
}


static void threads_cpuTimeCalc(thread_t *current, thread_t *selected)
{
	cycles_t now = 0;
	time_t jiffies;

	jiffies = threads_common.jiffies;

#ifdef HPTIMER_IRQ
	now = hal_getTimer();
#else
	hal_cpuGetCycles(&now);
#endif

#ifdef NOMMU
	/* Add jiffies because hal_cpuGetCycles returns time only in range from 0 to 1000 us */
	/* Applies to the hardware without CPU cycle counter */
	now += threads_common.jiffies;
#endif

	threads_findCurrBucket(&threads_common.load, jiffies);

	threads_common.load.cycl[threads_common.load.cyclptr] += now - threads_common.load.cyclPrev;
	threads_common.load.cyclPrev = now;

	if (current != NULL) {
		/* Find current bucket */
		threads_findCurrBucket(&current->load, jiffies);

		if (current->load.cyclPrev != 0) {
			current->load.total += now - current->load.cyclPrev;
			current->load.cycl[current->load.cyclptr] += now - current->load.cyclPrev;
		}

		current->load.cyclPrev = now;
	}

	if (current != NULL && selected != NULL && current != selected) {
		current->load.cyclPrev = 0;

		/* Find current bucket */
		threads_findCurrBucket(&selected->load, jiffies);

		selected->load.cyclPrev = now;
	}
}
#else
int threads_getCpuTime(thread_t *t)
{
	return 0;
}
#endif


int threads_schedule(unsigned int n, cpu_context_t *context, void *arg)
{
	thread_t *current, *selected;
	unsigned int i;
	process_t *proc;

	threads_common.executions++;

	hal_spinlockSet(&threads_common.spinlock);
	current = threads_common.current[hal_cpuGetID()];

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
	for (i = 0; i < sizeof(threads_common.ready) / sizeof(thread_t *); i++) {
		if ((selected = threads_common.ready[i]) != NULL)
			break;
	}

	if (selected != NULL) {
		LIST_REMOVE(&threads_common.ready[selected->priority], selected);
		threads_common.current[hal_cpuGetID()] = selected;

		if (((proc = selected->process) != NULL) && (proc->mapp != NULL)) {
			/* Switch address space */
			pmap_switch(&proc->mapp->pmap);
			_hal_cpuSetKernelStack(selected->kstack + selected->kstacksz);
		}

		_perf_scheduling(selected);
		hal_cpuRestore(context, selected->context);
	}

#ifndef CPU_STM32
	/* Update CPU usage */
	threads_cpuTimeCalc(current, selected);
#endif

	/* Test stack usage */
	if (selected != NULL && !selected->execfl && ((void *)selected->context < selected->kstack + selected->kstacksz - 9 * selected->kstacksz / 10)) {
		lib_printf("proc: Stack limit exceeded, sp=%p\n", selected->context);
		for (;;);
	}

	hal_spinlockClear(&threads_common.spinlock);

	return EOK;
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

	hal_spinlockSet(&threads_common.spinlock);
	current = threads_common.current[hal_cpuGetID()];
	hal_spinlockClear(&threads_common.spinlock);

	return current;
}


int proc_threadCreate(process_t *process, void (*start)(void *), unsigned int *id, unsigned int priority, size_t kstacksz, void *stack, size_t stacksz, void *arg)
{
	/* TODO - save user stack and it's size in thread_t */
	thread_t *t;

	if (priority >= sizeof(threads_common.ready) / sizeof(thread_t *))
		return -EINVAL;

	if ((t = vm_kmalloc(sizeof(thread_t))) == NULL)
		return -ENOMEM;

	t->kstacksz = kstacksz;
	if ((t->kstack = vm_kmalloc(t->kstacksz)) == NULL) {
		vm_kfree(t);
		return -ENOMEM;
	}

	hal_spinlockCreate(&t->execwaitsl, "thread.execwaitsl");

	hal_memset(t->kstack, 0xba, t->kstacksz);

	t->state = READY;
	t->wakeup = 0;
	t->process = process;
	t->parentkstack = NULL;
	t->sigmask = t->sigpend = 0;
	t->refs = 1;

	t->id = (unsigned long)t;

	if (id != NULL)
		*id = t->id;

	t->stick = 0;
	t->utick = 0;
	t->priority = priority;
	// t->flags = thread_protected;

	if (process != NULL) {
		hal_spinlockSet(&threads_common.spinlock);
		LIST_ADD_EX(&process->threads, t, procnext, procprev);
		hal_spinlockClear(&threads_common.spinlock);
	}

	t->execfl = OWNSTACK;
	t->execparent = NULL;

	/* Insert thread to global quee */
	proc_lockSet(&threads_common.lock);
	lib_rbInsert(&threads_common.id, &t->idlinkage);

	/* Prepare initial stack */
	hal_cpuCreateContext(&t->context, start, t->kstack, t->kstacksz, stack + stacksz, arg);

#ifndef CPU_STM32
	hal_memset(&t->load.cycl, 0, sizeof(t->load.cycl));
	t->load.cyclPrev = 0;
	t->load.cyclptr = 0;
	t->load.jiffiesptr = 0;
	t->load.total = 0;
#endif

	if (process != NULL) {
		hal_cpuSetCtxGot(t->context, process->got);
	}


	/* Insert thread to scheduler queue */
	hal_spinlockSet(&threads_common.spinlock);
	_perf_begin(t);

	t->maxWait = 0;
	_perf_waking(t);

	LIST_ADD(&threads_common.ready[priority], t);
	// if (current != NULL && current->flags & thread_killme)
		// t->flags |= thread_killme;
	hal_spinlockClear(&threads_common.spinlock);

	proc_lockClear(&threads_common.lock);

	return EOK;
}


static void _proc_threadDequeue(thread_t **queue, thread_t *t)
{
	_perf_waking(t);

	LIST_REMOVE(queue, t);

	if (t->wakeup != 0)
		lib_rbRemove(&threads_common.sleeping, &t->sleeplinkage);

	t->wakeup = 0;
	t->wait = NULL;
	t->state = READY;

	/* MOD */
	if (t != threads_common.current[hal_cpuGetID()])
		LIST_ADD(&threads_common.ready[t->priority], t);
}


static void _thread_interrupt(thread_t *t)
{
	_proc_threadDequeue(t->wait, t);
	hal_cpuSetReturnValue(t->context, -EINTR);
}


void proc_threadDetach(thread_t *t)
{
	hal_spinlockSet(&threads_common.spinlock);
	LIST_REMOVE_EX(&t->process->threads, t, procnext, procprev);
	t->process = NULL;
	hal_spinlockClear(&threads_common.spinlock);
}


// void proc_threadEnd(void)
 void proc_threadDestroy(void)
{
	int cpu;
	thread_t *t;

	hal_spinlockSet(&threads_common.spinlock);
	cpu = hal_cpuGetID();
	t = threads_common.current[cpu];
	threads_common.current[cpu] = NULL;
	LIST_ADD(&threads_common.ghosts, t);
	_perf_end(t);
	hal_cpuReschedule(&threads_common.spinlock);
}


void proc_threadsDestroy(process_t *proc)
{
	thread_t *t;

	hal_spinlockSet(&threads_common.spinlock);
	if ((t = proc->threads) != NULL) {
		do {
			if (t->interruptible)
				_thread_interrupt(t);
		}
		while ((t = t->procnext) != proc->threads);
	}
	hal_spinlockClear(&threads_common.spinlock);
}


int _proc_threadSetPriority(thread_t *t, unsigned int priority)
{
	if (priority > sizeof(threads_common.ready) / sizeof(thread_t *))
		return -EINVAL;

	hal_spinlockSet(&threads_common.spinlock);
	do {
		if (t->priority == priority)
			break;

		/* If thread exist in ready queue */
		if (t->next != NULL) {

			LIST_REMOVE(&threads_common.ready[t->priority], t);
			LIST_ADD(&threads_common.ready[priority], t);

		}
		t->priority = priority;
		t = t->blocking;
	}
	while ((t != NULL) && (t->priority <= priority));

	hal_spinlockClear(&threads_common.spinlock);
	return EOK;
}


/*
 * Sleeping and waiting
 */


int proc_threadSleep(unsigned int us)
{
	thread_t *current;
	int err;
	time_t now;

	proc_threadUnprotect();
	hal_spinlockSet(&threads_common.spinlock);

	now = _threads_getTimer();

	current = threads_common.current[hal_cpuGetID()];
	current->state = SLEEP;
	current->wait = NULL;
	current->wakeup = now + TIMER_US2CYC(us);

	lib_rbInsert(&threads_common.sleeping, &current->sleeplinkage);

	_perf_enqueued(current);

	_threads_updateWakeup(now, NULL);

	if ((err = hal_cpuReschedule(&threads_common.spinlock)) == -ETIME)
		err = EOK;

	proc_threadProtect();

	return err;
}


static void _proc_threadEnqueue(thread_t **queue, time_t timeout)
{
	thread_t *current;
	time_t now;

	if (*queue == (void *)(-1)) {
		(*queue) = NULL;
		return;
	}

	current = threads_common.current[hal_cpuGetID()];

	LIST_ADD(queue, current);

	current->state = SLEEP;
	current->wakeup = 0;
	current->wait = queue;

	if (timeout) {
		now = _threads_getTimer();
		current->wakeup = now + TIMER_US2CYC(timeout);
		lib_rbInsert(&threads_common.sleeping, &current->sleeplinkage);
		_threads_updateWakeup(now, NULL);
	}

	_perf_enqueued(current);
}


static int _proc_threadWait(thread_t **queue, time_t timeout)
{
	int err;

	_proc_threadEnqueue(queue, timeout);

	if (*queue == NULL)
		return EOK;

	err = hal_cpuReschedule(&threads_common.spinlock);
	hal_spinlockSet(&threads_common.spinlock);

	return err;
}


int proc_threadWait(thread_t **queue, spinlock_t *spinlock, time_t timeout)
{
	int err;

	hal_spinlockSet(&threads_common.spinlock);
	_proc_threadEnqueue(queue, timeout);

	if (*queue == NULL) {
		hal_spinlockClear(&threads_common.spinlock);
		return EOK;
	}

	hal_spinlockClear(&threads_common.spinlock);
	err = hal_cpuReschedule(spinlock);
	hal_spinlockSet(spinlock);

	return err;
}


static void _proc_threadWakeup(thread_t **queue)
{
	_proc_threadDequeue(queue, *queue);
}


int proc_threadWakeup(thread_t **queue)
{
	int ret = 0;

	hal_spinlockSet(&threads_common.spinlock);
	if (*queue != NULL && *queue != (void *)(-1)) {
		_proc_threadWakeup(queue);
		ret = 1;
	}
	else {
		(*queue) = (void *)(-1);
	}
	hal_spinlockClear(&threads_common.spinlock);
	return ret;
}


int proc_threadBroadcast(thread_t **queue)
{
	int ret = 0;

	hal_spinlockSet(&threads_common.spinlock);
	if (*queue != (void *)-1) {
		while (*queue != NULL) {
			_proc_threadWakeup(queue);
			ret++;
		}
	}
	hal_spinlockClear(&threads_common.spinlock);
	return ret;
}


void proc_threadWakeupYield(thread_t **queue)
{
	hal_spinlockSet(&threads_common.spinlock);
	if (*queue != NULL && *queue != (void *)(-1)) {
		_proc_threadWakeup(queue);
		hal_cpuReschedule(&threads_common.spinlock);
	}
	else {
		(*queue) = (void *)(-1);
		hal_spinlockClear(&threads_common.spinlock);
	}
}


void proc_threadBroadcastYield(thread_t **queue)
{
	hal_spinlockSet(&threads_common.spinlock);
	if (*queue != (void *)-1 && *queue != NULL) {
		while (*queue != NULL)
			_proc_threadWakeup(queue);

		hal_cpuReschedule(&threads_common.spinlock);
	}
	else {
		*queue = (void *)(-1);
		hal_spinlockClear(&threads_common.spinlock);
	}
}


int proc_waitpid(int pid, int *stat, int options)
{
	int err = 0;
	return err;
}


int proc_waittid(int tid, int options)
{
	int err = 0;
	return err;
}


time_t proc_uptime(void)
{
	time_t time;

	hal_spinlockSet(&threads_common.spinlock);
	time = _threads_getTimer();
	hal_spinlockClear(&threads_common.spinlock);

	return TIMER_CYC2US(time);
}


void proc_gettime(time_t *raw, time_t *offs)
{
	hal_spinlockSet(&threads_common.spinlock);
	(*raw) = TIMER_CYC2US(_threads_getTimer());
	(*offs) = threads_common.utcoffs;
	hal_spinlockClear(&threads_common.spinlock);
}


int proc_settime(time_t offs)
{
	hal_spinlockSet(&threads_common.spinlock);
	threads_common.utcoffs = offs;
	hal_spinlockClear(&threads_common.spinlock);

	return EOK;
}


time_t proc_nextWakeup(void)
{
	thread_t *thread;
	time_t wakeup = 0;
	time_t now;

	hal_spinlockSet(&threads_common.spinlock);
	thread = lib_treeof(thread_t, sleeplinkage, lib_rbMinimum(threads_common.sleeping.root));
	if (thread != NULL) {
		now = _threads_getTimer();
		if (now >= thread->wakeup)
			wakeup = 0;
		else
			wakeup = thread->wakeup - now;
	}
	hal_spinlockClear(&threads_common.spinlock);

	return wakeup;
}


/*
 * Signals
 */


int _proc_sigwant(thread_t *thr)
{
	process_t *proc;

	if (thr == NULL && (thr = _proc_current()) == NULL)
		return 0;

	if ((proc = thr->process) == NULL)
		return 0;

	if (!(proc->sigpend & ~thr->sigmask) && !thr->sigpend)
		return 0;

	if (proc->sighandler == NULL)
		return 0;

	// if (thr->flags & thread_protected)
		// return 0;

	if ((void *)(&proc) - thr->kstack < sizeof(cpu_context_t) * 2 + 128)
		return 0;

	return 1;
}


int proc_sigpost(process_t *process, thread_t *thread, int sig)
{
	int sigbit = 1 << sig;
	thread_t *execthr = NULL;

	if (sig == signal_kill) {
		process->exit = sig << 8;
		proc_kill(process);
		return EOK;
	}

	hal_spinlockSet(&threads_common.spinlock);

	if (process->sighandler == NULL || sigbit & process->sigmask || (thread != NULL && sigbit & thread->sigmask)) {
		hal_spinlockClear(&threads_common.spinlock);

		switch (sig) {
		case signal_segv:
		case signal_illegal:
			process->exit = sig << 8;
			proc_kill(process);
			break;
		default:
			break;
		}

		return EOK;
	}

	if (thread != NULL) {
		thread->sigpend |= sigbit;
		execthr = thread;
	}
	else {
		process->sigpend |= sigbit;
		process->sigmask |= sigbit;

		/* find a thread to wake up to handle the signal */
		thread = process->threads;
		do {
			if (_proc_sigwant(thread)) {
				execthr = thread;
				break;
			}
		} while ((thread = thread->procnext) != process->threads);

		if (execthr == NULL) {
			hal_spinlockClear(&threads_common.spinlock);
			return EOK;
		}
	}

	if (execthr->state == READY) {
		hal_spinlockClear(&threads_common.spinlock);
		return EOK;
	}

	if (execthr->state == SLEEP) {
		execthr->state = READY;

		if (execthr->wakeup > 0) {
			lib_rbRemove(&threads_common.sleeping, &execthr->sleeplinkage);
			execthr->wakeup = 0;
		}

		if (execthr->wait != NULL) {
			LIST_REMOVE(execthr->wait, execthr);
			execthr->wait = NULL;
		}

		LIST_ADD(&threads_common.ready[execthr->priority], execthr);
		hal_cpuSetReturnValue(execthr->context, -EINTR);
	}

	hal_spinlockClear(&threads_common.spinlock);
	return EOK;
}


void proc_sighandle(void *kstack)
{
	thread_t *thr;
	cpu_context_t signal, *top, *dummy;
	long s;

	hal_spinlockSet(&threads_common.spinlock);
	thr = _proc_current();

	if (thr->sigpend) {
		s = hal_cpuGetLastBit(thr->sigpend);
		thr->sigpend &= ~(1 << s);
	}
	else {
		s = hal_cpuGetLastBit(thr->process->sigpend & ~thr->sigmask);
		thr->process->sigpend &= ~(1 << s);
	}

	top = thr->kstack + thr->kstacksz - sizeof(cpu_context_t);
	PUTONSTACK(kstack, size_t, thr->kstacksz);
	thr->kstacksz = kstack - thr->kstack;
	hal_cpuCreateContext(&dummy, thr->process->sighandler, &signal, sizeof(signal), hal_cpuGetUserSP(top), (void *)s);
	hal_cpuGuard(&signal, thr->kstack);
	_hal_cpuSetKernelStack(kstack);
	hal_spinlockClear(&threads_common.spinlock);
	hal_longjmp(&signal);
}


void proc_sigreturn(int s)
{
	thread_t *thr = proc_current();
	void *kstack;
	cpu_context_t *prev;

	hal_spinlockSet(&threads_common.spinlock);
	thr->process->sigmask &= ~(1 << s);
	kstack = thr->kstack + thr->kstacksz;
	thr->kstacksz = *(size_t *)kstack;
	prev = (cpu_context_t *)(kstack + sizeof(size_t));
	_hal_cpuSetKernelStack(thr->kstack + thr->kstacksz);
	hal_spinlockClear(&threads_common.spinlock);
	hal_longjmp(prev);
}


/*
 * Locks
 */


int _proc_lockSet(lock_t *lock)
{
	while (lock->v == 0) {
		if (proc_threadWait(&lock->queue, &lock->spinlock, 0) == -EINTR)
			return -EINTR;
	}

	lock->v = 0;
	return EOK;
}


int proc_lockSet(lock_t *lock)
{
	int err;

	if (!hal_started())
		return -EINVAL;

	hal_spinlockSet(&lock->spinlock);
	err = _proc_lockSet(lock);
	hal_spinlockClear(&lock->spinlock);
	return err;
}


int proc_lockTry(lock_t *lock)
{
	int err = EOK;

	if (!hal_started())
		return -EINVAL;

	hal_spinlockSet(&lock->spinlock);
	if (lock->v == 0)
		err = -EBUSY;

	lock->v = 0;
	hal_spinlockClear(&lock->spinlock);

	return err;
}


int _proc_lockClear(lock_t *lock)
{
	lock->v = 1;
	if (lock->queue == NULL || lock->queue == (void *)-1)
		return 0;

	proc_threadWakeup(&lock->queue);
	return 1;
}


int proc_lockClear(lock_t *lock)
{
	if (!hal_started())
		return -EINVAL;

	hal_spinlockSet(&lock->spinlock);
	if (_proc_lockClear(lock))
		hal_cpuReschedule(&lock->spinlock);
	else
		hal_spinlockClear(&lock->spinlock);

	return EOK;
}


int proc_lockSet2(lock_t *l1, lock_t *l2)
{
	int err;

	if ((err = proc_lockSet(l1)) < 0)
		return err;

	while (proc_lockTry(l2) < 0) {
		proc_lockClear(l1);
		if ((err = proc_lockSet(l2)) < 0)
			return err;
		swap(l1, l2);
	}
	return EOK;
}


int proc_lockWait(thread_t **queue, lock_t *lock, time_t timeout)
{
	int err;
	hal_spinlockSet(&lock->spinlock);
	_proc_lockClear(lock);
	err = proc_threadWait(queue, &lock->spinlock, timeout);
	_proc_lockSet(lock);
	hal_spinlockClear(&lock->spinlock);
	return err;
}


int proc_lockInit(lock_t *lock)
{
	lock->owner = NULL;
	lock->priority = 0;
	lock->queue = NULL;
	lock->v = 1;
	hal_spinlockCreate(&lock->spinlock, "lock.spinlock");
	return EOK;
}


int proc_lockDone(lock_t *lock)
{
	hal_spinlockDestroy(&lock->spinlock);
	return EOK;
}


/*
 * Initialization
 */


static void threads_idlethr(void *arg)
{
	time_t wakeup;
	thread_t *ghost;

	for (;;) {
		if (threads_common.ghosts != NULL) {
			hal_spinlockSet(&threads_common.spinlock);
			if ((ghost = threads_common.ghosts) != NULL)
				LIST_REMOVE(&threads_common.ghosts, ghost);
			hal_spinlockClear(&threads_common.spinlock);

			if (ghost != NULL)
				threads_put(ghost);
		}

		wakeup = proc_nextWakeup();

		if (wakeup > TIMER_US2CYC(2000)) {
			wakeup = hal_cpuLowPower((wakeup + TIMER_US2CYC(500)) / 1000);
#ifdef CPU_STM32
			hal_spinlockSet(&threads_common.spinlock);
			threads_common.jiffies += wakeup * 1000;
			hal_spinlockClear(&threads_common.spinlock);
#endif
		}
		hal_cpuHalt();
	}
}


void proc_threadsDump(unsigned int priority)
{
	thread_t *t;

	lib_printf("threads: ");
	hal_spinlockSet(&threads_common.spinlock);

	t = threads_common.ready[priority];
	do {
		lib_printf("[%p] ", t);

		if (t == NULL)
			break;

		t = t->next;
	} while (t != threads_common.ready[priority]);
	hal_spinlockClear(&threads_common.spinlock);

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

	proc_lockSet(&threads_common.lock);

	t = lib_treeof(thread_t, idlinkage, lib_rbMinimum(threads_common.id.root));

	while (i < n && t != NULL) {
		if (t->process != NULL) {
			info[i].pid = t->process->id;
			// info[i].ppid = t->process->parent != NULL ? t->process->parent->id : 0;
		}
		else {
			info[i].pid = 0;
			info[i].ppid = 0;
		}

		info[i].tid = t->id;
#ifndef CPU_STM32
		info[i].load = threads_getCpuTime(t);
		info[i].cpu_time = (unsigned int) (TIMER_CYC2US(t->load.total) / 1000000);
#else
		info[i].load = 0;
		info[i].cpu_time = 0;
#endif
		info[i].priority = t->priority;
		info[i].state = t->state;

		hal_spinlockSet(&threads_common.spinlock);
		now = TIMER_CYC2US(_threads_getTimer());

		if (t->state == READY && t->maxWait < now - t->readyTime)
			info[i].wait = now - t->readyTime;
		else
			info[i].wait = t->maxWait;
		hal_spinlockClear(&threads_common.spinlock);

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
		t = lib_treeof(thread_t, idlinkage, lib_rbNext(&t->idlinkage));
	}

	proc_lockClear(&threads_common.lock);

	return i;
}


int _threads_init(vm_map_t *kmap, vm_object_t *kernel)
{
	unsigned int i;
	threads_common.kmap = kmap;
	threads_common.executions = 0;
	threads_common.jiffies = 0;
	threads_common.ghosts = NULL;
	threads_common.utcoffs = 0;

	threads_common.perfGather = 0;

	proc_lockInit(&threads_common.lock);

#ifndef CPU_STM32
	hal_memset(&threads_common.load, 0, sizeof(threads_common.load));
#endif

	/* Initiaizlie scheduler queue */
	for (i = 0; i < sizeof(threads_common.ready) / sizeof(thread_t *); i++)
		threads_common.ready[i] = NULL;

	lib_rbInit(&threads_common.sleeping, threads_sleepcmp, NULL);
	lib_rbInit(&threads_common.id, threads_idcmp, NULL);

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
	threads_common.timeintrHandler.f = threads_timeintr;

	hal_memset(&threads_common.scheduleHandler, NULL, sizeof(threads_common.scheduleHandler));
	threads_common.scheduleHandler.f = threads_schedule;

#ifdef HPTIMER_IRQ
	threads_common.timeintrHandler.n = HPTIMER_IRQ;
	threads_common.scheduleHandler.n = HPTIMER_IRQ;
#else
	threads_common.timeintrHandler.n = SYSTICK_IRQ;
	threads_common.scheduleHandler.n = SYSTICK_IRQ;
#endif

	hal_interruptsSetHandler(&threads_common.timeintrHandler);
	hal_interruptsSetHandler(&threads_common.scheduleHandler);

	return EOK;
}
