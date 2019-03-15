/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Thread manager
 *
 * Copyright 2012-2013, 2017 Phoenix Systems
 * Copyright 2001, 2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk, Jacek Popko
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PROC_THREADS_H_
#define _PROC_THREADS_H_

#include HAL
#include "../lib/lib.h"
#include "process.h"
#include "lock.h"
#include "../../include/sysinfo.h"


/* Parent thread states */
enum { PREFORK = 0, FORKING = 1, FORKED, NOFORK };

/* Child thread states */
enum { OWNSTACK = 0, PARENTSTACK };


typedef struct {
	cycles_t cycl[10];
	cycles_t cyclPrev;
	int cyclptr;
	time_t jiffiesptr;
	time_t total;
} cpu_load_t;


typedef struct _thread_t {
	struct _thread_t *next;
	struct _thread_t *prev;

	rbnode_t sleeplinkage;
	rbnode_t idlinkage;

	struct _process_t *process;
	struct _thread_t *procnext;
	struct _thread_t *procprev;

	unsigned long id;
	unsigned int priority;
	struct _thread_t *blocking;

	struct _thread_t **wait;
	volatile time_t wakeup;

	volatile enum {
		thread_killme = 1 << 0,
		thread_protected = 1 << 1,
	} flags;

	unsigned sigmask;
	unsigned sigpend;

	time_t stick;
	time_t utick;

	void *kstack;
	size_t kstacksz;

	struct _thread_t *execwaitq;
	void *parentkstack, *execkstack;
	struct _thread_t *execparent;

	spinlock_t execwaitsl;

	volatile enum { READY = 0, SLEEP = 1 } state;
	char execfl;

	time_t readyTime;
	time_t maxWait;

#ifndef CPU_STM32
	cpu_load_t load;
#endif

	cpu_context_t *context;
} thread_t;


extern int perf_start(unsigned pid);


extern int perf_read(void *buffer, size_t bufsz);


extern int perf_finish(void);


extern void perf_fork(process_t *p);


extern void perf_kill(process_t *p);


extern void perf_exec(process_t *p, char *path);


extern thread_t *proc_current(void);


extern int proc_threadCreate(process_t *process, void (*start)(void *), unsigned int *id, unsigned int priority, size_t kstacksz, void *stack, size_t stacksz, void *arg);


extern void proc_threadProtect(void);


extern void proc_threadUnprotect(void);


extern void proc_threadDestroy(void);


extern int proc_threadJoin(unsigned int id);


extern void proc_threadsDestroy(process_t *proc);


extern int proc_waitpid(int pid, int *stat, int options);


extern int proc_waittid(int pid, int options);


extern int proc_threadsList(int n, threadinfo_t *info);


extern void proc_zombie(process_t *proc);


extern int proc_threadClone(void);


extern int proc_threadSleep(unsigned int us);


extern int proc_threadWait(thread_t **queue, spinlock_t *spinlock, time_t timeout);


extern int proc_threadWakeup(thread_t **queue);


extern void proc_threadWakeupYield(thread_t **queue);


extern int proc_threadBroadcast(thread_t **queue);


extern void proc_threadBroadcastYield(thread_t **queue);


extern int threads_getCpuTime(thread_t *t);


extern thread_t *threads_findThread(int tid);


extern time_t proc_uptime(void);


extern void proc_gettime(time_t *raw, time_t *offs);


extern int proc_settime(time_t offs);


extern int proc_nextWakeup(void);


extern void proc_threadsDump(unsigned int priority);


extern int _threads_init(vm_map_t *kmap, vm_object_t *kernel);


extern int proc_sigpost(process_t *process, thread_t *thread, int sig);


extern void proc_sighandle(void *kstack);


extern int _proc_sigwant(thread_t *thread);


extern void proc_sigreturn(int s);

#endif
