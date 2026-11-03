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
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_PROC_THREADS_H_
#define _PH_PROC_THREADS_H_

#include "hal/hal.h"
#include "lib/lib.h"
#include "include/sysinfo.h"
#include "include/sched.h"
#include "process.h"
#include "lock.h"
#include "sc.h"

#ifdef DEBUG_THREADS
#define LIB_ASSERT_THREADS(condition, fmt, ...) LIB_ASSERT_ALWAYS((condition), (fmt), ##__VA_ARGS__)
#else
#define LIB_ASSERT_THREADS(condition, fmt, ...)
#endif

#define NPRIOS   16U
#define MAX_PRIO (NPRIOS - 1U) /* Maximum priority value, of the lowest criticality (prio=0 is of the HIGHEST) */

#define MAX_TID        MAX_ID
#define THREAD_END     1U
#define THREAD_END_NOW 2U

/* Thread states */
#define READY 0U
#define SLEEP 1U
#define GHOST 2U

typedef struct _thread_t {
	struct _lock_t *locks;

	rbnode_t sleeplinkage;
	idnode_t idlinkage;

	struct _process_t *process;
	struct _thread_t *procnext;
	struct _thread_t *procprev;

	/* TODO lots of pointers... maybe it'd possible to optimize this? */
	struct _thread_t *qnext;
	struct _thread_t *qprev;

	struct _thread_t *gnext; /* threads_common.ghosts linkage */
	struct _thread_t *gprev;

	int refs;
	struct _thread_t *blocking;

	struct _thread_t **wait;
	time_t wakeup;

	sched_context_t *scOwn;    /* thread's base SC */
	sched_context_t *scActive; /* SC currently being consumed (used by scheduler) */

	unsigned int priorityBase : 4;
	unsigned int priority : 4;
	unsigned int state : 2;
	unsigned int exit : 2;
	unsigned interruptible : 1;
	unsigned int onReady : 1; /* 1 iff linked in threads_common.ready[priority] via scActive. Not implied by state == READY! */

	unsigned int sigmask;
	unsigned int sigpend;

	void *kstack;
	size_t kstacksz;
	char *ustack;

	hal_tls_t tls;

	/* for vfork/exec */
	void *parentkstack, *execkstack;
	void *execdata;

	cpu_context_t *context;
	cpu_context_t *longjmpctx;
} thread_t;


static inline int proc_getTid(const thread_t *t)
{
	return t->idlinkage.id;
}


thread_t *proc_current(void);


void threads_canaryInit(thread_t *t, void *ustack);


int proc_threadCreate(process_t *process, startFn_t start, int *id, u8 priority, size_t kstacksz, void *stack, size_t stacksz, unsigned int sigmask, void *arg);


int proc_threadPriority(thread_t *t, int signedPriority);


__attribute__((noreturn)) void proc_threadEnd(void);


void proc_threadDestroy(thread_t *t);


void proc_threadsDestroy(thread_t **threads, const thread_t *except);


int proc_join(int tid, time_t timeout);


void proc_changeMap(process_t *proc, vm_map_t *map, vm_map_t *imap, pmap_t *pmap);


typedef void (*proc_threadsListCb_t)(void *arg, int i, threadinfo_t *info);


int proc_threadsIter(int n, proc_threadsListCb_t cb, void *arg);


int proc_threadsList(int n, threadinfo_t *info);


int proc_threadsOther(thread_t *t);


int proc_threadSleep(time_t us);


int proc_threadNanoSleep(time_t *sec, long int *nsec, int absolute);


int proc_threadWait(thread_t **queue, spinlock_t *spinlock, time_t timeout, spinlock_ctx_t *scp);


int proc_threadWaitInterruptible(thread_t **queue, spinlock_t *spinlock, time_t timeout, spinlock_ctx_t *scp);


int proc_threadWaitExclusive(thread_t **queue, time_t timeout);


int proc_threadWakeup(thread_t **queue);


void proc_threadWakeupYield(thread_t **queue);


int proc_threadBroadcast(thread_t **queue);


void proc_threadBroadcastYield(thread_t **queue);


int proc_schedInfo(int policy, sched_info_t *info);


int proc_schedGet(thread_t *t, sched_params_t *params);


int proc_schedSet(thread_t *t, int policy, sched_params_t *params);


thread_t *threads_findThread(int tid);


int proc_firstThreadTid(process_t *proc);


void threads_put(thread_t *thread);


time_t proc_uptime(void);


void proc_gettime(time_t *raw, time_t *offs);


int proc_settime(time_t offs);


__attribute__((noreturn)) void proc_longjmp(cpu_context_t *ctx);


void proc_threadsDump(u8 priority);


int _threads_init(vm_map_t *kmap, vm_object_t *kernel);


/* The scheduler lock */
extern spinlock_t threads_spinlock;


int threads_sigpost(process_t *process, thread_t *thread, int sig);


int threads_sigsuspend(unsigned int mask);


void threads_setupUserReturn(void *retval, cpu_context_t *ctx);


__attribute__((noreturn)) void threads_halt(void);


#endif
