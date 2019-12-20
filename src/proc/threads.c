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
#include "../../include/wait.h"
#include "../../include/signal.h"
#include "threads.h"
#include "../lib/lib.h"
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
	unsigned int idcounter;
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
	thread_t *reaper;

	int perfGather;
	time_t perfLastTimestamp;
	cbuffer_t perfBuffer;
	page_t *perfPages;
} threads_common;


static thread_t *_proc_current(void);
static void _proc_threadDequeue(thread_t *t);
static int _proc_threadWait(thread_t **queue, time_t timeout);


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


void perf_end(thread_t *t)
{
	perf_levent_end_t ev;
	time_t now;

	if (!threads_common.perfGather)
		return;

	hal_spinlockSet(&threads_common.spinlock);
	ev.sbz = 0;
	ev.type = perf_levEnd;
	ev.tid = perf_idpack(t->id);

	now = TIMER_CYC2US(_threads_getTimer());
	ev.deltaTimestamp = now - threads_common.perfLastTimestamp;
	threads_common.perfLastTimestamp = now;

	_cbuffer_write(&threads_common.perfBuffer, &ev, sizeof(ev));
	hal_spinlockClear(&threads_common.spinlock);
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

		_proc_threadDequeue(t);
		hal_cpuSetReturnValue(t->context, -ETIME);
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
	process_t *process, *parent;
	perf_end(t);

	vm_kfree(t->kstack);

	if ((process = t->process) != NULL) {
		hal_spinlockSet(&threads_common.spinlock);
		LIST_REMOVE_EX(&process->threads, t, procnext, procprev);
		LIST_ADD_EX(&process->ghosts, t, procnext, procprev);
		_proc_threadWakeup(&process->reaper);
		hal_spinlockClear(&threads_common.spinlock);

		if (process->threads == NULL && (parent = proc_find(process->ppid)) != NULL) {
			proc_zombie(process, parent);
			proc_put(parent);
		}

		proc_put(process);
	}
	else {
		vm_kfree(t);
	}
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
	unsigned int i, sig;
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
	for (i = 0; i < sizeof(threads_common.ready) / sizeof(thread_t *);) {
		if ((selected = threads_common.ready[i]) == NULL) {
			i++;
			continue;
		}

		LIST_REMOVE(&threads_common.ready[i], selected);

		if (!selected->exit || hal_cpuSupervisorMode(selected->context))
			break;

		LIST_ADD(&threads_common.ghosts, selected);
		_proc_threadWakeup(&threads_common.reaper);
	}

	if (selected != NULL) {
		threads_common.current[hal_cpuGetID()] = selected;

		if (((proc = selected->process) != NULL) && (proc->mapp != NULL)) {
			/* Switch address space */
			pmap_switch(&proc->mapp->pmap);
			_hal_cpuSetKernelStack(selected->kstack + selected->kstacksz);

			/* Check for signals to handle */
			if ((sig = (selected->sigpend | proc->sigpend) & ~selected->sigmask) && proc->sighandler != NULL) {
				sig = hal_cpuGetLastBit(sig);

				if (hal_cpuPushSignal(selected->kstack + selected->kstacksz, proc->sighandler, sig) == EOK) {
					selected->sigpend &= ~(1 << sig);
					proc->sigpend &= ~(1 << sig);
				}
			}
		}

		_perf_scheduling(selected);
		hal_cpuRestore(context, selected->context);
	}

#ifndef CPU_STM32
	/* Update CPU usage */
	threads_cpuTimeCalc(current, selected);
#endif

#if 0
	/* Test stack usage */
	if (selected != NULL && !selected->execkstack && ((void *)selected->context < selected->kstack + selected->kstacksz - 9 * selected->kstacksz / 10)) {
		lib_printf("proc: Stack limit exceeded, sp=%p\n", selected->context);
		// for (;;);
	}
#endif

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


static unsigned _thread_alloc(unsigned id)
{
	thread_t *p = lib_treeof(thread_t, idlinkage, threads_common.id.root);

	while (p != NULL) {
		if (p->lgap && id < p->id) {
			if (p->idlinkage.left == NULL)
				return max(id, p->id - p->lgap);

			p = lib_treeof(thread_t, idlinkage, p->idlinkage.left);
			continue;
		}

		if (p->rgap) {
			if (p->idlinkage.right == NULL)
				return max(id, p->id + 1);

			p = lib_treeof(thread_t, idlinkage, p->idlinkage.right);
			continue;
		}

		for (;; p = lib_treeof(thread_t, idlinkage, p->idlinkage.parent)) {
			if (p->idlinkage.parent == NULL)
				return NULL;

			if ((p == lib_treeof(thread_t, idlinkage, p->idlinkage.parent->left)) && lib_treeof(thread_t, idlinkage, p->idlinkage.parent)->rgap)
				break;
		}
		p = lib_treeof(thread_t, idlinkage, p->idlinkage.parent);

		if (p->idlinkage.right == NULL)
			return p->id + 1;

		p = lib_treeof(thread_t, idlinkage, p->idlinkage.right);
	}

	return id;
}


static unsigned thread_alloc(thread_t *thread)
{
	thread->id = _thread_alloc(threads_common.idcounter);

	if (!thread->id)
		thread->id = _thread_alloc(threads_common.idcounter = 1);

	if (threads_common.idcounter == MAX_TID)
		threads_common.idcounter = 1;

	if (thread->id) {
		lib_rbInsert(&threads_common.id, &thread->idlinkage);
		threads_common.idcounter++;
	}
	else {
		lib_printf("thread id allocation failed\n");
	}

	return thread->id;
}


static void thread_augment(rbnode_t *node)
{
	rbnode_t *it;
	thread_t *n = lib_treeof(thread_t, idlinkage, node);
	thread_t *p = n, *r, *l;

	if (node->left == NULL) {
		for (it = node; it->parent != NULL; it = it->parent) {
			p = lib_treeof(thread_t, idlinkage, it->parent);
			if (it->parent->right == it)
				break;
		}

		n->lgap = !!((n->id <= p->id) ? n->id : n->id - p->id - 1);
	}
	else {
		l = lib_treeof(thread_t, idlinkage, node->left);
		n->lgap = max((int)l->lgap, (int)l->rgap);
	}

	if (node->right == NULL) {
		for (it = node; it->parent != NULL; it = it->parent) {
			p = lib_treeof(thread_t, idlinkage, it->parent);
			if (it->parent->left == it)
				break;
		}

		n->rgap = !!((n->id >= p->id) ? MAX_TID - n->id - 1 : p->id - n->id - 1);
	}
	else {
		r = lib_treeof(thread_t, idlinkage, node->right);
		n->rgap = max((int)r->lgap, (int)r->rgap);
	}

	for (it = node; it->parent != NULL; it = it->parent) {
		n = lib_treeof(thread_t, idlinkage, it);
		p = lib_treeof(thread_t, idlinkage, it->parent);

		if (it->parent->left == it)
			p->lgap = max((int)n->lgap, (int)n->rgap);
		else
			p->rgap = max((int)n->lgap, (int)n->rgap);
	}
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

	t->stick = 0;
	t->utick = 0;
	t->priority = priority;

	if (process != NULL) {
		hal_spinlockSet(&threads_common.spinlock);
		LIST_ADD_EX(&process->threads, t, procnext, procprev);
		hal_spinlockClear(&threads_common.spinlock);
	}

	t->execdata = NULL;

	/* Insert thread to global quee */
	proc_lockSet(&threads_common.lock);
	thread_alloc(t);

	if (id != NULL)
		*id = t->id;

	/* Prepare initial stack */
	hal_cpuCreateContext(&t->context, start, t->kstack, t->kstacksz, stack + stacksz, arg);

#ifndef CPU_STM32
	hal_memset(&t->load.cycl, 0, sizeof(t->load.cycl));
	t->load.cyclPrev = 0;
	t->load.cyclptr = 0;
	t->load.jiffiesptr = 0;
	t->load.total = 0;
#endif

	if (process != NULL)
		hal_cpuSetCtxGot(t->context, process->got);


	/* Insert thread to scheduler queue */
	hal_spinlockSet(&threads_common.spinlock);
	_perf_begin(t);

	t->maxWait = 0;
	_perf_waking(t);

	LIST_ADD(&threads_common.ready[priority], t);
	hal_spinlockClear(&threads_common.spinlock);

	proc_lockClear(&threads_common.lock);

	return EOK;
}


int proc_threadKill(pid_t pid, int tid, int signal)
{
	process_t *process, *peer;
	thread_t *thread = 0;

	if (signal < 0 || signal > NSIG)
		return -EINVAL;

	if (pid == 0)
		return -ENOSYS;
	if (pid == -1)
		return -ESRCH;

	if (pid > 0) {
		if ((process = proc_find(pid)) == NULL)
			return -ESRCH;

		if (tid && (thread = threads_findThread(tid)) == NULL)
			return -ESRCH;

		threads_sigpost(process, thread, signal);
		if (tid)
			threads_put(thread);
		proc_put(process);
	}
	else {
		if (tid)
			return -EINVAL;

		if ((process = proc_find(-pid)) == NULL)
			return -ESRCH;

		proctree_lock();
		peer = process;
		do {
			threads_sigpost(process, NULL, signal);
		} while ((peer = peer->pg_next) != process);
		proctree_unlock();

		proc_put(process);
	}

	return EOK;
}


static void _thread_interrupt(thread_t *t)
{
	_proc_threadDequeue(t);
	hal_cpuSetReturnValue(t->context, -EINTR);
}


void proc_threadEnd(void)
{
	thread_t *t;
	int cpu;

	hal_spinlockSet(&threads_common.spinlock);
	cpu = hal_cpuGetID();
	t = threads_common.current[cpu];
	threads_common.current[cpu] = NULL;
	LIST_ADD(&threads_common.ghosts, t);
	_proc_threadWakeup(&threads_common.reaper);
	hal_cpuReschedule(&threads_common.spinlock);
}


static void _proc_threadExit(thread_t *t)
{
	t->exit = 1;
	if (t->interruptible)
		_thread_interrupt(t);
}


void proc_threadsDestroy(thread_t **threads)
{
	thread_t *t;

	hal_spinlockSet(&threads_common.spinlock);
	if ((t = *threads) != NULL) {
		do
			_proc_threadExit(t);
		while ((t = t->procnext) != *threads);
	}
	hal_spinlockClear(&threads_common.spinlock);
}


void proc_reap(void)
{
	thread_t *ghost;

	hal_spinlockSet(&threads_common.spinlock);
	while ((ghost = threads_common.ghosts) == NULL)
		_proc_threadWait(&threads_common.reaper, 0);

	LIST_REMOVE(&threads_common.ghosts, ghost);
	hal_spinlockClear(&threads_common.spinlock);

	threads_put(ghost);
}


/*
 * Sleeping and waiting
 */

static void _proc_threadDequeue(thread_t *t)
{
	_perf_waking(t);

	if (t->wait != NULL)
		LIST_REMOVE(t->wait, t);

	if (t->wakeup)
		lib_rbRemove(&threads_common.sleeping, &t->sleeplinkage);

	t->wakeup = 0;
	t->wait = NULL;
	t->state = READY;
	t->interruptible = 0;

	/* MOD */
	if (t != threads_common.current[hal_cpuGetID()])
		LIST_ADD(&threads_common.ready[t->priority], t);
}


static void _proc_threadEnqueue(thread_t **queue, time_t timeout, int interruptible)
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
	current->interruptible = interruptible;

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

	_proc_threadEnqueue(queue, timeout, 0);

	if (*queue == NULL)
		return EOK;

	err = hal_cpuReschedule(&threads_common.spinlock);
	hal_spinlockSet(&threads_common.spinlock);

	return err;
}


int proc_threadSleep(unsigned long long us)
{
	thread_t *current;
	int err;
	time_t now;

	hal_spinlockSet(&threads_common.spinlock);

	now = _threads_getTimer();

	current = threads_common.current[hal_cpuGetID()];
	current->state = SLEEP;
	current->wait = NULL;
	current->wakeup = now + TIMER_US2CYC(us);
	current->interruptible = 1;

	lib_rbInsert(&threads_common.sleeping, &current->sleeplinkage);
	_perf_enqueued(current);
	_threads_updateWakeup(now, NULL);

	if ((err = hal_cpuReschedule(&threads_common.spinlock)) == -ETIME)
		err = EOK;

	return err;
}


static int proc_threadWaitEx(thread_t **queue, spinlock_t *spinlock, time_t timeout, int interruptible)
{
	int err;

	hal_spinlockSet(&threads_common.spinlock);
	_proc_threadEnqueue(queue, timeout, interruptible);

	if (*queue == NULL) {
		hal_spinlockClear(&threads_common.spinlock);
		return EOK;
	}

	hal_spinlockClear(&threads_common.spinlock);
	err = hal_cpuReschedule(spinlock);
	hal_spinlockSet(spinlock);

	return err;
}


int proc_threadWait(thread_t **queue, spinlock_t *spinlock, time_t timeout)
{
	return proc_threadWaitEx(queue, spinlock, timeout, 0);
}


int proc_threadWaitInterruptible(thread_t **queue, spinlock_t *spinlock, time_t timeout)
{
	return proc_threadWaitEx(queue, spinlock, timeout, 1);
}


static void _proc_threadWakeup(thread_t **queue)
{
	if (*queue != NULL && *queue != (void *)-1)
		_proc_threadDequeue(*queue);
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


int proc_join(time_t timeout)
{
	int err;
	thread_t *ghost;
	process_t *process = proc_current()->process;

	hal_spinlockSet(&threads_common.spinlock);
	while ((ghost = process->ghosts) == NULL) {
		err = _proc_threadWait(&process->reaper, timeout);

		if (err == -EINTR || err == -ETIME)
			break;
	}

	if (ghost != NULL) {
		LIST_REMOVE_EX(&process->ghosts, ghost, procnext, procprev);
		err = ghost->id;
	}
	hal_spinlockClear(&threads_common.spinlock);

	vm_kfree(ghost);
	return err;
}


int proc_child(process_t *child, process_t *parent)
{
	child->ppid = parent->id;
	hal_spinlockSet(&threads_common.spinlock);
	LIST_ADD(&parent->children, child);
	hal_spinlockClear(&threads_common.spinlock);
	return EOK;
}


int proc_zombie(process_t *zombie, process_t *parent)
{
	hal_spinlockSet(&threads_common.spinlock);
	LIST_REMOVE(&parent->children, zombie);
	LIST_ADD(&parent->zombies, zombie);
	hal_spinlockClear(&threads_common.spinlock);

	proc_threadBroadcastYield(&parent->wait);
	threads_sigpost(parent, NULL, SIGCHLD);
	return EOK;
}


int proc_waitpid(pid_t pid, int *status, int options)
{
	process_t *p = proc_current()->process, *c;
	int err = EOK;
	int found = 0;

	hal_spinlockSet(&threads_common.spinlock);
	do {
		if ((c = p->zombies) == NULL) {
			if (p->children == NULL) {
				err = -ECHILD;
				break;
			}
			else if (options & WNOHANG) {
				err = EOK;
				break;
			}
			else {
				while ((c = p->zombies) == NULL && !err)
					err = _proc_threadWait(&p->wait, 0);

				if (err == -EINTR) {
					hal_spinlockClear(&threads_common.spinlock);
					return -EINTR;
				}
				else if (err) {
					/* Should not happen */
					break;
				}
			}
		}

		do {
			if (pid == -1 || (c->group != NULL && !pid && c->group->id == p->group->id) ||
			    (pid < 0 && c->group != NULL && c->group->id == -pid) || pid == c->id) {
				LIST_REMOVE(&p->zombies, c);
				found = 1;
			}
		}
		while (!found && (c = c->next) != p->zombies);
	}
	while (!found);
	hal_spinlockClear(&threads_common.spinlock);

	if (found) {
		err = c->id;

		if (status != NULL)
			*status = c->exit;

		proc_put(c);
	}

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


int threads_sigpost(process_t *process, thread_t *thread, int sig)
{
	int sigbit = 1 << sig;
	int kill = 0;

	switch (sig) {
		case SIGSEGV:
		case SIGILL:
			if (process->sighandler != NULL)
				break;

		/* passthrough */
		case SIGKILL:
			proc_kill(process);

		/* passthrough */
		case 0:
			return EOK;

		default:
			break;
	}

	hal_spinlockSet(&threads_common.spinlock);
	if (thread != NULL && hal_cpuPushSignal(thread->kstack + thread->kstacksz, thread->process->sighandler, sig) != EOK) {
		thread->sigpend |= sigbit;

		if (sig == SIGSEGV || sig == SIGILL) {
			/* If they can't handle those right away, kill */
			kill = 1;
			_proc_threadExit(thread);
		}
	}
	else {
		process->sigpend |= sigbit;
		thread = process->threads;

		do {
			if (sigbit & ~thread->sigmask) {
				if (thread->interruptible)
					_thread_interrupt(thread);

				break;
			}
		}
		while ((thread = thread->procnext) != process->threads);
	}
	hal_cpuReschedule(&threads_common.spinlock);

	if (kill)
		proc_kill(process);

	return EOK;
}


/*
 * Locks
 */


int _proc_lockSet(lock_t *lock, int interruptible)
{
	while (lock->v == 0) {
		if (proc_threadWaitEx(&lock->queue, &lock->spinlock, 0, interruptible) == -EINTR)
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
	err = _proc_lockSet(lock, 0);
	hal_spinlockClear(&lock->spinlock);
	return err;
}


int proc_lockSetInterruptible(lock_t *lock)
{
	int err;

	hal_spinlockSet(&lock->spinlock);
	err = _proc_lockSet(lock, 1);
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
	if ((err = proc_threadWaitEx(queue, &lock->spinlock, timeout, 1)) != -EINTR)
		_proc_lockSet(lock, 0);
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

	for (;;) {
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
			info[i].ppid = t->process->ppid;
			info[i].ppid = 0;
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
	threads_common.reaper = NULL;
	threads_common.utcoffs = 0;
	threads_common.idcounter = 0;

	threads_common.perfGather = 0;

	proc_lockInit(&threads_common.lock);

#ifndef CPU_STM32
	hal_memset(&threads_common.load, 0, sizeof(threads_common.load));
#endif

	/* Initiaizlie scheduler queue */
	for (i = 0; i < sizeof(threads_common.ready) / sizeof(thread_t *); i++)
		threads_common.ready[i] = NULL;

	lib_rbInit(&threads_common.sleeping, threads_sleepcmp, NULL);
	lib_rbInit(&threads_common.id, threads_idcmp, thread_augment);

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
