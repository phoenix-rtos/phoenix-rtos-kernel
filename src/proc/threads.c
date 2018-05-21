/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Thread manager
 *
 * Copyright 2012-2015, 2017 Phoenix Systems
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
#include "resource.h"
#include "msg.h"


struct {
	vm_map_t *kmap;
	spinlock_t spinlock;
	lock_t lock;
	thread_t *ready[8];
	thread_t **current;
	volatile time_t jiffies;

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

	thread_t * volatile ghosts;
	process_t * volatile zombies;
} threads_common;


static int threads_sleepcmp(rbnode_t *n1, rbnode_t *n2)
{
	thread_t *t1 = lib_treeof(thread_t, sleeplinkage, n1);
	thread_t *t2 = lib_treeof(thread_t, sleeplinkage, n2);

	time_t t = t1->wakeup - t2->wakeup;

	if (t == 0)
		return (int)(t1->id - t2->id);

	return t;
}


static int threads_idcmp(rbnode_t *n1, rbnode_t *n2)
{
	thread_t *t1 = lib_treeof(thread_t, idlinkage, n1);
	thread_t *t2 = lib_treeof(thread_t, idlinkage, n2);

	return (t1->id - t2->id);
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
		wakeup = TIMER_US2CYC(1000);
	}

	if (wakeup > TIMER_US2CYC(1200))
		wakeup = TIMER_US2CYC(1000);

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
	now = threads_common.jiffies += TIMER_US2CYC(1000);
#endif

	for (;; i++) {
		t = lib_treeof(thread_t, sleeplinkage, lib_rbMinimum(threads_common.sleeping.root));

		if (t == NULL || t->wakeup > now)
			break;

		lib_rbRemove(&threads_common.sleeping, &t->sleeplinkage);
		t->state = READY;
		t->wakeup = 0;

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


thread_t *threads_getFirstThread(void)
{
	thread_t *t;

	proc_lockSet(&threads_common.lock);
	t = lib_treeof(thread_t, idlinkage, lib_rbMinimum(threads_common.id.root));
	proc_lockClear(&threads_common.lock);

	return t;
}


thread_t *threads_getNextThread(thread_t *prev)
{
	thread_t *r;

	proc_lockSet(&threads_common.lock);
	r = lib_treeof(thread_t, idlinkage, lib_rbNext(&prev->idlinkage));
	proc_lockClear(&threads_common.lock);

	return r;
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

	hal_cpuGetCycles(&now);
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

		if (current->load.cyclPrev != 0)
			current->load.cycl[current->load.cyclptr] += now - current->load.cyclPrev;

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
		if (current->state == READY)
			LIST_ADD(&threads_common.ready[current->priority], current);
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

		hal_cpuRestore(context, selected->context);
	}

#ifndef CPU_STM32
	/* Update CPU usage */
	threads_cpuTimeCalc(current, selected);
#endif

	hal_spinlockClear(&threads_common.spinlock);

	/* Test stack usage */
	if (selected != NULL && !selected->execfl && ((void *)selected->context < selected->kstack + selected->kstacksz - 9 * selected->kstacksz / 10)) {
#ifdef CPU_IA32
		lib_printf("proc: Stack limit exceeded, sp=%p %p pc=%p %p\n", &selected, selected->kstack, selected->id, selected->context->eip);
		for (;;);
#else
		lib_printf("proc: Stack limit exceeded, sp=%p %p pc=%p %p\n", &selected, selected->kstack, selected->id, selected->context->pc);
#endif
	}

	return EOK;
}


thread_t *_proc_current(void)
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
	thread_t *t, *current = proc_current();

	if (priority >= sizeof(threads_common.ready) / sizeof(thread_t *))
		return -EINVAL;

	hal_spinlockSet(&threads_common.spinlock);
	if ((t = threads_common.ghosts) != NULL)
		LIST_REMOVE(&threads_common.ghosts, t);
	hal_spinlockClear(&threads_common.spinlock);

	if (t == NULL) {
		if ((t = (thread_t *)vm_kmalloc(sizeof(thread_t))) == NULL)
			return -ENOMEM;

		t->kstacksz = kstacksz;
		if ((t->kstack = vm_kmalloc(t->kstacksz)) == NULL) {
			vm_kfree(t);
			return -ENOMEM;
		}

		hal_spinlockCreate(&t->execwaitsl, "thread.execwaitsl");
	}
	else if (t->kstacksz != kstacksz) {
		vm_kfree(t->kstack);

		t->kstacksz = kstacksz;
		if ((t->kstack = vm_kmalloc(t->kstacksz)) == NULL) {
			vm_kfree(t);
			return -ENOMEM;
		}
	}

	hal_memset(t->kstack, 0xba, t->kstacksz);

	t->state = READY;
	t->wakeup = 0;
	t->process = process;
	t->parentkstack = NULL;
	t->sigmask = t->sigpend = 0;

	t->id = (unsigned long)t;

	if (id != NULL)
		*id = t->id;

	t->stick = 0;
	t->utick = 0;
	t->priority = priority;
	t->flags = thread_protected;

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
#endif

	if (process != NULL) {
		hal_cpuSetGot(t->context, process->got);
	}

	/* Insert thread to scheduler queue */
	hal_spinlockSet(&threads_common.spinlock);
	LIST_ADD(&threads_common.ready[priority], t);
	if (current != NULL && current->flags & thread_killme)
		t->flags |= thread_killme;
	hal_spinlockClear(&threads_common.spinlock);

	proc_lockClear(&threads_common.lock);

	return EOK;
}


void proc_threadProtect(void)
{
	thread_t *t;

	hal_spinlockSet(&threads_common.spinlock);
	t = threads_common.current[hal_cpuGetID()];
	if (t != NULL)
		t->flags |= thread_protected;
	hal_spinlockClear(&threads_common.spinlock);
}


void proc_threadUnprotect(void)
{
	thread_t *t;

	hal_spinlockSet(&threads_common.spinlock);
	t = threads_common.current[hal_cpuGetID()];
	if (t != NULL) {
		t->flags &= ~thread_protected;
		if (t->flags & thread_killme) {
			hal_spinlockClear(&threads_common.spinlock);
			proc_threadDestroy();
		}
	}
	hal_spinlockClear(&threads_common.spinlock);
}


void proc_threadDestroy(void)
{
	thread_t *thr = proc_current();

	hal_spinlockSet(&threads_common.spinlock);
	if (thr->process != NULL)
		LIST_REMOVE_EX(&thr->process->threads, thr, procnext, procprev);
	threads_common.current[hal_cpuGetID()] = NULL;
	LIST_ADD(&threads_common.ghosts, thr);
	hal_cpuReschedule(&threads_common.spinlock);
}


void proc_threadsDestroy(process_t *proc)
{
	thread_t *t = NULL, *n, *l;

	hal_spinlockSet(&threads_common.spinlock);
	n = proc->threads;
	l = n->procprev;

	while (t != l && (t = n) != NULL) {
		n = t->procnext;

		if (t == _proc_current())
			continue;

		if (t->flags & thread_protected) {
			t->flags |= thread_killme;
			continue;
		}

		if (t->state == SLEEP) {
			if (t->wakeup)
				lib_rbRemove(&threads_common.sleeping, &t->sleeplinkage);

			if (t->wait != NULL && *t->wait != (void *)-1)
				LIST_REMOVE(t->wait, t);
		}
		else if (t->state == READY) {
			LIST_REMOVE(&threads_common.ready[t->priority], t);
		}

		LIST_ADD(&threads_common.ghosts, t);
		LIST_REMOVE_EX(&proc->threads, t, procnext, procprev);
	}
	hal_spinlockClear(&threads_common.spinlock);
}


void proc_zombie(process_t *proc)
{
	proc_portsDestroy(proc);

	hal_spinlockSet(&threads_common.spinlock);
	LIST_ADD(&threads_common.zombies, proc);
	hal_spinlockClear(&threads_common.spinlock);

	if (proc->parent != NULL) {
		hal_spinlockSet(&proc->parent->waitsl);
		proc_threadWakeup(&proc->parent->waitq);
		hal_spinlockClear(&proc->parent->waitsl);
	}
}


static void proc_cleanupZombie(process_t *proc)
{
#ifndef NOMMU
	int i = 0;
	addr_t a;
#endif

	proc_resourcesFree(proc);

	if (proc->mapp != NULL)
		vm_mapDestroy(proc, proc->mapp);

#ifndef NOMMU
	if (proc->mapp != NULL) {
		while ((a = pmap_destroy(&proc->map.pmap, &i)))
			vm_pageFree(_page_get(a));

		vm_munmap(threads_common.kmap, proc->pmapv, SIZE_PAGE);
		vm_pageFree(proc->pmapp);
	}
#endif

	hal_spinlockDestroy(&proc->waitsl);
	proc_lockDone(&proc->lock);

	if (proc->path != NULL)
		vm_kfree((void *)proc->path);

	vm_kfree(proc);
}

static void proc_cleanupGhost(thread_t *thr)
{
	proc_lockSet(&threads_common.lock);
	lib_rbRemove(&threads_common.id, &thr->idlinkage);
	proc_lockClear(&threads_common.lock);

	hal_spinlockDestroy(&thr->execwaitsl);
	vm_kfree(thr->kstack);
	vm_kfree(thr);
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

	_threads_updateWakeup(now, NULL);
	err = hal_cpuReschedule(&threads_common.spinlock);
	proc_threadProtect();

	return err;
}


int proc_threadWait(thread_t **queue, spinlock_t *spinlock, time_t timeout)
{
	int err = EOK;
	thread_t *current;
	time_t now;

	hal_spinlockSet(&threads_common.spinlock);

	if (*queue == (void *)(-1)) {
		(*queue) = NULL;
		hal_spinlockClear(&threads_common.spinlock);
		return err;
	}

	current = threads_common.current[hal_cpuGetID()];

	LIST_ADD(queue, current);

	current->state = SLEEP;
	current->wakeup = 0;
	current->wait = queue;

	if (timeout) {
		now = _threads_getTimer();
		current->wakeup = now + timeout;
		lib_rbInsert(&threads_common.sleeping, &current->sleeplinkage);
		_threads_updateWakeup(now, NULL);
	}

	hal_spinlockClear(&threads_common.spinlock);
	err = hal_cpuReschedule(spinlock);
	hal_spinlockSet(spinlock);

	hal_spinlockSet(&threads_common.spinlock);
	if (current->wakeup)
		err = -ETIME;
	hal_spinlockClear(&threads_common.spinlock);

	return err;
}


void proc_threadWakeup(thread_t **queue)
{
	thread_t *first;

	hal_spinlockSet(&threads_common.spinlock);
	if (*queue != NULL && *queue != (void *)(-1)) {
		first = *queue;
		LIST_REMOVE(queue, first);
		if (first->wakeup != 0)
			lib_rbRemove(&threads_common.sleeping, &first->sleeplinkage);

		first->wakeup = 0;
		first->wait = NULL;
		first->state = READY;

		/* MOD */
		if (first != threads_common.current[hal_cpuGetID()])
			LIST_ADD(&threads_common.ready[first->priority], first);
	}
	else {
		(*queue) = (void *)(-1);
	}
	hal_spinlockClear(&threads_common.spinlock);

	return;
}


int proc_waitpid(int pid, int *stat, int options)
{
	int err;
	process_t *proc = proc_current()->process;

	/* TODO: WNOHANG */
	if (options & 1) {
		return EOK;
	}

	proc_threadUnprotect();
	hal_spinlockSet(&proc->waitsl);
	proc->waitpid = pid;
	err = proc_threadWait(&proc->waitq, &proc->waitsl, 0);
	proc->waitpid = 0;
	hal_spinlockClear(&proc->waitsl);
	proc_threadProtect();

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


int proc_nextWakeup(void)
{
	thread_t *thread;
	int wakeup = 0;
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

	if (thr->flags & thread_protected)
		return 0;

	if ((void *)(&proc) - thr->kstack < sizeof(cpu_context_t) * 2 + 128)
		return 0;

	return 1;
}


int proc_sigpost(process_t *process, thread_t *thread, int sig)
{
	int sigbit = 1 << sig;
	thread_t *execthr = NULL;

	if (sig == signal_kill) {
		proc_kill(process);
		return EOK;
	}

	hal_spinlockSet(&threads_common.spinlock);

	if (process->sighandler == NULL || sigbit & process->sigmask || (thread != NULL && sigbit & thread->sigmask)) {
		hal_spinlockClear(&threads_common.spinlock);

		switch (sig) {
		case signal_segv:
		case signal_illegal:
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
			if (!_proc_sigwant(thread))
				continue;

			execthr = thread;
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


void _proc_lockClear(lock_t *lock)
{
	lock->v = 1;
	proc_threadWakeup(&lock->queue);
	return;
}


int proc_lockClear(lock_t *lock)
{
	if (!hal_started())
		return -EINVAL;

	hal_spinlockSet(&lock->spinlock);
	_proc_lockClear(lock);
	hal_cpuReschedule(&lock->spinlock);

	return EOK;
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
	int wakeup;
	thread_t *ghost;
	process_t *zombie;

	for (;;) {
		/* Don't grab spinlocks unless there is something to clean up */
		if (threads_common.ghosts != NULL) {
			do {
				hal_spinlockSet(&threads_common.spinlock);
				if ((ghost = threads_common.ghosts) != NULL)
					LIST_REMOVE(&threads_common.ghosts, ghost);
				hal_spinlockClear(&threads_common.spinlock);

				if (ghost != NULL)
					proc_cleanupGhost(ghost);
			}
			while (ghost != NULL);
		}

		if (threads_common.zombies != NULL) {
			do {
				hal_spinlockSet(&threads_common.spinlock);
				if ((zombie = threads_common.zombies) != NULL && zombie->threads == NULL && zombie->ports == NULL)
					LIST_REMOVE(&threads_common.zombies, zombie);
				else
					zombie = NULL;
				hal_spinlockClear(&threads_common.spinlock);

				if (zombie != NULL)
					proc_cleanupZombie(zombie);
			}
			while (zombie != NULL);
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
	int i = 0;
	thread_t *t;
	int len;

	proc_lockSet(&threads_common.lock);

	t = lib_treeof(thread_t, idlinkage, lib_rbMinimum(threads_common.id.root));

	while (i < n && t != NULL) {
		if (t->process != NULL)
			info[i].pid = t->process->id;
		else
			info[i].pid = -1;

		info[i].tid = t->id;
		info[i].load = threads_getCpuTime(t);
		info[i].priority = t->priority;
		info[i].state = t->state;

		if (t->process != NULL) {
			if (t->process->path != NULL) {
				len = 1 + hal_strlen(t->process->path);
				hal_memcpy(info[i].name, t->process->path, min(len, sizeof(info[i].name)));
				info[i].name[sizeof(info[i].name) - 1] = 0;
			}
			else
				info[i].name[0] = 0;
		}
		else
			hal_memcpy(info[i].name, "idle thread", sizeof("idle thread"));

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
	threads_common.zombies = NULL;
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
