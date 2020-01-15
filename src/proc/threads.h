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

#define MAX_TID ((1LL << (__CHAR_BIT__ * (sizeof(unsigned)) - 1)) - 1)

/* Parent thread states */
enum { PREFORK = 0, FORKING = 1, FORKED };

/* Child thread states */
enum { OWNSTACK = 0, PARENTSTACK };

enum { READY = 0, SLEEP, STOPPED };

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
	unsigned lgap : 1;
	unsigned rgap : 1;

	struct _process_t *process;
	struct _thread_t *procnext;
	struct _thread_t *procprev;

	int refs;
	unsigned long id;
	struct _thread_t *blocking;

	struct _thread_t **wait;
	volatile time_t wakeup;

	unsigned priority : 4;
	unsigned exit : 1;
	unsigned stop : 1;
	unsigned state : 2;
	unsigned interruptible : 1;

	unsigned sigmask;
	unsigned sigpend;

	time_t stick;
	time_t utick;

	void *kstack;
	size_t kstacksz;

	/* for vfork/exec */
	void *parentkstack, *execkstack;
	void *execdata;

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


extern void proc_threadEnd(void);


extern int proc_threadJoin(unsigned int id);


extern void proc_threadsDestroy(thread_t **threads);


extern int proc_child(process_t *child, process_t *parent);


extern int proc_zombie(process_t *zombie, process_t *parent);


extern int proc_waitpid(pid_t pid, int *status, int options);


extern int proc_join(time_t timeout);


extern int proc_threadsList(int n, threadinfo_t *info);


extern int proc_threadKill(pid_t pid, int tid, int signal);


extern int proc_threadClone(void);


extern int proc_threadSleep(unsigned long long us);


extern int proc_threadWait(thread_t **queue, spinlock_t *spinlock, time_t timeout);


extern int proc_threadWaitInterruptible(thread_t **queue, spinlock_t *spinlock, time_t timeout);


extern int proc_threadWakeup(thread_t **queue);


extern void proc_threadWakeupYield(thread_t **queue);


extern int proc_threadBroadcast(thread_t **queue);


extern void proc_threadBroadcastYield(thread_t **queue);


extern int threads_getCpuTime(thread_t *t);


extern thread_t *threads_findThread(int tid);


extern void threads_put(thread_t *);


extern time_t proc_uptime(void);


extern void proc_gettime(time_t *raw, time_t *offs);


extern int proc_settime(time_t offs);


extern time_t proc_nextWakeup(void);


extern void proc_threadsDump(unsigned int priority);


extern int _threads_init(vm_map_t *kmap, vm_object_t *kernel);


extern int threads_sigpost(process_t *process, thread_t *thread, int sig);


extern void proc_sighandle(void *kstack);


extern int _proc_sigwant(thread_t *thread);


extern void proc_sigreturn(int s);

#endif
