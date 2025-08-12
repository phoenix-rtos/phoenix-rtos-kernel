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

#ifndef _PH_PROC_THREADS_H_
#define _PH_PROC_THREADS_H_

#include "hal/hal.h"
#include "lib/lib.h"
#include "include/sysinfo.h"
#include "process.h"
#include "lock.h"

#define MAX_TID        MAX_ID
#define THREAD_END     1U
#define THREAD_END_NOW 2U

/* Thread states */
#define READY 0U
#define SLEEP 1U
#define GHOST 2U

typedef struct _thread_t {
	struct _thread_t *next;
	struct _thread_t *prev;
	struct _lock_t *locks;

	rbnode_t sleeplinkage;
	idnode_t idlinkage;

	struct _process_t *process;
	struct _thread_t *procnext;
	struct _thread_t *procprev;

	int refs;
	struct _thread_t *blocking;

	struct _thread_t **wait;
	time_t wakeup;

	unsigned int priorityBase : 4;
	unsigned int priority : 4;
	unsigned int state : 2;
	unsigned int exit : 2;
	unsigned interruptible : 1;

	unsigned int sigmask;
	unsigned int sigpend;

	time_t stick;
	time_t utick;

	void *kstack;
	size_t kstacksz;
	char *ustack;

	hal_tls_t tls;

	/* for vfork/exec */
	void *parentkstack, *execkstack;
	void *execdata;

	time_t readyTime;
	time_t maxWait;

	time_t startTime;
	time_t cpuTime;
	time_t lastTime;

	cpu_context_t *context;
	cpu_context_t *longjmpctx;
} thread_t;


static inline int proc_getTid(thread_t *t)
{
	return t->idlinkage.id;
}


int perf_start(unsigned int pid);


int perf_read(void *buffer, size_t bufsz);


int perf_finish(void);


void perf_fork(process_t *p);


void perf_kill(process_t *p);


void perf_exec(process_t *p, char *path);


thread_t *proc_current(void);


void threads_canaryInit(thread_t *t, void *ustack);


int proc_threadCreate(process_t *process, startFn_t start, int *id, u8 priority, size_t kstacksz, void *stack, size_t stacksz, void *arg);


int proc_threadPriority(int signedPriority);


__attribute__((noreturn)) void proc_threadEnd(void);


void proc_threadDestroy(thread_t *t);


void proc_threadsDestroy(thread_t **threads, const thread_t *except);


int proc_join(int tid, time_t timeout);


void proc_changeMap(process_t *proc, vm_map_t *map, vm_map_t *imap, pmap_t *pmap);


int proc_threadsList(int n, threadinfo_t *info);


int proc_threadsOther(thread_t *t);


int proc_threadSleep(time_t us);


int proc_threadNanoSleep(time_t *sec, long int *nsec, int absolute);


int proc_threadWait(thread_t **queue, spinlock_t *spinlock, time_t timeout, spinlock_ctx_t *scp);


int proc_threadWaitInterruptible(thread_t **queue, spinlock_t *spinlock, time_t timeout, spinlock_ctx_t *scp);


int proc_threadWakeup(thread_t **queue);


void proc_threadWakeupYield(thread_t **queue);


int proc_threadBroadcast(thread_t **queue);


void proc_threadBroadcastYield(thread_t **queue);


thread_t *threads_findThread(int tid);


void threads_put(thread_t *thread);


time_t proc_uptime(void);


void proc_gettime(time_t *raw, time_t *offs);


extern time_t threads_getCpuTime(thread_t *t);


int proc_settime(time_t offs);


void proc_longjmp(cpu_context_t *ctx);


void proc_threadsDump(u8 priority);


int _threads_init(vm_map_t *kmap, vm_object_t *kernel);


int threads_sigpost(process_t *process, thread_t *thread, int sig);


int threads_sigsuspend(unsigned int mask);


void threads_setupUserReturn(void *retval, cpu_context_t *ctx);


#endif
